// $Id$

/*
TODO:
- Implement blinking (of page mask) in bitmap modes.
- Idea: make an abstract superclass for line-based Renderers, this
  class would know when to sync etc, but not be SDL dependent.
  Since most of the abstraction is done using <Pixel>, most code
  is SDL independent already.
  Is this still relevant after CharacterConverter/BitmapConverter
  split-off?
- Implement sprite pixels in Graphic 5.
- Move dirty checking to VDPVRAM.
*/

#include "SDLLoRenderer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "RealTime.hh"
#include <math.h>
#include "SDLConsole.hh"


/** Dimensions of screen.
  */
static const int WIDTH = 320;
static const int HEIGHT = 240;

/** Line number where top border starts.
  * This is independent of PAL/NTSC timing or number of lines per screen.
  */
static const int LINE_TOP_BORDER = 3 + 13;

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

template <class Pixel> inline void SDLLoRenderer<Pixel>::renderUntil(
	const EmuTime &time)
{
	int limitTicks = vdp->getTicksThisFrame(time);

	int limitX = limitTicks % VDP::TICKS_PER_LINE;
	limitX = (limitX - 100 - (VDP::TICKS_PER_LINE - 100) / 2 + WIDTH) / 2;
	if (limitX < 0) limitX = 0;
	else if (limitX > WIDTH) limitX = WIDTH;

	int limitY = limitTicks / VDP::TICKS_PER_LINE - lineRenderTop;
	if (limitY < 0) {
		limitX = 0;
		limitY = 0;
	} else if (limitY >= HEIGHT) {
		limitX = WIDTH;
		limitY = HEIGHT;
	}

	(this->*phaseHandler)(nextX, nextY, limitX, limitY);
	nextX = limitX;
	nextY = limitY;
}

template <class Pixel> inline void SDLLoRenderer<Pixel>::sync(
	const EmuTime &time)
{
	vram->sync(time);
	renderUntil(time);
}

template <class Pixel> inline int SDLLoRenderer<Pixel>::getLeftBorder()
{
	return (WIDTH - 256) / 2 - 7 + vdp->getHorizontalAdjust()
		+ (vdp->isTextMode() ? 9 : 0);
}

template <class Pixel> inline int SDLLoRenderer<Pixel>::getDisplayWidth()
{
	return vdp->isTextMode() ? 240 : 256;
}

template <class Pixel> inline Pixel *SDLLoRenderer<Pixel>::getLinePtr(
	SDL_Surface *displayCache, int line)
{
	return (Pixel *)( (byte *)displayCache->pixels
		+ line * displayCache->pitch );
}

// TODO: Cache this?
template <class Pixel> inline Pixel SDLLoRenderer<Pixel>::getBorderColour()
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

template <class Pixel> inline void SDLLoRenderer<Pixel>::renderBitmapLines(
	byte line, int count)
{
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
}

template <class Pixel> inline void SDLLoRenderer<Pixel>::renderPlanarBitmapLines(
	byte line, int count)
{
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
}

template <class Pixel> inline void SDLLoRenderer<Pixel>::renderCharacterLines(
	byte line, int count)
{
	while (count--) {
		// Render this line.
		characterConverter.convertLine(
			getLinePtr(charDisplayCache, line), line);
		line++; // is a byte, so wraps at 256
	}
}

template <class Pixel> SDLLoRenderer<Pixel>::DirtyChecker
	// Use checkDirtyBitmap for every mode for which isBitmapMode is true.
	SDLLoRenderer<Pixel>::modeToDirtyChecker[] = {
		// M5 M4 = 0 0  (MSX1 modes)
		&SDLLoRenderer::checkDirtyMSX1, // Graphic 1
		&SDLLoRenderer::checkDirtyMSX1, // Text 1
		&SDLLoRenderer::checkDirtyMSX1, // Multicolour
		&SDLLoRenderer::checkDirtyNull,
		&SDLLoRenderer::checkDirtyMSX1, // Graphic 2
		&SDLLoRenderer::checkDirtyMSX1, // Text 1 Q
		&SDLLoRenderer::checkDirtyMSX1, // Multicolour Q
		&SDLLoRenderer::checkDirtyNull,
		// M5 M4 = 0 1
		&SDLLoRenderer::checkDirtyMSX1, // Graphic 3
		&SDLLoRenderer::checkDirtyText2,
		&SDLLoRenderer::checkDirtyNull,
		&SDLLoRenderer::checkDirtyNull,
		&SDLLoRenderer::checkDirtyBitmap, // Graphic 4
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap,
		// M5 M4 = 1 0
		&SDLLoRenderer::checkDirtyBitmap, // Graphic 5
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap, // Graphic 6
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap,
		// M5 M4 = 1 1
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap, // Graphic 7
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap,
		&SDLLoRenderer::checkDirtyBitmap
	};

