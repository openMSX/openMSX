// $Id$

/*
TODO:
- Idea: make an abstract superclass for line-based Renderers, this
  class would know when to sync etc, but not be SDL dependent.
  Since most of the abstraction is done using <Pixel>, most code
  is SDL independent already.
- Implement sprite colour in Graphic 7.
- Implement sprite pixels in Graphic 5.
- Is it possible to combine dirtyPattern and dirtyColour into a single
  dirty array?
  Pitfalls:
  * in SCREEN1, a colour change invalidates 8 consequetive characters
  * A12 and A11 of patternMask and colourMask may be different
    also, colourMask has A10..A6 as well
    in most realistic cases however the two will be of equal size
- Clean up renderGraphics2, it is currently very hard to understand
  with all the masks and quarters etc.
- Try using a generic inlined pattern-to-pixels converter.
- Separate dirty checking and caching for character and bitmap modes?
  Dirty checking is pretty much separated already and there is a
  connection between caching and dirty checking.
  Advantages:
  * SCREEN4/5 hybrids like Space Manbow and Psycho World will be faster.
    Nice, but it's only a handful of games, so not very important.
  * Enables separation of character and bitmap code into different files.
    Useful since this file is now over 1000 lines long.
  Disadvantages:
  * More memory is used: 128K pixels, which is 25% more.
    I don't think 25% extra will pose a real problem.
- Fix character mode dirty checking to work with incremental rendering.
  Approach 1:
  * use two dirty arrays, one for this frame, one for next frame
  * on every change, mark dirty in both arrays
    (checking line is useless because of vertical scroll on screen splits)
  * in frameStart, swap arrays
  Approach 2:
  * cache characters as 16x16 blocks and blit them to the screen
  * on a name change, do nothing
  * on a pattern or colour change, mark the block as dirty
  * if a to-be-blitted block is dirty, recalculate it
  I'll implement approach 1 on account of being very similar to the
  existing code. Some time I'll implement approach 2 as well and see
  if it is an improvement (in clarity and performance).
- Register dirty checker with VDP.
  This saves one virtual method call on every VRAM write. (does it?)
  Put some generic dirty check classes in Renderer.hh/cc.
- Correctly implement vertical scroll in text modes.
  Can be implemented by reordering blitting, but uses a smaller
  wrap than GFX modes: 8 lines instead of 256 lines.
- Further optimise the background render routines.
  For example incremental computation of the name pointers (both
  VRAM and dirty).
*/

#include "SDLHiRenderer.hh"
#include "VDP.hh"
#include "RealTime.hh"
#include <math.h>
#include "ConsoleSource/SDLConsole.hh"


/** Dimensions of screen.
  */
static const int WIDTH = 640;
static const int HEIGHT = 480;

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

template <class Pixel> inline void SDLHiRenderer<Pixel>::renderUntil(
	int limit)
{
	if (nextLine < limit) {
		(this->*phaseHandler)(limit);
		nextLine = limit ;
	}
}

template <class Pixel> inline void SDLHiRenderer<Pixel>::sync(
	const EmuTime &time)
{
	int line = (vdp->getTicksThisFrame(time) + VDP::TICKS_PER_LINE / 2)
		/ VDP::TICKS_PER_LINE;
	renderUntil(line);
}

template <class Pixel> inline int SDLHiRenderer<Pixel>::getLeftBorder()
{
	return (WIDTH - 512) / 2 - 14 + vdp->getHorizontalAdjust() * 2
		+ (vdp->isTextMode() ? 16 : 0);
}

template <class Pixel> inline int SDLHiRenderer<Pixel>::getDisplayWidth()
{
	return vdp->isTextMode() ? 480 : 512;
}

template <class Pixel> inline Pixel *SDLHiRenderer<Pixel>::getLinePtr(
	SDL_Surface *displayCache, int line)
{
	return (Pixel *)( (byte *)displayCache->pixels
		+ line * displayCache->pitch );
}

template <class Pixel> inline Pixel SDLHiRenderer<Pixel>::graphic7Colour(
	byte index)
{
	return V9938_COLOURS
			[(index & 0x1C) >> 2]
			[(index & 0xE0) >> 5]
			[((index & 0x03) << 1) | ((index & 0x02) >> 1)];
}

