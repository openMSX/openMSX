// $Id$

/*
TODO:
- Implement blinking (of page mask) in bitmap modes.
- Implement sprite pixels in Graphic 5.
- Move dirty checking to VDPVRAM.
- Spot references to EmuTime: if implemented correctly this class should
  not need EmuTime, since it uses absolute VDP coordinates instead.
*/

#include "SDLHiRenderer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "RealTime.hh"
#include <math.h>
#include "SDLConsole.hh"
#include "RenderSettings.hh"


/** Dimensions of screen.
  */
static const int WIDTH = 640;
static const int HEIGHT = 480;

/** VDP ticks between start of line and start of left border.
  */
static const int TICKS_LEFT_BORDER = 100 + 102;

/** The middle of the visible (display + borders) part of a line,
  * expressed in VDP ticks since the start of the line.
  * TODO: Move this to PixelRenderer?
  *       It is not used there, but it is used by multiple subclasses.
  */
static const int TICKS_VISIBLE_MIDDLE =
	TICKS_LEFT_BORDER + (VDP::TICKS_PER_LINE - TICKS_LEFT_BORDER - 27) / 2;

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

/** Translate from absolute VDP coordinates to screen coordinates:
  * Note: In reality, there are only 569.5 visible pixels on a line.
  *       Because it looks better, the borders are extended to 640.
  */
inline static int translateX(int absoluteX)
{
	if (absoluteX == VDP::TICKS_PER_LINE) return WIDTH;
	// Note: The "& ~1" forces the ticks to a pixel (2-tick) boundary.
	//       If this is not done, rounding errors will occur.
	int screenX = (absoluteX - (TICKS_VISIBLE_MIDDLE & ~1)) / 2 + WIDTH / 2;
	return screenX < 0 ? 0 : screenX;
}

template <class Pixel> void SDLHiRenderer<Pixel>::finishFrame()
{
	// Render console if needed.
	console->drawConsole();

	// Update screen.
	SDL_UpdateRect(screen, 0, 0, 0, 0);
}

template <class Pixel> inline Pixel *SDLHiRenderer<Pixel>::getLinePtr(
	SDL_Surface *displayCache, int line)
{
	return (Pixel *)( (byte *)displayCache->pixels
		+ line * displayCache->pitch );
}

// TODO: Cache this?
template <class Pixel> inline Pixel SDLHiRenderer<Pixel>::getBorderColour()
{
	// TODO: Used knowledge of V9938 to merge two 4-bit colours
	//       into a single 8 bit colour for SCREEN8.
	//       Keep doing that or make VDP handle SCREEN8 differently?
	return
		( vdp->getDisplayMode() == 0x1C
		? PALETTE256[
			vdp->getBackgroundColour() | (vdp->getForegroundColour() << 4)]
		: palBg[ vdp->getDisplayMode() == 0x10
		       ? vdp->getBackgroundColour() & 3
		       : vdp->getBackgroundColour()
		       ]
		);
}

template <class Pixel> inline void SDLHiRenderer<Pixel>::renderBitmapLines(
	byte line, int count)
{
	// Lock surface, because we will access pixels directly.
	if (SDL_MUSTLOCK(bitmapDisplayCache) && SDL_LockSurface(bitmapDisplayCache) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}

	int mode = vdp->getDisplayMode();
	// Which bits in the name mask determine the page?
	int pageMask = 0x200 | vdp->getEvenOddMask();
	while (count--) {
		// TODO: Optimise addr and line; too many connversions right now.
		int vramLine = (vram->nameTable.getMask() >> 7) & (pageMask | line);
		if (lineValidInMode[vramLine] != mode) {
			const byte *vramPtr = vram->bitmapWindow.readArea(vramLine << 7);
			bitmapConverter.convertLine(
				getLinePtr(bitmapDisplayCache, vramLine), vramPtr );
			lineValidInMode[vramLine] = mode;
		}
		line++; // is a byte, so wraps at 256
	}

	// Unlock surface.
	if (SDL_MUSTLOCK(bitmapDisplayCache)) SDL_UnlockSurface(bitmapDisplayCache);
}

