// $Id$

/*
Low-res (320x240) renderer on SDL.

TODO:
- Verify the dirty checks, especially those of mode3 and mode23,
  which were different before.
- Further optimise the background render routines.
  For example incremental computation of the name pointers (both
  VRAM and dirty) and don't do a name table lookup if it's not dirty.

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

// TODO: Belongs in MSXTMS9928a because it is render independant.
inline static int calculatePattern(
	byte *patternPtr, int y, int size, int mag)
{
	// Calculate pattern.
	if (mag) y /= 2;
	int pattern = patternPtr[y] << 24;
	if (size == 16) {
		pattern |= patternPtr[y + 16] << 16;
	}
	if (mag) {
		// Double every dot.
		int doublePattern = 0;
		for (int i = 16; i--; ) {
			doublePattern <<= 2;
			if (pattern & 0x80000000) {
				doublePattern |= 3;
			}
			pattern <<= 1;
		}
		return doublePattern;
	}
	else {
		return pattern;
	}
}

template <class Pixel> SDLLoRenderer<Pixel>::SDLLoRenderer<Pixel>(
	MSXTMS9928a *vdp, SDL_Surface *screen)
{
	this->vdp = vdp;
	this->screen = screen;

	// Create canvas.
	canvas = SDL_CreateRGBSurface(
		SDL_HWSURFACE,
		screen->w,
		screen->h,
		screen->format->BitsPerPixel,
		screen->format->Rmask,
		screen->format->Gmask,
		screen->format->Bmask,
		screen->format->Amask
		);

	// Clear bitmap.
	// Note: SDL docs say created surface is "empty",
	//       but that does not guarantee zero pixel values.
	memset(canvas->pixels, 0, canvas->pitch * canvas->h);
	// All pixels are filled with zero bytes, so borders as well.
	memset(currBorderColours, 0, sizeof(currBorderColours));

	// Init line pointers array.
	for (int line = 0; line < HEIGHT; line++) {
		linePtrs[line] = (Pixel *)
			((byte *)canvas->pixels + line * canvas->pitch);
	}

	// Hide mouse cursor
	SDL_ShowCursor(0);

	// Reset the palette
	for (int i = 0; i < 16; i++) {
		XPal[i] = SDL_MapRGB(screen->format,
			MSXTMS9928a::TMS9928A_PALETTE[i * 3 + 0],
			MSXTMS9928a::TMS9928A_PALETTE[i * 3 + 1],
			MSXTMS9928a::TMS9928A_PALETTE[i * 3 + 2]);
	}

}

template <class Pixel> SDLLoRenderer<Pixel>::~SDLLoRenderer()
{
}

template <class Pixel> void SDLLoRenderer<Pixel>::toggleFullScreen()
{
	SDL_WM_ToggleFullScreen(screen);
}

template <class Pixel> void SDLLoRenderer<Pixel>::mode0(
	Pixel *pixelPtr, int line)
{
	Pixel backDropColour = XPal[vdp->getBackgroundColour()];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = vdp->tms.vMem[vdp->tms.nametbl + name];
		// TODO: is dirtyColour[charcode / 64] correct?
		//   It should be "charcode / 8" probably.
		if (vdp->dirtyName[name] || vdp->dirtyPattern[charcode]
		|| vdp->dirtyColour[charcode / 64]) {

			int colour = vdp->tms.vMem[vdp->tms.colour + charcode / 8];
			Pixel fg = colour >> 4;   fg = (fg ? XPal[fg] : backDropColour);
			Pixel bg = colour & 0x0F; bg = (bg ? XPal[bg] : backDropColour);

			int pattern = vdp->tms.vMem[vdp->tms.pattern + charcode * 8 + (line & 0x07)];
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
	Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;
	if (!vdp->anyDirtyColour) return;

	Pixel fg = XPal[vdp->getForegroundColour()];
	Pixel bg = XPal[vdp->getBackgroundColour()];

	int name = (line / 8) * 40;
	int x = 0;
	// Extended left border.
	for (; x < 8; x++) *pixelPtr++ = bg;
	// Actual display.
	while (x < 248) {
		int charcode = vdp->tms.vMem[vdp->tms.nametbl + name];
		if (vdp->dirtyName[name] || vdp->dirtyPattern[charcode]) {
			int pattern = vdp->tms.vMem[vdp->tms.pattern + charcode * 8 + (line & 7)];
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
	Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;

	Pixel backDropColour = XPal[vdp->getBackgroundColour()];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = vdp->tms.vMem[vdp->tms.nametbl+name] + (line / 64) * 256;
		int colourNr = (charcode & vdp->tms.colourmask);
		int patternNr = (charcode & vdp->tms.patternmask);
		if (vdp->dirtyName[name] || vdp->dirtyPattern[patternNr]
		|| vdp->dirtyColour[colourNr]) {
			// TODO: pattern uses colourNr and colour uses patterNr...
			//      I don't get it.
			int pattern = vdp->tms.vMem[vdp->tms.pattern + colourNr * 8 + (line & 7)];
			int colour = vdp->tms.vMem[vdp->tms.colour + patternNr * 8 + (line & 7)];
			Pixel fg = colour >> 4;   fg = (fg ? XPal[fg] : backDropColour);
			Pixel bg = colour & 0x0F; bg = (bg ? XPal[bg] : backDropColour);
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
	Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;
	if (!vdp->anyDirtyColour) return;

	Pixel fg = XPal[vdp->getForegroundColour()];
	Pixel bg = XPal[vdp->getBackgroundColour()];

	int name = (line / 8) * 32;
	int x = 0;
	// Extended left border.
	for ( ; x < 8; x++) *pixelPtr++ = bg;
	// Actual display.
	while (x < 248) {
		int charcode = (vdp->tms.vMem[vdp->tms.nametbl + name] + (line / 64) * 256) & vdp->tms.patternmask;
		if (vdp->dirtyName[name] || vdp->dirtyPattern[charcode]) {
			int pattern = vdp->tms.vMem[vdp->tms.pattern + charcode * 8];
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
	Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;
	if (!vdp->anyDirtyColour) return;

	Pixel backDropColour = XPal[vdp->getBackgroundColour()];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = vdp->tms.vMem[vdp->tms.nametbl + name];
		if (vdp->dirtyName[name] || vdp->dirtyPattern[charcode]) {
			int colour = vdp->tms.vMem[vdp->tms.pattern + charcode * 8 + ((line / 4) & 7)];
			Pixel fg = colour >> 4;   fg = (fg ? XPal[fg] : backDropColour);
			Pixel bg = colour & 0x0F; bg = (bg ? XPal[bg] : backDropColour);
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
	Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;

	Pixel fg = XPal[vdp->getForegroundColour()];
	Pixel bg = XPal[vdp->getBackgroundColour()];
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
	Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;
	if (!vdp->anyDirtyColour) return;

	Pixel backDropColour = XPal[vdp->getBackgroundColour()];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = vdp->tms.vMem[vdp->tms.nametbl + name];
		if (vdp->dirtyName[name] || vdp->dirtyPattern[charcode]) {
			int colour = vdp->tms.vMem[vdp->tms.pattern + ((charcode + ((line / 4) & 7) +
				(line / 64) * 256) & vdp->tms.patternmask) * 8];
			Pixel fg = colour >> 4;   fg = (fg ? XPal[fg] : backDropColour);
			Pixel bg = colour & 0x0F; bg = (bg ? XPal[bg] : backDropColour);
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

/** TODO: Is blanking really a mode?
  */