// TODO: Cache this?
template <class Pixel> inline Pixel SDLHiRenderer<Pixel>::getBorderColour()
{
	// TODO: Used knowledge of V9938 to merge two 4-bit colours
	//       into a single 8 bit colour for SCREEN8.
	//       Keep doing that or make VDP handle SCREEN8 differently?
	return
		( vdp->getDisplayMode() == 0x1C
		? graphic7Colour(
			vdp->getBackgroundColour() | (vdp->getForegroundColour() << 4))
		: palBg[ vdp->getDisplayMode() == 0x10
		       ? vdp->getBackgroundColour() & 3
		       : vdp->getBackgroundColour()
		       ]
		);
}

// TODO: Verify on real MSX2 what non-documented modes do.
//       For now, I used renderBogus for all of them.
template <class Pixel> SDLHiRenderer<Pixel>::RenderMethod
	SDLHiRenderer<Pixel>::modeToRenderMethod[] = {
		// M5 M4 = 0 0  (MSX1 modes)
		&SDLHiRenderer::renderGraphic1,
		&SDLHiRenderer::renderText1,
		&SDLHiRenderer::renderMulti,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderGraphic2,
		&SDLHiRenderer::renderText1Q,
		&SDLHiRenderer::renderMultiQ,
		&SDLHiRenderer::renderBogus,
		// M5 M4 = 0 1
		&SDLHiRenderer::renderGraphic2, // graphic 3, actually
		&SDLHiRenderer::renderText2,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderGraphic4,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		// M5 M4 = 1 0
		&SDLHiRenderer::renderGraphic5,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderGraphic6,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		// M5 M4 = 1 1
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderGraphic7,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus
	};

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
	: Renderer(fullScreen)
{
	this->vdp = vdp;
	this->screen = screen;
	// TODO: Store current time.
	//       Does the renderer actually have to keep time?
	//       Keeping render position should be good enough.

	// Init renderer state.
	phaseHandler = &SDLHiRenderer::blankPhase;
	renderMethod = modeToRenderMethod[vdp->getDisplayMode()];
	dirtyChecker = modeToDirtyChecker[vdp->getDisplayMode()];
	setDirty(true);
	dirtyForeground = dirtyBackground = true;

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
		// Reset the palette.
		for (int i = 0; i < 16; i++) {
			updatePalette(i, vdp->getPalette(i), time);
		}
	}

	// Now we're ready to start rendering the first frame.
	frameStart();
}

template <class Pixel> SDLHiRenderer<Pixel>::~SDLHiRenderer()
{
	// TODO: SDL_Free and such.
}