template <class Pixel> inline void SDLHiRenderer<Pixel>::renderPlanarBitmapLines(
	byte line, int count)
{
	// Lock surface, because we will access pixels directly.
	if (SDL_MUSTLOCK(bitmapDisplayCache) && SDL_LockSurface(bitmapDisplayCache) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}

	int mode = vdp->getDisplayMode();
	// Which bits in the name mask determine the page?
	int pageMask = vdp->getEvenOddMask();
	while (count--) {
		// TODO: Optimise addr and line; too many connversions right now.
		int vramLine = (vram->nameTable.getMask() >> 7) & (pageMask | line);
		if ( lineValidInMode[vramLine] != mode
		|| lineValidInMode[vramLine | 512] != mode ) {
			int addr0 = vramLine << 7;
			int addr1 = addr0 | 0x10000;
			const byte *vramPtr0 = vram->bitmapWindow.readArea(addr0);
			const byte *vramPtr1 = vram->bitmapWindow.readArea(addr1);
			bitmapConverter.convertLinePlanar(
				getLinePtr(bitmapDisplayCache, vramLine),
				vramPtr0, vramPtr1 );
			lineValidInMode[vramLine] =
				lineValidInMode[vramLine | 512] = mode;
		}
		line++; // is a byte, so wraps at 256
	}

	// Unlock surface.
	if (SDL_MUSTLOCK(bitmapDisplayCache)) SDL_UnlockSurface(bitmapDisplayCache);
}

template <class Pixel> inline void SDLHiRenderer<Pixel>::renderCharacterLines(
	byte line, int count)
{
	// Lock surface, because we will access pixels directly.
	if (SDL_MUSTLOCK(charDisplayCache) && SDL_LockSurface(charDisplayCache) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}
	
	while (count--) {
		// Render this line.
		characterConverter.convertLine(
			getLinePtr(charDisplayCache, line), line);
		line++; // is a byte, so wraps at 256
	}
	
	// Unlock surface.
	if (SDL_MUSTLOCK(charDisplayCache)) SDL_UnlockSurface(charDisplayCache);
}

template <class Pixel> SDLHiRenderer<Pixel>::DirtyChecker
	// Use checkDirtyBitmap for every mode for which isBitmapMode is true.
	SDLHiRenderer<Pixel>::modeToDirtyChecker[] = {
		// M5 M4 = 0 0  (MSX1 modes)
		&SDLHiRenderer::checkDirtyMSX1, // Graphic 1
		&SDLHiRenderer::checkDirtyMSX1, // Text 1
		&SDLHiRenderer::checkDirtyMSX1, // Multicolour
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyMSX1, // Graphic 2
		&SDLHiRenderer::checkDirtyMSX1, // Text 1 Q
		&SDLHiRenderer::checkDirtyMSX1, // Multicolour Q
		&SDLHiRenderer::checkDirtyNull,
		// M5 M4 = 0 1
		&SDLHiRenderer::checkDirtyMSX1, // Graphic 3
		&SDLHiRenderer::checkDirtyText2,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyBitmap, // Graphic 4
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap,
		// M5 M4 = 1 0
		&SDLHiRenderer::checkDirtyBitmap, // Graphic 5
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap, // Graphic 6
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap,
		// M5 M4 = 1 1
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap, // Graphic 7
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap,
		&SDLHiRenderer::checkDirtyBitmap
	};