template <class Pixel> void SDLLoRenderer<Pixel>::modeblank(
	Pixel *pixelPtr, int line)
{
	// Screen blanked so all background colour.
	Pixel colour = XPal[vdp->getBackgroundColour()];
	for (int x = 0; x < 256; x++) {
		*pixelPtr++ = colour;
	}
}

template <class Pixel> bool SDLLoRenderer<Pixel>::drawSprites(
	Pixel *pixelPtr, int line, bool *dirty)
{
	int size = vdp->getSpriteSize();

	// Determine sprites visible on this line.
	int visibleSprites[32];
	int visibleIndex = vdp->checkSprites(line, visibleSprites);

	bool ret = false;
	while (visibleIndex--) {
		// Get sprite info.
		byte *attributePtr = vdp->tms.vMem + vdp->tms.spriteattribute +
			visibleSprites[visibleIndex] * 4;
		int y = *attributePtr++;
		y = (y > 208 ? y - 255 : y + 1);
		int x = *attributePtr++;
		byte *patternPtr = vdp->tms.vMem + vdp->tms.spritepattern +
			((size == 16) ? *attributePtr & 0xFC : *attributePtr) * 8;
		Pixel colour = *++attributePtr;
		if (colour & 0x80) x -= 32;
		colour &= 0x0F;
		if (colour == 0) {
			// Don't draw transparent sprites.
			continue;
		}
		colour = XPal[colour];

		int pattern = calculatePattern(
			patternPtr, line - y, size, vdp->getSpriteMag());

		// Skip any dots that end up in the left border.
		if (x < 0) {
			pattern <<= -x;
			x = 0;
		}
		// Sprites are only visible in screen modes which have lines
		// of 32 8x8 chars.
		bool *dirtyPtr = dirty + (x / 8);
		if (pattern) {
			vdp->anyDirtyName = true;
			ret = true;
		}
		// Convert pattern to pixels.
		bool charDirty = false;
		while (pattern && (x < 256)) {
			// Draw pixel if sprite has a dot.
			if (pattern & 0x80000000) {
				pixelPtr[x] = colour;
				charDirty = true;
			}
			// Advancing behaviour.
			pattern <<= 1;
			x++;
			if ((x & 7) == 0) {
				if (charDirty) *dirtyPtr = true;
				charDirty = false;
				dirtyPtr++;
			}
		}
		// Semi-filled characters can be dirty as well.
		if ((x < 256) && charDirty) *dirtyPtr = true;
	}

	return ret;
}