template <class Pixel> void SDLHiRenderer<Pixel>::setFullScreen(
	bool fullScreen)
{
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
	fillBool(dirtyColour, true, sizeof(dirtyColour));
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
		fillBool(dirtyColour, true, sizeof(dirtyColour));
		memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateBlinkState(
	bool enabled, const EmuTime &time)
{
	sync(time);
	if (vdp->getDisplayMode() == 0x09) {
		// Text2 with blinking text.
		// Consider all characters dirty.
		// TODO: Only mark characters in blink colour dirty.
		anyDirtyName = true;
		fillBool(dirtyName, true, sizeof(dirtyName));
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::updatePalette(
	int index, int grb, const EmuTime &time)
{
	sync(time);

	// Update SDL colours in palette.
	palFg[index] = palBg[index] =
		V9938_COLOURS[(grb >> 4) & 7][(grb >> 8) & 7][grb & 7];

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
	fillBool(dirtyColour, true, sizeof(dirtyColour));
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

template <class Pixel> void SDLHiRenderer<Pixel>::updateDisplayEnabled(
	bool enabled, const EmuTime &time)
{
	sync(time);
	phaseHandler = ( enabled
		? &SDLHiRenderer::displayPhase : &SDLHiRenderer::blankPhase );
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateDisplayMode(
	int mode, const EmuTime &time)
{
	sync(time);
	renderMethod = modeToRenderMethod[mode];
	dirtyChecker = modeToDirtyChecker[mode];
	setDirty(true);
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateNameBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyName = true;
	fillBool(dirtyName, true, sizeof(dirtyName));
}

template <class Pixel> void SDLHiRenderer<Pixel>::updatePatternBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyPattern = true;
	fillBool(dirtyPattern, true, sizeof(dirtyPattern));
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateColourBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyColour = true;
	fillBool(dirtyColour, true, sizeof(dirtyColour));
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateSpriteAttributeBase(
	int addr, const EmuTime &time)
{
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateSpritePatternBase(
	int addr, const EmuTime &time)
{
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateVRAM(
	int addr, byte data, const EmuTime &time)
{
	// TODO: Is it possible to get rid of this method?
	//       One method call is a considerable overhead since VRAM
	//       changes occur pretty often.
	//       For example, register dirty checker at caller.

	// If display is disabled, VRAM changes will not affect the
	// renderer output, therefore sync is not necessary.
	// TODO: Changes in invisible pages do not require sync either.
	if (vdp->isDisplayEnabled()) sync(time);

	(this->*dirtyChecker)(addr, data, time);
}

template <class Pixel> void SDLHiRenderer<Pixel>::checkDirtyNull(
	int addr, byte data, const EmuTime &time)
{
	// Do nothing: this is a bogus mode whose display doesn't depend
	// on VRAM contents.
}

template <class Pixel> void SDLHiRenderer<Pixel>::checkDirtyMSX1(
	int addr, byte data, const EmuTime &time)
{
	if ((addr | ~(-1 << 10)) == vdp->getNameMask()) {
		dirtyName[addr & ~(-1 << 10)] = anyDirtyName = true;
	}
	if ((addr | ~(-1 << 13)) == vdp->getColourMask()) {
		dirtyColour[(addr / 8) & ~(-1 << 10)] = anyDirtyColour = true;
	}
	if ((addr | ~(-1 << 13)) == vdp->getPatternMask()) {
		dirtyPattern[(addr / 8) & ~(-1 << 10)] = anyDirtyPattern = true;
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::checkDirtyText2(
	int addr, byte data, const EmuTime &time)
{
	sync(time);
	int nameBase = vdp->getNameMask() & (-1 << 12);
	int i = addr - nameBase;
	if ((0 <= i) && (i < 2160)) {
		dirtyName[i] = anyDirtyName = true;
	}
	if ((addr | ~(-1 << 11)) == vdp->getPatternMask()) {
		dirtyPattern[(addr / 8) & ~(-1 << 8)] = anyDirtyPattern = true;
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::checkDirtyBitmap(
	int addr, byte data, const EmuTime &time)
{
	sync(time);
	lineValidInMode[addr >> 7] = 0xFF;
}

template <class Pixel> void SDLHiRenderer<Pixel>::setDirty(
	bool dirty)
{
	anyDirtyColour = anyDirtyPattern = anyDirtyName = dirty;
	fillBool(dirtyName, dirty, sizeof(dirtyName));
	fillBool(dirtyColour, dirty, sizeof(dirtyColour));
	fillBool(dirtyPattern, dirty, sizeof(dirtyPattern));
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderText1(
	Pixel *pixelPtr, int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | (line & 7)) & vdp->getPatternMask();

	// Actual display.
	int nameStart = (line / 8) * 40;
	int nameEnd = nameStart + 40;
	for (int name = nameStart; name < nameEnd; name++) {
		int charcode = vdp->getVRAM(nameBase | name);
		if (dirtyColours || dirtyName[name] || dirtyPattern[charcode]) {
			int pattern = vdp->getVRAM(patternBaseLine | (charcode * 8));
			for (int i = 6; i--; ) {
				pixelPtr[0] = pixelPtr[1] = ((pattern & 0x80) ? fg : bg);
				pixelPtr += 2;
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 12;
		}
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderText1Q(
	Pixel *pixelPtr, int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	int patternQuarter = (nameStart & ~0xFF) & (vdp->getPatternMask() / 8);
	int patternBaseLine = ((-1 << 13) | (line & 7)) & vdp->getPatternMask();

	// Actual display.
	for (int name = nameStart; name < nameEnd; name++) {
		int patternNr = vdp->getVRAM(nameBase | name) | patternQuarter;
		if (dirtyColours || dirtyName[name] || dirtyPattern[patternNr]) {
			int pattern = vdp->getVRAM(patternBaseLine | (patternNr * 8));
			for (int i = 6; i--; ) {
				pixelPtr[0] = pixelPtr[1] = ((pattern & 0x80) ? fg : bg);
				pixelPtr += 2;
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 12;
		}
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderText2(
	Pixel *pixelPtr, int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];

	int nameBase = (-1 << 12) & vdp->getNameMask();
	int nameMask = ~(-1 << 12) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | (line & 7)) & vdp->getPatternMask();

	// Actual display.
	// TODO: Implement blinking.
	int nameStart = (line / 8) * 80;
	int nameEnd = nameStart + 80;
	for (int name = nameStart; name < nameEnd; name++) {
		int maskedName = name & nameMask;
		int charcode = vdp->getVRAM(nameBase | maskedName);
		if (dirtyColours || dirtyName[maskedName] || dirtyPattern[charcode]) {
			int pattern = vdp->getVRAM(patternBaseLine | (charcode * 8));
			for (int i = 6; i--; ) {
				*pixelPtr++ = ((pattern & 0x80) ? fg : bg);
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 6;
		}
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderGraphic1(
	Pixel *pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern || anyDirtyColour)) return;

	int name = (line / 8) * 32;

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | (line & 7)) & vdp->getPatternMask();
	int colourBase = (-1 << 6) & vdp->getColourMask();

	for (int x = 0; x < 256; x += 8) {
		int charcode = vdp->getVRAM(nameBase | name);
		if (dirtyName[name] || dirtyPattern[charcode]
		|| dirtyColour[charcode / 64]) {
			int colour = vdp->getVRAM(colourBase | (charcode / 8));
			Pixel fg = palFg[colour >> 4];
			Pixel bg = palFg[colour & 0x0F];

			int pattern = vdp->getVRAM(patternBaseLine | (charcode * 8));
			// TODO: Compare performance of this loop vs unrolling.
			for (int i = 8; i--; ) {
				pixelPtr[0] = pixelPtr[1] = ((pattern & 0x80) ? fg : bg);
				pixelPtr += 2;
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 16;
		}
		name++;
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderGraphic2(
	Pixel *pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern || anyDirtyColour)) return;

	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;

	int quarter = nameStart & ~0xFF;
	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternQuarter = quarter & (vdp->getPatternMask() / 8);
	int patternBaseLine = ((-1 << 13) | (line & 7)) & vdp->getPatternMask();
	int colourNrBase = 0x3FF & (vdp->getColourMask() / 8);
	int colourBaseLine = ((-1 << 13) | (line & 7)) & vdp->getColourMask();
	for (int name = nameStart; name < nameEnd; name++) {
		int charCode = vdp->getVRAM(nameBase | name);
		int colourNr = (quarter | charCode) & colourNrBase;
		int patternNr = patternQuarter | charCode;
		if (dirtyName[name] || dirtyPattern[patternNr]
		|| dirtyColour[colourNr]) {
			int pattern = vdp->getVRAM(patternBaseLine | (patternNr * 8));
			int colour = vdp->getVRAM(colourBaseLine | (colourNr * 8));
			Pixel fg = palFg[colour >> 4];
			Pixel bg = palFg[colour & 0x0F];
			for (int i = 8; i--; ) {
				pixelPtr[0] = pixelPtr[1] = ((pattern & 0x80) ? fg : bg);
				pixelPtr += 2;
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 16;
		}
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderGraphic4(
	Pixel *pixelPtr, int line)
{
	int addr = (line << 7) & vdp->getNameMask();
	do {
		byte colour = vdp->getVRAM(addr++);
		pixelPtr[0] = pixelPtr[1] = palFg[colour >> 4];
		pixelPtr[2] = pixelPtr[3] = palFg[colour & 15];
		pixelPtr += 4;
	} while (addr & 127);
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderGraphic5(
	Pixel *pixelPtr, int line)
{
	int addr = (line << 7) & vdp->getNameMask();
	do {
		byte colour = vdp->getVRAM(addr++);
		*pixelPtr++ = palFg[colour >> 6];
		*pixelPtr++ = palFg[(colour >> 4) & 3];
		*pixelPtr++ = palFg[(colour >> 2) & 3];
		*pixelPtr++ = palFg[colour & 3];
	} while (addr & 127);
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderGraphic6(
	Pixel *pixelPtr, int line)
{
	int addr = (line << 7) & vdp->getNameMask();
	do {
		byte colour = vdp->getVRAM(addr);
		*pixelPtr++ = palFg[colour >> 4];
		*pixelPtr++ = palFg[colour & 0x0F];
		colour = vdp->getVRAM(0x10000 | addr);
		*pixelPtr++ = palFg[colour >> 4];
		*pixelPtr++ = palFg[colour & 0x0F];
	} while (++addr & 127);
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderGraphic7(
	Pixel *pixelPtr, int line)
{
	int addr = (line << 7) & vdp->getNameMask();
	do {
		pixelPtr[0] = pixelPtr[1] =
			graphic7Colour(vdp->getVRAM(addr));
		pixelPtr[2] = pixelPtr[3] =
			graphic7Colour(vdp->getVRAM(0x10000 | addr));
		pixelPtr += 4;
	} while (++addr & 127);
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderMulti(
	Pixel *pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern)) return;

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | ((line / 4) & 7))
		& vdp->getPatternMask();
	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	for (int name = nameStart; name < nameEnd; name++) {
		int charcode = vdp->getVRAM(nameBase | name);
		if (dirtyName[name] || dirtyPattern[charcode]) {
			int colour = vdp->getVRAM(patternBaseLine | (charcode * 8));
			Pixel cl = palFg[colour >> 4];
			Pixel cr = palFg[colour & 0x0F];
			for (int n = 8; n--; ) *pixelPtr++ = cl;
			for (int n = 8; n--; ) *pixelPtr++ = cr;
		}
		else {
			pixelPtr += 16;
		}
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderMultiQ(
	Pixel *pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern)) return;

	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternQuarter = (nameStart & ~0xFF) & (vdp->getPatternMask() / 8);
	int patternBaseLine = ((-1 << 13) | ((line / 4) & 7))
		& vdp->getPatternMask();
	for (int name = nameStart; name < nameEnd; name++) {
		int patternNr = patternQuarter | vdp->getVRAM(nameBase | name);
		if (dirtyName[name] || dirtyPattern[patternNr]) {
			int colour = vdp->getVRAM(patternBaseLine | (patternNr * 8));
			Pixel cl = palFg[colour >> 4];
			Pixel cr = palFg[colour & 0x0F];
			for (int n = 8; n--; ) *pixelPtr++ = cl;
			for (int n = 8; n--; ) *pixelPtr++ = cr;
		}
		else {
			pixelPtr += 16;
		}
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderBogus(
	Pixel *pixelPtr, int line)
{
	if (!(dirtyForeground || dirtyBackground)) return;

	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];
	for (int n = 16; n--; ) *pixelPtr++ = bg;
	for (int c = 40; c--; ) {
		for (int n = 8; n--; ) *pixelPtr++ = fg;
		for (int n = 4; n--; ) *pixelPtr++ = bg;
	}
	for (int n = 16; n--; ) *pixelPtr++ = bg;
}

template <class Pixel> void SDLHiRenderer<Pixel>::drawSprites(
	int absLine)
{
	// Calculate display line.
	// Negative line numbers (possible on overscan) wrap around.
	int displayLine = (absLine - vdp->getLineZero()) & 255;

	// Check whether this line is inside the host screen.
	int screenLine = (absLine - lineRenderTop) * 2;
	if (screenLine >= HEIGHT) return;

	// Determine sprites visible on this line.
	VDP::SpriteInfo *visibleSprites;
	int visibleIndex =
		vdp->getSprites(displayLine, visibleSprites);
	// Optimisation: return at once if no sprites on this line.
	// Lines without any sprites are very common in most programs.
	if (visibleIndex == 0) return;

	// TODO: Calculate pointers incrementally outside this method.
	Pixel *pixelPtr0 = (Pixel *)( (byte *)screen->pixels
		+ screenLine * screen->pitch + getLeftBorder() * sizeof(Pixel));
	Pixel *pixelPtr1 = (Pixel *)(((byte *)pixelPtr0) + screen->pitch);

	if (vdp->getDisplayMode() < 8) {
		// Sprite mode 1: render directly to screen using overdraw.
		while (visibleIndex--) {
			// Get sprite info.
			VDP::SpriteInfo *sip = &visibleSprites[visibleIndex];
			Pixel colour = sip->colourAttrib & 0x0F;
			// Don't draw transparent sprites in sprite mode 1.
			// TODO: Verify on real V9938 that sprite mode 1 indeed
			//       ignores the transparency bit.
			if (colour == 0) continue;
			colour = palBg[colour];
			VDP::SpritePattern pattern = sip->pattern;
			int x = sip->x;
			// Skip any dots that end up in the border.
			if (x < 0) {
				pattern <<= -x;
				x = 0;
			} else if (x > 256 - 32) {
				pattern &= -1 << (32 - (256 - x));
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
		VDP::SpritePattern combined = 0;
		for (int i = 0; i < visibleIndex; i++) {
			combined |= visibleSprites[i].pattern;
		}
		int size = 0;
		while (combined) {
			size++;
			combined <<= 1;
		}
		// Left-to-right scan.
		for (int pixelDone = 0; pixelDone < 256; pixelDone++) {
			// Skip pixels if possible.
			int minStart = pixelDone - size;
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
				VDP::SpriteInfo *sip = &visibleSprites[i];
				int shift = pixelDone - sip->x;
				if ((0 <= shift && shift < 32)
				&& ((sip->pattern << shift) & 0x80000000)) {
					byte c = sip->colourAttrib & 0x0F;
					if (c == 0 && vdp->getTransparency()) continue;
					colour = c;
					// Merge in any following CC=1 sprites.
					for (i++ ; i < visibleIndex; i++) {
						sip = &visibleSprites[i];
						if (!(sip->colourAttrib & 0x40)) break;
						int shift = pixelDone - sip->x;
						if ((0 <= shift && shift < 32)
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
					palBg[colour];
			}
		}
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::blankPhase(
	int limit)
{
	// TODO: Only redraw if necessary.
	SDL_Rect rect;
	rect.x = 0;
	rect.y = nextLine - lineRenderTop;
	rect.w = WIDTH;
	rect.h = limit - nextLine;

	// Clip to area actually displayed.
	// TODO: Does SDL_FillRect clip as well?
	if (rect.y < 0) {
		rect.h += rect.y;
		rect.y = 0;
	}
	else if (rect.y + rect.h > HEIGHT) {
		rect.h = HEIGHT - rect.y;
	}

	// Draw lines in background colour.
	if (rect.h > 0) {
		rect.y *= 2;
		rect.h *= 2;
		Pixel bgColour = getBorderColour();
		// Note: return code ignored.
		SDL_FillRect(screen, &rect, bgColour);
	}
}

// TODO: Instead of modifying nextLine field, pass it as a parameter.
//       After all, so is limit.
//       Having nextLine as a parameter would make it easier to put this
//       method in a separate class.
template <class Pixel> void SDLHiRenderer<Pixel>::displayPhase(
	int limit)
{
	//cerr << "displayPhase from " << nextLine << " until " << limit << "\n";

	// Check for bottom erase; even on overscan this suspends display.
	if (limit > lineBottomErase) {
		limit = lineBottomErase;
	}
	if (limit > lineRenderTop + HEIGHT) {
		limit = lineRenderTop + HEIGHT;
	}
	if (nextLine >= limit) return;

	// Perform vertical scroll.
	int scrolledLine =
		(nextLine - vdp->getLineZero() + vdp->getVerticalScroll()) & 0xFF;

	// Character mode or bitmap mode?
	SDL_Surface *displayCache =
		vdp->isBitmapMode() ? bitmapDisplayCache : charDisplayCache;

	// Lock surface, because we will access pixels directly.
	if (SDL_MUSTLOCK(displayCache) && SDL_LockSurface(displayCache) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}
	// Render background lines.
	// TODO: Complete separation of character and bitmap modes.
	if (vdp->isBitmapMode()) {
		int line = scrolledLine;
		int n = limit - nextLine;
		bool planar = vdp->isPlanar();
		do {
			int addr = planar
				? vdp->getNameMask() & (0x08000 | (line << 7))
				: vdp->getNameMask() & (0x18000 | (line << 7));
			int vramLine = addr >> 7;
			if ( (lineValidInMode[vramLine] != vdp->getDisplayMode())
			|| (planar && (lineValidInMode[vramLine | 512] != vdp->getDisplayMode())) ) {
				(this->*renderMethod)
					(getLinePtr(displayCache, vramLine), vramLine);
				lineValidInMode[vramLine] = vdp->getDisplayMode();
				if (planar) {
					lineValidInMode[vramLine | 512] = vdp->getDisplayMode();
				}
			}
			line = (line + 1) & 0xFF;
		} while (--n);
	} else {
		int line = scrolledLine;
		int n = limit - nextLine;
		do {
			(this->*renderMethod)
				(getLinePtr(displayCache, line), line);
			line = (line + 1) & 0xFF;
		} while (--n);
	}
	// Unlock surface.
	if (SDL_MUSTLOCK(displayCache)) SDL_UnlockSurface(displayCache);

	// Copy background image.
	// TODO: Unify MSX1 and MSX2 modes?
	SDL_Rect source;
	source.x = 0;
	source.w = getDisplayWidth();
	source.h = 1;
	SDL_Rect dest;
	dest.x = getLeftBorder();
	dest.y = (nextLine - lineRenderTop) * 2;
	int line = scrolledLine;
	// TODO: This is useful in the previous loop as well.
	int pageMask = vdp->isPlanar() ? 0x100 : 0x300;
	// TODO: Optimise.
	for (int n = limit - nextLine; n--; ) {
		source.y =
			( vdp->isBitmapMode()
			? (vdp->getNameMask() >> 7) & (pageMask | line)
			: line
			);
		// TODO: Can we safely use SDL_LowerBlit?
		// Note: return value is ignored.
		SDL_BlitSurface(displayCache, &source, screen, &dest);
		dest.y++;
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
	for (int line = nextLine; line < limit; line++) {
		drawSprites(line);
	}
	// Unlock surface.
	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

	// Borders are drawn after the display area:
	// V9958 can extend the left border over the display area,
	// this is implemented using overdraw.
	// TODO: Does the extended border clip sprites as well?
	Pixel bgColour = getBorderColour();
	dest.x = 0;
	dest.y = (nextLine - lineRenderTop) * 2;
	dest.w = getLeftBorder();
	dest.h = (limit - nextLine) * 2;
	SDL_FillRect(screen, &dest, bgColour);
	dest.x = dest.w + getDisplayWidth();
	dest.w = WIDTH - dest.x;
	SDL_FillRect(screen, &dest, bgColour);
}

template <class Pixel> void SDLHiRenderer<Pixel>::frameStart()
{
	//cerr << "timing: " << (vdp->isPalTiming() ? "PAL" : "NTSC") << "\n";

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	lineRenderTop = vdp->isPalTiming() ? 59 - 14 : 32 - 14;

	// Calculate important moments in frame rendering.
	lineBottomErase = vdp->isPalTiming() ? 313 - 3 : 262 - 3;
	nextLine = lineRenderTop;

	// Screen is up-to-date, so nothing is dirty.
	// TODO: Either adapt implementation to work with incremental
	//       rendering, or get rid of dirty tracking.
	//setDirty(false);
	//dirtyForeground = dirtyBackground = false;
}

template <class Pixel> void SDLHiRenderer<Pixel>::putImage(
	const EmuTime &time)
{
	// Render changes from this last frame.
	// TODO: Use sync instead?
	renderUntil(vdp->isPalTiming() ? 313 : 262);

	// Render console if needed
	Console::instance()->drawConsole();

	// Update screen.
	// Note: return value ignored.
	SDL_Flip(screen);

	// Perform initialisation for next frame.
	frameStart();

	// The screen will be locked for a while, so now is a good time
	// to perform real time sync.
	RealTime::instance()->sync();
}

Renderer *createSDLHiRenderer(VDP *vdp, bool fullScreen, const EmuTime &time)
{
	int flags = SDL_HWSURFACE | SDL_DOUBLEBUF
		| (fullScreen ? SDL_FULLSCREEN : 0);

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
	//Trying to attach a console
	SDLConsole::instance()->hookUpSDLConsole(screen);

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