template <class Pixel> SDLHiRenderer<Pixel>::SDLHiRenderer<Pixel>(
	VDP *vdp, SDL_Surface *screen, bool fullScreen, const EmuTime &time)
	: PixelRenderer(vdp, fullScreen, time)
	, characterConverter(vdp, palFg, palBg)
	, bitmapConverter(palFg, PALETTE256)
{
	console = new SDLConsole(screen);

	this->vdp = vdp;
	this->screen = screen;
	vram = vdp->getVRAM();
	spriteChecker = vdp->getSpriteChecker();
	// TODO: Store current time.
	//       Does the renderer actually have to keep time?
	//       Keeping render position should be good enough.

	// Create display caches.
	charDisplayCache = SDL_CreateRGBSurface(
		SDL_HWSURFACE,
		512,
		vdp->isMSX1VDP() ? 192 : 256,
		screen->format->BitsPerPixel,
		screen->format->Rmask,
		screen->format->Gmask,
		screen->format->Bmask,
		screen->format->Amask
		);
	bitmapDisplayCache = ( vdp->isMSX1VDP()
		? NULL
		: SDL_CreateRGBSurface(
			SDL_HWSURFACE,
			512,
			256 * 4,
			screen->format->BitsPerPixel,
			screen->format->Rmask,
			screen->format->Gmask,
			screen->format->Bmask,
			screen->format->Amask
			)
		);

	// Hide mouse cursor.
	SDL_ShowCursor(SDL_DISABLE);

	// Init the palette.
	if (vdp->isMSX1VDP()) {
		// Fixed palette.
		for (int i = 0; i < 16; i++) {
			palFg[i] = palBg[i] = SDL_MapRGB(
				screen->format,
				TMS99X8A_PALETTE[i][0],
				TMS99X8A_PALETTE[i][1],
				TMS99X8A_PALETTE[i][2]
				);
		}
	}
	else {
		// Precalculate SDL palette for V9938 colours.
		for (int r = 0; r < 8; r++) {
			for (int g = 0; g < 8; g++) {
				for (int b = 0; b < 8; b++) {
					const float gamma = 2.2 / 2.8;
					V9938_COLOURS[r][g][b] = SDL_MapRGB(
						screen->format,
						(int)(pow((float)r / 7.0, gamma) * 255),
						(int)(pow((float)g / 7.0, gamma) * 255),
						(int)(pow((float)b / 7.0, gamma) * 255)
						);
				}
			}
		}
		// Precalculate Graphic 7 bitmap palette.
		for (int i = 0; i < 256; i++) {
			PALETTE256[i] = V9938_COLOURS
				[(i & 0x1C) >> 2]
				[(i & 0xE0) >> 5]
				[((i & 0x03) << 1) | ((i & 0x02) >> 1)];
		}
		// Precalculate Graphic 7 sprite palette.
		for (int i = 0; i < 16; i++) {
			word grb = GRAPHIC7_SPRITE_PALETTE[i];
			palGraphic7Sprites[i] =
				V9938_COLOURS[(grb >> 4) & 7][grb >> 8][grb & 7];
		}
	}
	reset(time);
}

template <class Pixel> SDLHiRenderer<Pixel>::~SDLHiRenderer()
{
	delete console;
	SDL_FreeSurface(charDisplayCache);
	SDL_FreeSurface(bitmapDisplayCache);
}