// Note: currently not used.
template <class Pixel> inline void SDLLoRenderer<Pixel>::drawEmptyLine(
	Pixel *linePtr, Pixel colour)
{
	for (int i = WIDTH; i--; ) {
		*linePtr++ = colour;
	}
}

template <class Pixel> inline void SDLLoRenderer<Pixel>::drawBorders(
	Pixel *linePtr, Pixel colour, int displayStart, int displayStop)
{
	for (int i = displayStart; i--; ) {
		*linePtr++ = colour;
	}
	linePtr += displayStop - displayStart;
	for (int i = WIDTH - displayStop; i--; ) {
		*linePtr++ = colour;
	}
}

template <class Pixel> void SDLLoRenderer<Pixel>::fullScreenRefresh()
{
	Pixel borderColour = XPal[vdp->getBackgroundColour()];
	int displayX = (WIDTH - 256) / 2;
	int displayY = (HEIGHT - 192) / 2;
	int line = 0;
	Pixel *currBorderColoursPtr = currBorderColours;
	SDL_Rect rect;
	rect.x = 0;
	rect.w = WIDTH;
	rect.h = 1;

	// Top border.
	for (; line < displayY; line++, currBorderColoursPtr++) {
		if (*currBorderColoursPtr != borderColour) {
			rect.y = line;
			// Note: return code ignored.
			SDL_FillRect(canvas, &rect, borderColour);
			//drawEmptyLine(linePtrs[line], borderColour);
			*currBorderColoursPtr = borderColour;
		}
	}

	// Characters that contain sprites should be drawn again next frame.
	bool nextDirty[32];
	bool nextAnyDirty = false; // dirty pixel in current character row?
	bool nextAnyDirtyName = false; // dirty pixel anywhere in frame?

	// Lock surface, because we will access pixels directly.
	if (SDL_MUSTLOCK(canvas) && SDL_LockSurface(canvas)<0) return;//ExitNow=1;

	// Display area.
	RenderMethod renderMethod = modeToRenderMethod[vdp->tms.mode];
	for (int y = 0; y < 192; y++, line++, currBorderColoursPtr++) {
		if ((y & 7) == 0) {
			fillBool(nextDirty, false, 32);
			nextAnyDirty = false;
		}
		Pixel *linePtr = &linePtrs[line][displayX];
		if (vdp->displayEnabled()) {
			(this->*renderMethod)(linePtr, y);
			if (vdp->spritesEnabled()) {
				nextAnyDirty |= drawSprites(linePtr, y, nextDirty);
			}
		}
		else {
			modeblank(linePtr, y);
		}
		if ((y & 7) == 7) {
			if (nextAnyDirty) {
				memcpy(vdp->dirtyName + (y / 8) * 32, nextDirty, 32 * sizeof(bool));
				nextAnyDirtyName = true;
			}
			else {
				fillBool(vdp->dirtyName + (y / 8) * 32, false, 32);
			}
		}
		// Borders are drawn after the display area:
		// V9958 can extend the left border over the display area,
		// this is implemented using overdraw.
		// TODO: Does the extended border clip sprites as well?
		if (*currBorderColoursPtr != borderColour) {
			drawBorders(linePtrs[line], borderColour,
				displayX, WIDTH - displayX);
			*currBorderColoursPtr = borderColour;
		}
	}

	// Unlock surface.
	if (SDL_MUSTLOCK(canvas)) SDL_UnlockSurface(canvas);

	// Bottom border.
	for (; line < HEIGHT; line++, currBorderColoursPtr++) {
		if (*currBorderColoursPtr != borderColour) {
			rect.y = line;
			// Note: return code ignored.
			SDL_FillRect(canvas, &rect, borderColour);
			//drawEmptyLine(linePtrs[line], borderColour);
			*currBorderColoursPtr = borderColour;
		}
	}

	// TODO: Verify interaction between dirty flags and blanking.
	vdp->anyDirtyName = nextAnyDirtyName;
	vdp->anyDirtyColour = vdp->anyDirtyPattern = false;
	fillBool(vdp->dirtyColour, false, sizeof(vdp->dirtyColour));
	fillBool(vdp->dirtyPattern, false, sizeof(vdp->dirtyPattern));
}

template <class Pixel> void SDLLoRenderer<Pixel>::putImage()
{
	// Copy image.
	// Note: return value is ignored.
	SDL_BlitSurface(canvas, NULL, screen, NULL);

	// Update screen.
	SDL_UpdateRect(screen, 0, 0, 0, 0);
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