template <class Pixel> SDLLoRenderer<Pixel>::SDLLoRenderer<Pixel>(
	VDP *vdp, SDL_Surface *screen, bool fullScreen, const EmuTime &time)
	: Renderer(fullScreen)
	, characterConverter(vdp, palFg, palBg)
	, bitmapConverter(palFg, PALETTE256)
{
	// Calculate blendMask:
	//      0000BBBBGGGGRRRR 
	//  --> 0001000100010000
	const SDL_PixelFormat *format = screen->format;
	int rBit = (format->Rmask << 1) & ~format->Rmask;
	int gBit = (format->Gmask << 1) & ~format->Gmask;
	int bBit = (format->Bmask << 1) & ~format->Bmask;
	int blendMask = rBit | gBit | bBit;

	characterConverter.setBlendMask(blendMask);
	bitmapConverter.setBlendMask(blendMask);


	console = new SDLConsole(screen);

	this->vdp = vdp;
	this->screen = screen;
	vram = vdp->getVRAM();
	spriteChecker = vdp->getSpriteChecker();
	// TODO: Store current time.
	//       Does the renderer actually have to keep time?
	//       Keeping render position should be good enough.

	// Init renderer state.
	phaseHandler = &SDLLoRenderer::blankPhase;
	dirtyChecker = modeToDirtyChecker[vdp->getDisplayMode()];
	if (vdp->isBitmapMode()) {
		bitmapConverter.setDisplayMode(vdp->getDisplayMode());
	} else {
		characterConverter.setDisplayMode(vdp->getDisplayMode());
	}
	palSprites = palBg;
	setDirty(true);
	dirtyForeground = dirtyBackground = true;

	// Create display caches.
	charDisplayCache = SDL_CreateRGBSurface(
		SDL_HWSURFACE,
		256,
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
			256,
			256 * 4,
			screen->format->BitsPerPixel,
			screen->format->Rmask,
			screen->format->Gmask,
			screen->format->Bmask,
			screen->format->Amask
			)
		);
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));

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
		// Reset the palette.
		for (int i = 0; i < 16; i++) {
			updatePalette(i, vdp->getPalette(i), time);
		}
	}

	// Now we're ready to start rendering the first frame.
	frameStart(time);
}

template <class Pixel> SDLLoRenderer<Pixel>::~SDLLoRenderer()
{
	delete console;
	SDL_FreeSurface(charDisplayCache);
	SDL_FreeSurface(bitmapDisplayCache);
}

