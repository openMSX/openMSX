// $Id$

/*
Low-res (320x240) renderer on SDL.

TODO:
- Move to double buffering.
  Current screen line cache performs double buffering,
  but when it is changed to a display line buffer it can no longer
  serve that function.
- Verify interaction between dirty flags and blanking.
- Verify the dirty checks, especially those of mode3 and mode23,
  which were different before.
- Further optimise the background render routines.
  For example incremental computation of the name pointers (both
  VRAM and dirty).

Idea:
For bitmap modes, cache VRAM lines rather than screen lines.
Better performance when doing raster tricks or double buffering.
Probably also easier to implement when using line buffers.
*/

#include "SDLLoRenderer.hh"
#include "MSXTMS9928a.hh"
#include "config.h"

/** Fill a boolean array with a single value.
  * Optimised for byte-sized booleans,
  * but correct for every size.
  */
inline static void fillBool(bool *ptr, bool value, int nr)
{
#if SIZEOF_BOOL == 1
	memset(ptr, value, nr);
#else
	for (int i = nr; i--; ) *ptr++ = value;
#endif
}

template <class Pixel> SDLLoRenderer<Pixel>::RenderMethod
	SDLLoRenderer<Pixel>::modeToRenderMethod[] = {
		&SDLLoRenderer::mode0,
		&SDLLoRenderer::mode1,
		&SDLLoRenderer::mode2,
		&SDLLoRenderer::mode12,
		&SDLLoRenderer::mode3,
		&SDLLoRenderer::modebogus,
		&SDLLoRenderer::mode23,
		&SDLLoRenderer::modebogus
	};

template <class Pixel> SDLLoRenderer<Pixel>::SDLLoRenderer<Pixel>(
	MSXTMS9928a *vdp, SDL_Surface *screen)
{
	this->vdp = vdp;
	this->screen = screen;

	// Init cached VDP state.
	displayMode = 0;
	nameBase = 0;
	patternBase = 0;
	colourBase = 0;
	spriteAttributeBase = 0;
	spritePatternBase = 0;
	patternMask = -1;
	colourMask = -1;

	// Init dirty admin.
	setDirty(true);

	// Init render state.
	currPhase = &SDLLoRenderer::offPhase;
	currLine = 0;

	// Create display cache.
	displayCache = SDL_CreateRGBSurface(
		SDL_HWSURFACE,
		256,
		192,
		screen->format->BitsPerPixel,
		screen->format->Rmask,
		screen->format->Gmask,
		screen->format->Bmask,
		screen->format->Amask
		);
	// Init line pointers array.
	for (int line = 0; line < 192; line++) {
		linePtrs[line] = (Pixel *)
			((byte *)displayCache->pixels + line * displayCache->pitch);
	}

	// Hide mouse cursor
	SDL_ShowCursor(0);

	// Reset the palette
	for (int i = 0; i < 16; i++) {
		XPalFg[i] = XPalBg[i] = SDL_MapRGB(
			screen->format,
			MSXTMS9928a::TMS9928A_PALETTE[i * 3 + 0],
			MSXTMS9928a::TMS9928A_PALETTE[i * 3 + 1],
			MSXTMS9928a::TMS9928A_PALETTE[i * 3 + 2]
			);
	}

}

template <class Pixel> SDLLoRenderer<Pixel>::~SDLLoRenderer()
{
	// TODO: SDL_Free and such.
}