template <class Pixel> void SDLHiRenderer<Pixel>::reset(const EmuTime &time)
{
	PixelRenderer::reset(time);
	
	// Init renderer state.
	dirtyChecker = modeToDirtyChecker[vdp->getDisplayMode()];
	if (vdp->isBitmapMode()) {
		bitmapConverter.setDisplayMode(vdp->getDisplayMode());
	} else {
		characterConverter.setDisplayMode(vdp->getDisplayMode());
	}
	
	palSprites = palBg;
	setDirty(true);
	dirtyForeground = dirtyBackground = true;
	
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));

	if (!vdp->isMSX1VDP()) {
		// Reset the palette.
		for (int i = 0; i < 16; i++) {
			updatePalette(i, vdp->getPalette(i), time);
		}
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::frameStart(
	const EmuTime &time)
{
	// Call superclass implementation.
	PixelRenderer::frameStart(time);

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	lineRenderTop = vdp->isPalTiming() ? 59 - 14 : 32 - 14;
}

template <class Pixel> void SDLHiRenderer<Pixel>::setFullScreen(
	bool fullScreen)
{
	Renderer::setFullScreen(fullScreen);
	if (((screen->flags & SDL_FULLSCREEN) != 0) != fullScreen) {
		SDL_WM_ToggleFullScreen(screen);
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateTransparency(
	bool enabled, const EmuTime &time)
{
	sync(time);
	// Set the right palette for pixels of colour 0.
	palFg[0] = palBg[enabled ? vdp->getBackgroundColour() : 0];
	// Any line containing pixels of colour 0 must be repainted.
	// We don't know which lines contain such pixels,
	// so we have to repaint them all.
	anyDirtyColour = true;
	fillBool(dirtyColour, true, sizeof(dirtyColour) / sizeof(bool));
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateForegroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	dirtyForeground = true;
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateBackgroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	dirtyBackground = true;
	if (vdp->getTransparency()) {
		// Transparent pixels have background colour.
		palFg[0] = palBg[colour];
		// Any line containing pixels of colour 0 must be repainted.
		// We don't know which lines contain such pixels,
		// so we have to repaint them all.
		anyDirtyColour = true;
		fillBool(dirtyColour, true, sizeof(dirtyColour) / sizeof(bool));
		memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateBlinkForegroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	dirtyForeground = true;
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateBlinkBackgroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	dirtyBackground = true;
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateBlinkState(
	bool enabled, const EmuTime &time)
{
	// TODO: When the sync call is enabled, the screen flashes on
	//       every call to this method.
	//       I don't know why exactly, but it's probably related to
	//       being called at frame start.
	//sync(time);
	if (vdp->getDisplayMode() == 0x09) {
		// Text2 with blinking text.
		// Consider all characters dirty.
		// TODO: Only mark characters in blink colour dirty.
		anyDirtyName = true;
		fillBool(dirtyName, true, sizeof(dirtyName) / sizeof(bool));
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::updatePalette(
	int index, int grb, const EmuTime &time)
{
	sync(time);

	// Update SDL colours in palette.
	palFg[index] = palBg[index] =
		V9938_COLOURS[(grb >> 4) & 7][grb >> 8][grb & 7];

	// Is this the background colour?
	if (vdp->getBackgroundColour() == index && vdp->getTransparency()) {
		dirtyBackground = true;
		// Transparent pixels have background colour.
		palFg[0] = palBg[vdp->getBackgroundColour()];
	}

	// Any line containing pixels of this colour must be repainted.
	// We don't know which lines contain which colours,
	// so we have to repaint them all.
	anyDirtyColour = true;
	fillBool(dirtyColour, true, sizeof(dirtyColour) / sizeof(bool));
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateVerticalScroll(
	int scroll, const EmuTime &time)
{
	sync(time);
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateHorizontalAdjust(
	int adjust, const EmuTime &time)
{
	sync(time);
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateDisplayMode(
	int mode, const EmuTime &time)
{
	sync(time);
	dirtyChecker = modeToDirtyChecker[mode];
	if (vdp->isBitmapMode(mode)) {
		bitmapConverter.setDisplayMode(mode);
	} else {
		characterConverter.setDisplayMode(mode);
	}
	palSprites = (mode == 0x1C ? palGraphic7Sprites : palBg);
	setDirty(true);
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateNameBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyName = true;
	fillBool(dirtyName, true, sizeof(dirtyName) / sizeof(bool));
}

template <class Pixel> void SDLHiRenderer<Pixel>::updatePatternBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyPattern = true;
	fillBool(dirtyPattern, true, sizeof(dirtyPattern) / sizeof(bool));
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateColourBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyColour = true;
	fillBool(dirtyColour, true, sizeof(dirtyColour) / sizeof(bool));
}

template <class Pixel> void SDLHiRenderer<Pixel>::checkDirtyNull(
	int addr, byte data)
{
	// Do nothing: this is a bogus mode whose display doesn't depend
	// on VRAM contents.
}

template <class Pixel> void SDLHiRenderer<Pixel>::checkDirtyMSX1(
	int addr, byte data)
{
	if (vram->nameTable.isInside(addr)) {
		dirtyName[addr & ~(-1 << 10)] = anyDirtyName = true;
	}
	if (vram->colourTable.isInside(addr)) {
		dirtyColour[(addr / 8) & ~(-1 << 10)] = anyDirtyColour = true;
	}
	if (vram->patternTable.isInside(addr)) {
		dirtyPattern[(addr / 8) & ~(-1 << 10)] = anyDirtyPattern = true;
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::checkDirtyText2(
	int addr, byte data)
{
	if (vram->nameTable.isInside(addr)) {
		dirtyName[addr & ~(-1 << 12)] = anyDirtyName = true;
	}
	if (vram->patternTable.isInside(addr)) {
		dirtyPattern[(addr / 8) & ~(-1 << 8)] = anyDirtyPattern = true;
	}
	// TODO: Mask and index overlap in Text2, so it is possible for multiple
	//       addresses to be mapped to a single byte in the colour table.
	//       Therefore the current implementation is incorrect and a different
	//       approach is needed.
	//       The obvious solutions is to mark entries as dirty in the colour
	//       table, instead of the name table.
	//       The check code here was updated, the rendering code not yet.
	/*
	int colourBase = vdp->getColourMask() & (-1 << 9);
	int i = addr - colourBase;
	if ((0 <= i) && (i < 2160/8)) {
		dirtyName[i*8+0] = dirtyName[i*8+1] =
		dirtyName[i*8+2] = dirtyName[i*8+3] =
		dirtyName[i*8+4] = dirtyName[i*8+5] =
		dirtyName[i*8+6] = dirtyName[i*8+7] =
		anyDirtyName = true;
	}
	*/
	if (vram->colourTable.isInside(addr)) {
		dirtyColour[addr & ~(-1 << 9)] = anyDirtyColour = true;
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::checkDirtyBitmap(
	int addr, byte data)
{
	lineValidInMode[addr >> 7] = 0xFF;
}

template <class Pixel> void SDLHiRenderer<Pixel>::setDirty(
	bool dirty)
{
	anyDirtyColour = anyDirtyPattern = anyDirtyName = dirty;
	fillBool(dirtyName, dirty, sizeof(dirtyName) / sizeof(bool));
	fillBool(dirtyColour, dirty, sizeof(dirtyColour) / sizeof(bool));
	fillBool(dirtyPattern, dirty, sizeof(dirtyPattern) / sizeof(bool));
}

template <class Pixel> void SDLHiRenderer<Pixel>::drawSprites(
	int screenLine, int leftBorder, int minX, int maxX)
{
	// TODO: Pass absLine as a parameter instead of converting back.
	int absLine = screenLine / 2 + lineRenderTop;

	// Determine sprites visible on this line.
	SpriteChecker::SpriteInfo *visibleSprites;
	int visibleIndex =
		spriteChecker->getSprites(absLine, visibleSprites);
	// Optimisation: return at once if no sprites on this line.
	// Lines without any sprites are very common in most programs.
	if (visibleIndex == 0) return;

	// Sprites use 256 pixels per screen, while minX and maxX are on a scale
	// of 512 pixels per screen.
	minX /= 2;
	maxX /= 2;

	// TODO: Calculate pointers incrementally outside this method.
	Pixel *pixelPtr0 = (Pixel *)( (byte *)screen->pixels
		+ screenLine * screen->pitch + leftBorder * sizeof(Pixel));
	Pixel *pixelPtr1 = (Pixel *)(((byte *)pixelPtr0) + screen->pitch);

	if (vdp->getDisplayMode() < 8) {
		// Sprite mode 1: render directly to screen using overdraw.
		while (visibleIndex--) {
			// Get sprite info.
			SpriteChecker::SpriteInfo *sip = &visibleSprites[visibleIndex];
			Pixel colour = sip->colourAttrib & 0x0F;
			// Don't draw transparent sprites in sprite mode 1.
			// TODO: Verify on real V9938 that sprite mode 1 indeed
			//       ignores the transparency bit.
			if (colour == 0) continue;
			colour = palSprites[colour];
			SpriteChecker::SpritePattern pattern = sip->pattern;
			int x = sip->x;
			// Clip sprite pattern to render range.
			if (x < minX) {
				if (x <= minX - 32) continue;
				pattern <<= minX - x;
				x = minX;
			} else if (x > maxX - 32) {
				if (x >= maxX) continue;
				pattern &= -1 << (32 - (maxX - x));
			}
			// Convert pattern to pixels.
			Pixel *p0 = &pixelPtr0[x * 2];
			Pixel *p1 = &pixelPtr1[x * 2];
			while (pattern) {
				// Draw pixel if sprite has a dot.
				if (pattern & 0x80000000) {
					p0[0] = p0[1] = p1[0] = p1[1] = colour;
				}
				// Advancing behaviour.
				pattern <<= 1;
				p0 += 2;
				p1 += 2;
			}
		}
	} else {
		// Sprite mode 2: single pass left-to-right render.

		// Determine width of sprites.
		SpriteChecker::SpritePattern combined = 0;
		for (int i = 0; i < visibleIndex; i++) {
			combined |= visibleSprites[i].pattern;
		}
		int maxSize = SpriteChecker::patternWidth(combined);
		// Left-to-right scan.
		for (int pixelDone = minX; pixelDone < maxX; pixelDone++) {
			// Skip pixels if possible.
			int minStart = pixelDone - maxSize;
			int leftMost = 0xFFFF;
			for (int i = 0; i < visibleIndex; i++) {
				int x = visibleSprites[i].x;
				if (minStart < x && x < leftMost) leftMost = x;
			}
			if (leftMost > pixelDone) {
				pixelDone = leftMost;
				if (pixelDone >= 256) break;
			}
			// Calculate colour of pixel to be plotted.
			byte colour = 0xFF;
			for (int i = 0; i < visibleIndex; i++) {
				SpriteChecker::SpriteInfo *sip = &visibleSprites[i];
				int shift = pixelDone - sip->x;
				if ((0 <= shift && shift < maxSize)
				&& ((sip->pattern << shift) & 0x80000000)) {
					byte c = sip->colourAttrib & 0x0F;
					if (c == 0 && vdp->getTransparency()) continue;
					colour = c;
					// Merge in any following CC=1 sprites.
					for (i++ ; i < visibleIndex; i++) {
						sip = &visibleSprites[i];
						if (!(sip->colourAttrib & 0x40)) break;
						int shift = pixelDone - sip->x;
						if ((0 <= shift && shift < maxSize)
						&& ((sip->pattern << shift) & 0x80000000)) {
							colour |= sip->colourAttrib & 0x0F;
						}
					}
					break;
				}
			}
			// Plot it.
			if (colour != 0xFF) {
				int i = pixelDone * 2;
				pixelPtr0[i] = pixelPtr0[i + 1] =
				pixelPtr1[i] = pixelPtr1[i + 1] =
					palSprites[colour];
			}
		}
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::drawBorder(
	int fromX, int fromY, int limitX, int limitY)
{
	// TODO: Only redraw if necessary.
	SDL_Rect rect;
	rect.x = translateX(fromX);
	rect.w = translateX(limitX) - rect.x;
	rect.y = (fromY - lineRenderTop) * 2;
	rect.h = (limitY - fromY) * 2;

	// Note: return code ignored.
	SDL_FillRect(screen, &rect, getBorderColour());
}

// TODO: Clean up this routine.
template <class Pixel> void SDLHiRenderer<Pixel>::drawDisplay(
	int fromX, int fromY, int limitX, int limitY)
{
	// Calculate which pixels within the display area should be plotted.
	int displayLeftTicks = getDisplayLeft();
	int displayX = (fromX - displayLeftTicks) / 2;
	int displayWidth = (limitX - fromX) / 2;
	assert(0 <= displayX);
	assert(displayX + displayWidth <= 512);

	fromX = translateX(fromX);
	limitX = translateX(limitX);
	if (fromX == limitX) return;
	assert(fromX < limitX);
	//PRT_DEBUG("drawDisplay: ("<<fromX<<","<<fromY<<")-("<<limitX-1<<","<<limitY<<")");

	int displayY = (fromY - lineRenderTop) * 2;
	int nrLines = limitY - fromY;
	if (!(settings->getDeinterlace()->getValue()) &&
	    vdp->isInterlaced() &&
	    vdp->getEvenOdd()) {
		displayY++;
	}
	int displayLimitY = displayY + 2 * nrLines;

	// Render background lines:

	// Calculate display line (wraps at 256).
	byte line = fromY - vdp->getLineZero();
	if (!vdp->isTextMode()) {
		line += vdp->getVerticalScroll();
	}

	// Copy background image.
	SDL_Rect source, dest;
	source.x = displayX;
	source.w = displayWidth;
	source.h = 1;
	dest.x = fromX;

	if (vdp->isBitmapMode()) {
		if (vdp->isPlanar()) {
			renderPlanarBitmapLines(line, nrLines);
		} else {
			renderBitmapLines(line, nrLines);
		}

		int pageMaskEven, pageMaskOdd;
		if (settings->getDeinterlace()->getValue() &&
		    vdp->isInterlaced() &&
		    vdp->isEvenOddEnabled()) {
			pageMaskEven = vdp->isPlanar() ? 0x000 : 0x200;
			pageMaskOdd  = vdp->isPlanar() ? 0x100 : 0x300;
		} else {
			pageMaskEven = pageMaskOdd =
				(vdp->isPlanar() ? 0x000 : 0x200) | vdp->getEvenOddMask();
		}
		// Bring bitmap cache up to date.
		for (dest.y = displayY; dest.y < displayLimitY; ) {
			source.y = (vram->nameTable.getMask() >> 7)
				& (pageMaskEven | line);
			// TODO: Can we safely use SDL_LowerBlit?
			// Note: return value is ignored.
			SDL_BlitSurface(bitmapDisplayCache, &source, screen, &dest);
			dest.y++;
			source.y = (vram->nameTable.getMask() >> 7)
				& (pageMaskOdd | line);
			SDL_BlitSurface(bitmapDisplayCache, &source, screen, &dest);
			dest.y++;
			line++;	// wraps at 256
		}
	} else {
		renderCharacterLines(line, nrLines);

		for (dest.y = displayY; dest.y < displayLimitY; ) {
			assert(!vdp->isMSX1VDP() || line < 192);
			source.y = line;
			// TODO: Can we safely use SDL_LowerBlit?
			// Note: return value is ignored.
			/*
			printf("plotting character cache line %d to screen line %d\n",
				source.y, dest.y);
			*/
			SDL_BlitSurface(charDisplayCache, &source, screen, &dest);
			dest.y++;
			SDL_BlitSurface(charDisplayCache, &source, screen, &dest);
			dest.y++;
			line++;	// wraps at 256
		}
	}

	// Render sprites.
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
	int leftBorder = translateX(displayLeftTicks);
	for (int y = displayY; y < displayLimitY; y += 2) {
		drawSprites(y, leftBorder, displayX, displayX + displayWidth);
	}
	// Unlock surface.
	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

}

Renderer *createSDLHiRenderer(VDP *vdp, bool fullScreen, const EmuTime &time)
{
	int flags = SDL_HWSURFACE | (fullScreen ? SDL_FULLSCREEN : 0);

	// Try default bpp.
	SDL_Surface *screen = SDL_SetVideoMode(WIDTH, HEIGHT, 0, flags);

	// If no screen or unsupported screen,
	// try supported bpp in order of preference.
	int bytepp = (screen ? screen->format->BytesPerPixel : 0);
	if (bytepp != 1 && bytepp != 2 && bytepp != 4) {
		if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 15, flags);
		if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 16, flags);
		if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 32, flags);
		if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 8, flags);
	}

	if (!screen) {
		printf("FAILED to open any screen!");
		// TODO: Throw exception.
		return NULL;
	}
	PRT_DEBUG("Display is " << (int)(screen->format->BitsPerPixel) << " bpp.");

	switch (screen->format->BytesPerPixel) {
	case 1:
		return new SDLHiRenderer<Uint8>(vdp, screen, fullScreen, time);
	case 2:
		return new SDLHiRenderer<Uint16>(vdp, screen, fullScreen, time);
	case 4:
		return new SDLHiRenderer<Uint32>(vdp, screen, fullScreen, time);
	default:
		printf("FAILED to open supported screen!");
		// TODO: Throw exception.
		return NULL;
	}
}