template <class Pixel> void SDLLoRenderer<Pixel>::setFullScreen(
	bool fullScreen)
{
	Renderer::setFullScreen(fullScreen);
	if (((screen->flags & SDL_FULLSCREEN) != 0) != fullScreen) {
		SDL_WM_ToggleFullScreen(screen);
	}
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateTransparency(
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

template <class Pixel> void SDLLoRenderer<Pixel>::updateForegroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	dirtyForeground = true;
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateBackgroundColour(
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

template <class Pixel> void SDLLoRenderer<Pixel>::updateBlinkForegroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	dirtyForeground = true;
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateBlinkBackgroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	dirtyBackground = true;
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateBlinkState(
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

template <class Pixel> void SDLLoRenderer<Pixel>::updatePalette(
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

template <class Pixel> void SDLLoRenderer<Pixel>::updateVerticalScroll(
	int scroll, const EmuTime &time)
{
	sync(time);
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateHorizontalAdjust(
	int adjust, const EmuTime &time)
{
	sync(time);
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateDisplayEnabled(
	bool enabled, const EmuTime &time)
{
	sync(time);
	phaseHandler = ( enabled
		? &SDLLoRenderer::displayPhase : &SDLLoRenderer::blankPhase );
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateDisplayMode(
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

template <class Pixel> void SDLLoRenderer<Pixel>::updateNameBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyName = true;
	fillBool(dirtyName, true, sizeof(dirtyName) / sizeof(bool));
}

template <class Pixel> void SDLLoRenderer<Pixel>::updatePatternBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyPattern = true;
	fillBool(dirtyPattern, true, sizeof(dirtyPattern) / sizeof(bool));
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateColourBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyColour = true;
	fillBool(dirtyColour, true, sizeof(dirtyColour) / sizeof(bool));
}

template <class Pixel> void SDLLoRenderer<Pixel>::updateVRAM(
	int addr, byte data, const EmuTime &time)
{
	// TODO: Is it possible to get rid of this method?
	//       One method call is a considerable overhead since VRAM
	//       changes occur pretty often.
	//       For example, register dirty checker at caller.

	// If display is disabled, VRAM changes will not affect the
	// renderer output, therefore sync is not necessary.
	// TODO: Changes in invisible pages do not require sync either.
	//       Maybe this is a task for the dirty checker, because what is
	//       visible is display mode dependant.
	if (vdp->isDisplayEnabled()) renderUntil(time);

	(this->*dirtyChecker)(addr, data);
}

template <class Pixel> void SDLLoRenderer<Pixel>::checkDirtyNull(
	int addr, byte data)
{
	// Do nothing: this is a bogus mode whose display doesn't depend
	// on VRAM contents.
}

template <class Pixel> void SDLLoRenderer<Pixel>::checkDirtyMSX1(
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

template <class Pixel> void SDLLoRenderer<Pixel>::checkDirtyText2(
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

template <class Pixel> void SDLLoRenderer<Pixel>::checkDirtyBitmap(
	int addr, byte data)
{
	lineValidInMode[addr >> 7] = 0xFF;
}

template <class Pixel> void SDLLoRenderer<Pixel>::setDirty(
	bool dirty)
{
	anyDirtyColour = anyDirtyPattern = anyDirtyName = dirty;
	fillBool(dirtyName, dirty, sizeof(dirtyName) / sizeof(bool));
	fillBool(dirtyColour, dirty, sizeof(dirtyColour) / sizeof(bool));
	fillBool(dirtyPattern, dirty, sizeof(dirtyPattern) / sizeof(bool));
}

template <class Pixel> void SDLLoRenderer<Pixel>::drawSprites(
	int absLine)
{
	// Check whether this line is inside the host screen.
	int screenLine = absLine - lineRenderTop;
	if (screenLine >= HEIGHT) return;

	// Determine sprites visible on this line.
	SpriteChecker::SpriteInfo *visibleSprites;
	int visibleIndex =
		spriteChecker->getSprites(absLine, visibleSprites);
	// Optimisation: return at once if no sprites on this line.
	// Lines without any sprites are very common in most programs.
	if (visibleIndex == 0) return;

	// TODO: Calculate pointers incrementally outside this method.
	Pixel *pixelPtr0 = (Pixel *)( (byte *)screen->pixels
		+ screenLine * screen->pitch + getLeftBorder() * sizeof(Pixel));

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
			// Skip any dots that end up in the border.
			if (x <= -32) {
				continue;
			} else if (x < 0) {
				pattern <<= -x;
				x = 0;
			} else if (x > 256 - 32) {
				pattern &= -1 << (32 - (256 - x));
			}
			// Convert pattern to pixels.
			Pixel *p0 = &pixelPtr0[x];
			while (pattern) {
				// Draw pixel if sprite has a dot.
				if (pattern & 0x80000000) {
					p0[0] = colour;
				}
				// Advancing behaviour.
				pattern <<= 1;
				p0 += 1;
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
		for (int pixelDone = 0; pixelDone < 256; pixelDone++) {
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
				pixelPtr0[pixelDone] = palSprites[colour];
			}
		}
	}
}

template <class Pixel> void SDLLoRenderer<Pixel>::blankPhase(
	int fromX, int fromY, int limitX, int limitY)
{
	// TODO: Only redraw if necessary.
	SDL_Rect rect;
	rect.x = 0;
	rect.y = fromY;
	rect.w = WIDTH;
	rect.h = limitY - fromY;

	// Draw lines in background colour.
	// SDL takes care of clipping
	if (rect.h > 0) {
		Pixel bgColour = getBorderColour();
		// Note: return code ignored.
		SDL_FillRect(screen, &rect, bgColour);
	}
}

template <class Pixel> void SDLLoRenderer<Pixel>::displayPhase(
	int fromX, int fromY, int limitX, int limitY)
{
	//cerr << "displayPhase from "
	//	<< "(" << fromX << "," << fromY << ") until "
	//	<< "(" << limitX << "," << limitY << ")\n";

	// Lock surface, because we will access pixels directly.
	// TODO: Move to renderXxxLines.
	SDL_Surface *displayCache =
		vdp->isBitmapMode() ? bitmapDisplayCache : charDisplayCache;
	if (SDL_MUSTLOCK(displayCache) && SDL_LockSurface(displayCache) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}
	// Render background lines.
	// TODO: Complete separation of character and bitmap modes.
	byte scrolledLine = lineRenderTop + fromY - vdp->getLineZero();
	if (!vdp->isTextMode())
		scrolledLine += vdp->getVerticalScroll();
	int lineCount = limitY - fromY;
	if (limitX > 0) lineCount++;
	if (vdp->isBitmapMode()) {
		if (vdp->isPlanar()) renderPlanarBitmapLines(scrolledLine, lineCount);
		else renderBitmapLines(scrolledLine, lineCount);
	} else {
		renderCharacterLines(scrolledLine, lineCount);
	}
	// Unlock surface.
	if (SDL_MUSTLOCK(displayCache)) SDL_UnlockSurface(displayCache);

	// Copy background image.
	int line = scrolledLine;
	int pageMaskEven, pageMaskOdd;
	if (vdp->isInterlaced() && vdp->isEvenOddEnabled()) {
		pageMaskEven = vdp->isPlanar() ? 0x000 : 0x200;
		pageMaskOdd  = vdp->isPlanar() ? 0x100 : 0x300;
	} else {
		pageMaskEven = pageMaskOdd =
			(vdp->isPlanar() ? 0x000 : 0x200) | vdp->getEvenOddMask();
	}

	// TODO: Unify MSX1 and MSX2 modes?
	SDL_Rect source;
	source.x = 0;
	source.w = getDisplayWidth();
	source.h = 1;
	SDL_Rect dest;
	dest.x = getLeftBorder();
	dest.y = fromY;
	for (int n = limitY - fromY; n--; ) {
		source.y =
			( vdp->isBitmapMode()
			? (vram->nameTable.getMask() >> 7) & (pageMaskEven | line)
			: line
			);
		// TODO: Can we safely use SDL_LowerBlit?
		// Note: return value is ignored.
		SDL_BlitSurface(displayCache, &source, screen, &dest);
		dest.y++;
		line = (line + 1) & 0xFF;
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
	for (int line = fromY; line < limitY; line++) {
		drawSprites(lineRenderTop + line);
	}
	// Unlock surface.
	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

	// Borders are drawn after the display area:
	// V9958 can extend the left border over the display area,
	// this is implemented using overdraw.
	// TODO: Does the extended border clip sprites as well?
	Pixel bgColour = getBorderColour();
	dest.x = 0;
	dest.y = fromY;
	dest.w = getLeftBorder();
	dest.h = lineCount;
	// Note: SDL clipping is relied upon because interlace can push rect
	//       one line out of the screen.
	SDL_FillRect(screen, &dest, bgColour);
	dest.x = dest.w + getDisplayWidth();
	dest.w = WIDTH - dest.x;
	SDL_FillRect(screen, &dest, bgColour);
}

template <class Pixel> void SDLLoRenderer<Pixel>::frameStart(
	const EmuTime &time)
{
	//cerr << "timing: " << (vdp->isPalTiming() ? "PAL" : "NTSC") << "\n";

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	// TODO: Use screen lines instead.
	lineRenderTop = vdp->isPalTiming() ? 59 - 14 : 32 - 14;

	// Calculate important moments in frame rendering.
	lineBottomErase = vdp->isPalTiming() ? 313 - 3 : 262 - 3;
	nextX = 0;
	nextY = 0;

	// Screen is up-to-date, so nothing is dirty.
	// TODO: Either adapt implementation to work with incremental
	//       rendering, or get rid of dirty tracking.
	//setDirty(false);
	//dirtyForeground = dirtyBackground = false;
}

template <class Pixel> void SDLLoRenderer<Pixel>::putImage(
	const EmuTime &time)
{
	// Render changes from this last frame.
	sync(time);

	// Render console if needed
	console->drawConsole();

	// Update screen.
	SDL_UpdateRect(screen, 0, 0, 0, 0);

	// The screen will be locked for a while, so now is a good time
	// to perform real time sync.
	RealTime::instance()->sync();
}

Renderer *createSDLLoRenderer(VDP *vdp, bool fullScreen, const EmuTime &time)
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
		return new SDLLoRenderer<Uint8>(vdp, screen, fullScreen, time);
	case 2:
		return new SDLLoRenderer<Uint16>(vdp, screen, fullScreen, time);
	case 4:
		return new SDLLoRenderer<Uint32>(vdp, screen, fullScreen, time);
	default:
		printf("FAILED to open supported screen!");
		// TODO: Throw exception.
		return NULL;
	}
}