template <class Pixel> void SDLLoRenderer<Pixel>::setFullScreen(
	bool fullScreen)
{
	if (((screen->flags & SDL_FULLSCREEN) != 0) != fullScreen) {
		SDL_WM_ToggleFullScreen(screen);
	}
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateBackgroundColour(
	int colour, Emutime &time)
{
	XPalFg[0] = XPalBg[colour];
	// Any line containing transparent pixels must be repainted.
	// We don't know which lines contain transparent pixels,
	// so we have to repaint them all.
	anyDirtyColour = true;
	fillBool(dirtyColour, true, sizeof(dirtyColour));
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateDisplayMode(
	int mode, Emutime &time)
{
	displayMode = mode;
	setDirty(true);
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateNameBase(
	int addr, Emutime &time)
{
	nameBase = addr;
	anyDirtyName = true;
	fillBool(dirtyName, true, sizeof(dirtyName));
}

template <class Pixel> void SDLLoRenderer<Pixel>::updatePatternBase(
	int addr, int mask, Emutime &time)
{
	patternBase = addr;
	patternMask = mask;
	anyDirtyPattern = true;
	fillBool(dirtyPattern, true, sizeof(dirtyPattern));
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateColourBase(
	int addr, int mask, Emutime &time)
{
	colourBase = addr;
	colourMask = mask;
	anyDirtyColour = true;
	fillBool(dirtyColour, true, sizeof(dirtyColour));
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateSpriteAttributeBase(
	int addr, Emutime &time)
{
	spriteAttributeBase = addr;
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateSpritePatternBase(
	int addr, Emutime &time)
{
	spritePatternBase = addr;
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateVRAM(
	int addr, byte data, Emutime &time)
{
	int i = addr - nameBase;
	if ((i >= 0) && (i < (int)sizeof(dirtyName))) {
		dirtyName[i] = anyDirtyName = true;
	}

	i = (addr - colourBase) >> 3;
	if ((i >= 0) && (i < (int)sizeof(dirtyColour))) {
		dirtyColour[i] = anyDirtyColour = true;
	}

	i = (addr - patternBase) >> 3;
	if ((i >= 0) && (i < (int)sizeof(dirtyPattern))) {
		dirtyPattern[i] = anyDirtyPattern = true;
	}

}

template <class Pixel> void SDLLoRenderer<Pixel>::setDirty(
	bool dirty)
{
	anyDirtyColour = anyDirtyPattern = anyDirtyName = dirty;
	fillBool(dirtyName, dirty, sizeof(dirtyName));
	fillBool(dirtyColour, dirty, sizeof(dirtyColour));
	fillBool(dirtyPattern, dirty, sizeof(dirtyPattern));
}

template <class Pixel> void SDLLoRenderer<Pixel>::mode0(
	int line)
{
	Pixel *pixelPtr = linePtrs[line];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = vdp->getVRAM(nameBase + name);
		// TODO: is dirtyColour[charcode / 64] correct?
		//   It should be "charcode / 8" probably.
		if (dirtyName[name] || dirtyPattern[charcode]
		|| dirtyColour[charcode / 64]) {

			int colour = vdp->getVRAM(colourBase + charcode / 8);
			Pixel fg = XPalFg[colour >> 4];
			Pixel bg = XPalFg[colour & 0x0F];

			int pattern = vdp->getVRAM(patternBase + charcode * 8 + (line & 7));
			// TODO: Compare performance of this loop vs unrolling.
			for (int i = 8; i--; ) {
				*pixelPtr++ = ((pattern & 0x80) ? fg : bg);
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 8;
		}
		name++;
	}
}

template <class Pixel> void SDLLoRenderer<Pixel>::mode1(
	int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(anyDirtyColour || anyDirtyName || anyDirtyPattern) )
	//  return;
	if (!anyDirtyColour) return;

	Pixel *pixelPtr = linePtrs[line];
	Pixel fg = XPalFg[vdp->getForegroundColour()];
	Pixel bg = XPalBg[vdp->getBackgroundColour()];

	int x = 0;
	// Extended left border.
	for (; x < 8; x++) *pixelPtr++ = bg;
	// Actual display.
	int name = (line / 8) * 40;
	while (x < 248) {
		int charcode = vdp->getVRAM(nameBase + name);
		if (dirtyName[name] || dirtyPattern[charcode]) {
			int pattern = vdp->getVRAM(patternBase + charcode * 8 + (line & 7));
			for (int i = 6; i--; ) {
				*pixelPtr++ = ((pattern & 0x80) ? fg : bg);
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 6;
		}
		x += 6;
		name++;
	}
	// Extended right border.
	for (; x < 256; x++) *pixelPtr++ = bg;
}

template <class Pixel> void SDLLoRenderer<Pixel>::mode2(
	int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(anyDirtyColour || anyDirtyName || anyDirtyPattern) )
	//  return;

	// TODO: Something seems wrong about name, quarter and dirty.
	Pixel *pixelPtr = linePtrs[line];
	int name = (line / 8) * 32;
	int quarter = (line / 64) * 256;
	for (int x = 0; x < 256; x += 8) {
		int charCode = vdp->getVRAM(nameBase + name) + quarter;
		int colourNr = (charCode & colourMask);
		int patternNr = (charCode & patternMask);
		if (dirtyName[name] || dirtyPattern[patternNr]
		|| dirtyColour[colourNr]) {
			// TODO: pattern uses colourNr and colour uses patterNr...
			//      I don't get it.
			int pattern = vdp->getVRAM(patternBase + colourNr * 8 + (line & 7));
			int colour = vdp->getVRAM(colourBase + patternNr * 8 + (line & 7));
			Pixel fg = XPalFg[colour >> 4];
			Pixel bg = XPalFg[colour & 0x0F];
			for (int i = 8; i--; ) {
				*pixelPtr++ = ((pattern & 0x80) ? fg : bg);
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 8;
		}
		name++;
	}
}

template <class Pixel> void SDLLoRenderer<Pixel>::mode12(
	int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(anyDirtyColour || anyDirtyName || anyDirtyPattern) )
	//  return;
	if (!anyDirtyColour) return;

	Pixel *pixelPtr = linePtrs[line];
	Pixel fg = XPalFg[vdp->getForegroundColour()];
	Pixel bg = XPalBg[vdp->getBackgroundColour()];

	int x = 0;
	// Extended left border.
	for ( ; x < 8; x++) *pixelPtr++ = bg;
	// Actual display.
	int name = (line / 8) * 32;
	while (x < 248) {
		int charcode = (vdp->getVRAM(nameBase + name) + (line / 64) * 256) & patternMask;
		if (dirtyName[name] || dirtyPattern[charcode]) {
			int pattern = vdp->getVRAM(patternBase + charcode * 8);
			for (int i = 6; i--; ) {
				*pixelPtr++ = ((pattern & 0x80) ? fg : bg);
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 6;
		}
		x += 6;
		name++;
	}
	// Extended right border.
	for ( ; x < 256; x++) *pixelPtr++ = bg;
}

template <class Pixel> void SDLLoRenderer<Pixel>::mode3(
	int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(anyDirtyColour || anyDirtyName || anyDirtyPattern) )
	//  return;
	if (!anyDirtyColour) return;

	Pixel *pixelPtr = linePtrs[line];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = vdp->getVRAM(nameBase + name);
		if (dirtyName[name] || dirtyPattern[charcode]) {
			int colour = vdp->getVRAM(patternBase + charcode * 8 + ((line / 4) & 7));
			Pixel fg = XPalFg[colour >> 4];
			Pixel bg = XPalFg[colour & 0x0F];
			int n;
			for (n = 4; n--; ) *pixelPtr++ = fg;
			for (n = 4; n--; ) *pixelPtr++ = bg;
		}
		else {
			pixelPtr += 8;
		}
		name++;
	}
}

template <class Pixel> void SDLLoRenderer<Pixel>::modebogus(
	int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(anyDirtyColour || anyDirtyName || anyDirtyPattern) )
	//  return;

	Pixel *pixelPtr = linePtrs[line];
	Pixel fg = XPalFg[vdp->getForegroundColour()];
	Pixel bg = XPalBg[vdp->getBackgroundColour()];
	int x = 0;
	for (; x < 8; x++) *pixelPtr++ = bg;
	for (; x < 248; x += 6) {
		int n;
		for (n = 4; n--; ) *pixelPtr++ = fg;
		for (n = 2; n--; ) *pixelPtr++ = bg;
	}
	for (; x < 256; x++) *pixelPtr++ = bg;
}

template <class Pixel> void SDLLoRenderer<Pixel>::mode23(
	int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(anyDirtyColour || anyDirtyName || anyDirtyPattern) )
	//  return;
	if (!anyDirtyColour) return;

	Pixel *pixelPtr = linePtrs[line];
	int name = (line / 8) * 32;
	int quarter = (line / 64) * 256;
	for (int x = 0; x < 256; x += 8) {
		int charCode = vdp->getVRAM(nameBase + name);
		if (dirtyName[name] || dirtyPattern[charCode]) {
			int colour = vdp->getVRAM(patternBase +
				((charCode + ((line / 4) & 7) + quarter) & patternMask) * 8);
			Pixel fg = XPalFg[colour >> 4];
			Pixel bg = XPalFg[colour & 0x0F];
			int n;
			for (n = 4; n--; ) *pixelPtr++ = fg;
			for (n = 4; n--; ) *pixelPtr++ = bg;
		}
		else {
			pixelPtr += 8;
		}
		name++;
	}
}

template <class Pixel> void SDLLoRenderer<Pixel>::drawSprites(
	Pixel *pixelPtr, int line)
{
	// Determine sprites visible on this line.
	MSXTMS9928a::SpriteInfo visibleSprites[32];
	int visibleIndex = vdp->checkSprites(line, visibleSprites);

	while (visibleIndex--) {
		// Get sprite info.
		MSXTMS9928a::SpriteInfo *sip = &visibleSprites[visibleIndex];
		int pattern = sip->pattern;
		int x = sip->x;
		Pixel colour = sip->colour;
		if (colour == 0) {
			// Don't draw transparent sprites.
			continue;
		}
		colour = XPalBg[colour];
		// Skip any dots that end up in the left border.
		if (x < 0) {
			pattern <<= -x;
			x = 0;
		}
		// Convert pattern to pixels.
		while (pattern && (x < 256)) {
			// Draw pixel if sprite has a dot.
			if (pattern & 0x80000000) {
				pixelPtr[x] = colour;
			}
			// Advancing behaviour.
			pattern <<= 1;
			x++;
		}
	}

}

/*
NTSC phase diagram

line:  phase:           handler:
   0   bottom erase     offPhase
   3   sync             offPhase
   6   top erase        offPhase
  19   top border       blankPhase
  45   display          displayPhase
 237   bottom border    blankPhase
 262   end of screen
*/
static const int LINE_TOP_BORDER = 19;
static const int LINE_DISPLAY = 45;
static const int LINE_BOTTOM_BORDER = 237;
static const int LINE_END_OF_SCREEN = 262;
/*
Lines rendered are [21..261).
*/
static const int LINE_RENDER_TOP = 21;

template <class Pixel> void SDLLoRenderer<Pixel>::offPhase(
	int limit)
{
	if (limit > LINE_TOP_BORDER) {
		// Top border ends off phase.
		currPhase = &SDLLoRenderer::blankPhase;
		limit = LINE_TOP_BORDER;
	}
	// No rendering, only catch up.
	if (currLine < limit) currLine = limit;
}

template <class Pixel> void SDLLoRenderer<Pixel>::blankPhase(
	int limit)
{
	if ((currLine < LINE_BOTTOM_BORDER) && (limit > LINE_DISPLAY)
	&& vdp->displayEnabled()) {
		// Display ends blank phase.
		currPhase = &SDLLoRenderer::displayPhase;
		limit = LINE_DISPLAY;
	}
	else if (limit == LINE_END_OF_SCREEN) {
		// End of screen ends blank phase.
		currPhase = &SDLLoRenderer::offPhase;
	}
	// Render lines up to limit.
	if (currLine < limit) {
		// TODO: Only redraw if necessary.
		SDL_Rect rect;
		rect.x = 0;
		rect.y = currLine - LINE_RENDER_TOP;
		rect.w = WIDTH;
		rect.h = limit - currLine;
		// Clip to area actually displayed.
		if (rect.y < 0) {
			rect.h += rect.y;
			rect.y = 0;
		}
		else if (rect.y + rect.h > HEIGHT) {
			rect.h = HEIGHT - rect.y;
		}
		if (rect.h > 0) {
			// Note: return code ignored.
			SDL_FillRect(screen, &rect, XPalBg[vdp->getBackgroundColour()]);
		}
		currLine = limit;
	}
}

template <class Pixel> void SDLLoRenderer<Pixel>::displayPhase(
	int limit)
{
	if (!vdp->displayEnabled()) {
		// Forced blanking ends display phase.
		currPhase = &SDLLoRenderer::blankPhase;
		return;
	}
	if (limit > LINE_BOTTOM_BORDER) {
		// Bottom border ends display phase.
		currPhase = &SDLLoRenderer::blankPhase;
		limit = LINE_BOTTOM_BORDER;
	}
	if (currLine >= limit) return;

	// Lock surface, because we will access pixels directly.
	if (SDL_MUSTLOCK(displayCache) && SDL_LockSurface(displayCache) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}
	// Render background lines.
	RenderMethod renderMethod = modeToRenderMethod[displayMode];
	for (int line = currLine; line < limit; line++) {
		(this->*renderMethod)(line - LINE_DISPLAY);
	}
	// Unlock surface.
	if (SDL_MUSTLOCK(displayCache)) SDL_UnlockSurface(displayCache);

	// Where does the display start? (equal to width of left border)
	int displayX = (WIDTH - 256) / 2;

	// Copy background image.
	SDL_Rect source;
	source.x = 0;
	source.y = currLine - LINE_DISPLAY;
	source.w = 256;
	source.h = limit - currLine;
	SDL_Rect dest;
	dest.x = displayX;
	dest.y = currLine - LINE_RENDER_TOP;
	// TODO: Can we safely use SDL_LowerBlit?
	// Note: return value is ignored.
	SDL_BlitSurface(displayCache, &source, screen, &dest);

	// Render sprites.
	// TODO: Draw sprites only in modes that actually have sprites.
	if (vdp->spritesEnabled()) {
		// Lock surface, because we will access pixels directly.
		// TODO: Locking the surface directly after a blit is
		//   probably not the smartest thing to do performance-wise.
		//   Since sprite data will buffered, why not plot them
		//   just before page flip?
		//   Will only work if *all* data required is buffered, including
		//   for example RGB colour (on V9938 palette may change).
		if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) {
			// Display will be wrong, but this is not really critical.
			return;
		}
		for (int line = currLine; line < limit; line++) {
			// TODO: Cache this, like linePtrs?
			Pixel *pixelPtr = (Pixel *)(
				  (byte *)screen->pixels
				+ (line - LINE_RENDER_TOP) * screen->pitch
				+ displayX * sizeof(Pixel)
				);
			drawSprites(pixelPtr, line - LINE_DISPLAY);
		}
		// Unlock surface.
		if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	}

	// Borders are drawn after the display area:
	// V9958 can extend the left border over the display area,
	// this is implemented using overdraw.
	// TODO: Does the extended border clip sprites as well?
	Pixel bgColour = XPalBg[vdp->getBackgroundColour()];
	dest.x = 0;
	dest.w = displayX;
	SDL_FillRect(screen, &dest, bgColour);
	dest.x = WIDTH - displayX;
	dest.w = displayX;
	SDL_FillRect(screen, &dest, bgColour);

	currLine = limit;
}

template <class Pixel> void SDLLoRenderer<Pixel>::renderUntil(
	int limit)
{
	while (currLine < limit) {
		(this->*currPhase)(limit);
	}
}

template <class Pixel> void SDLLoRenderer<Pixel>::putImage()
{
	// Render changes from this last frame.
	// TODO: Support PAL.
	renderUntil(262);
	currLine = 0;

	// Update screen.
	// TODO: Move to double buffering?
	SDL_UpdateRect(screen, 0, 0, 0, 0);

	// Screen is up-to-date, so nothing is dirty.
	setDirty(false);
}

Renderer *createSDLLoRenderer(MSXTMS9928a *vdp, bool fullScreen)
{
	int width = SDLLoRenderer<Uint32>::WIDTH;
	int height = SDLLoRenderer<Uint32>::HEIGHT;
	int flags = SDL_HWSURFACE | (fullScreen ? SDL_FULLSCREEN : 0);

	// Try default bpp.
	PRT_DEBUG("OK\n  Opening display... ");
	SDL_Surface *screen = SDL_SetVideoMode(width, height, 0, flags);

	// If no screen or unsupported screen,
	// try supported bpp in order of preference.
	int bytepp = (screen ? screen->format->BytesPerPixel : 0);
	if ((bytepp != 1) && (bytepp != 2) && (bytepp != 4)) {
		if (!screen) screen = SDL_SetVideoMode(width, height, 15, flags);
		if (!screen) screen = SDL_SetVideoMode(width, height, 16, flags);
		if (!screen) screen = SDL_SetVideoMode(width, height, 32, flags);
		if (!screen) screen = SDL_SetVideoMode(width, height, 8, flags);
	}

	if (!screen) {
		printf("FAILED to open any screen!");
		// TODO: Throw exception.
		return NULL;
	}
	PRT_DEBUG("Display is " << (int)(screen->format->BitsPerPixel) << " bpp.");

	switch (screen->format->BytesPerPixel) {
	case 1:
		return new SDLLoRenderer<Uint8>(vdp, screen);
	case 2:
		return new SDLLoRenderer<Uint16>(vdp, screen);
	case 4:
		return new SDLLoRenderer<Uint32>(vdp, screen);
	default:
		printf("FAILED to open supported screen!");
		// TODO: Throw exception.
		return NULL;
	}

}

