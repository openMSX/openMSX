// $Id$

/*
TODO:
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
- Draw text mode extended border together with the rest of the border.
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
- Implement dirty checking for bitmap modes.
  Approach:
  * use an array which one entry for each 128 bytes of VRAM
    contains display mode in which line is valid, or 0xFF for none
  * when constructing the screen, only re-render if display mode
    mismatches
  * in SCREEN7/8, check two entries and store on lower pages
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
#include "config.h"

#include <math.h>

/** Dimensions of screen.
  */
static const int WIDTH = 640;
static const int HEIGHT = 480;

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
	/*
	if (nextLine != limit) {
		cout << "render display lines ["
			<< (nextLine - lineDisplay) << ".."
			<< (limit - lineDisplay)
			<< ") in mode " << vdp->getDisplayMode() << "\n";
	}
	*/
	while (nextLine < limit) {
		(this->*phaseHandler)(limit);
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
	return (WIDTH - 512) / 2 - 14 + vdp->getHorizontalAdjust() * 2;
}

template <class Pixel> inline Pixel *SDLHiRenderer<Pixel>::getLinePtr(
	int line)
{
	return (Pixel *)( (byte *)displayCache->pixels
		+ line * displayCache->pitch );
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
		&SDLHiRenderer::renderBogus, // renderGraphic6
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		// M5 M4 = 1 1
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus, // renderGraphic7
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus,
		&SDLHiRenderer::renderBogus
	};

template <class Pixel> SDLHiRenderer<Pixel>::DirtyChecker
	SDLHiRenderer<Pixel>::modeToDirtyChecker[] = {
		// M5 M4 = 0 0  (MSX1 modes)
		&SDLHiRenderer::checkDirtyMSX1,
		&SDLHiRenderer::checkDirtyMSX1,
		&SDLHiRenderer::checkDirtyMSX1,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyMSX1,
		&SDLHiRenderer::checkDirtyMSX1,
		&SDLHiRenderer::checkDirtyMSX1,
		&SDLHiRenderer::checkDirtyNull,
		// M5 M4 = 0 1
		&SDLHiRenderer::checkDirtyMSX1, // graphic 3, actually
		&SDLHiRenderer::checkDirtyText2,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull, // renderGraphic4
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		// M5 M4 = 1 0
		&SDLHiRenderer::checkDirtyNull, // renderGraphic5
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull, // renderGraphic6
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		// M5 M4 = 1 1
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull, // renderGraphic7
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull,
		&SDLHiRenderer::checkDirtyNull
	};

template <class Pixel> SDLHiRenderer<Pixel>::SDLHiRenderer<Pixel>(
	VDP *vdp, SDL_Surface *screen, const EmuTime &time)
{
	this->vdp = vdp;
	this->screen = screen;
	// TODO: Store current time.
	//       Does the renderer actually have to keep time?
	//       Keeping render position should be good enough.

	// Init render state.
	phaseHandler = 0;
	nextLine = 999; // past end-of-screen
	updateDisplayMode(vdp->getDisplayMode(), time);
	dirtyForeground = dirtyBackground = true;

	// Create display cache.
	displayCache = SDL_CreateRGBSurface(
		SDL_HWSURFACE,
		512,
		vdp->isMSX1VDP() ? 192 : 256 * 4,
		screen->format->BitsPerPixel,
		screen->format->Rmask,
		screen->format->Gmask,
		screen->format->Bmask,
		screen->format->Amask
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
		// Reset the palette.
		for (int i = 0; i < 16; i++) {
			updatePalette(i, vdp->getPalette(i), time);
		}
	}

	// Now we're ready to start rendering the first frame.
	frameStart();
	phaseHandler = &SDLHiRenderer::blankPhase;
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
	if (vdp->getBackgroundColour() == index) {
		dirtyBackground = true;
		// Transparent pixels have background colour.
		palFg[0] = palBg[vdp->getBackgroundColour()];
	}

	// Any line containing pixels of this colour must be repainted.
	// We don't know which lines contain which colours,
	// so we have to repaint them all.
	anyDirtyColour = true;
	fillBool(dirtyColour, true, sizeof(dirtyColour));
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

	// When display is re-enabled, consider every pixel dirty.
	if (enabled) {
		dirtyForeground = true;
		setDirty(true);
	}
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
	/*
	For the highest accuracy, we should sync here.
	But I don't think the damage of rendering VRAM changes one frame
	too early is worth the overhead, so I disabled it.
	Well-behaving programs don't write to the visual area during
	scanning anyway.
	*/
	sync(time);
	(this->*dirtyChecker)(addr, data, time);
}

template <class Pixel> void SDLHiRenderer<Pixel>::checkDirtyNull(
	int addr, byte data, const EmuTime &time)
{
	// Do nothing: this display mode doesn't have dirty checking.
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

	// Extended left border.
	for (int n = 16; n--; ) *pixelPtr++ = bg;
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
	// Extended right border.
	for (int n = 16; n--; ) *pixelPtr++ = bg;
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

	// Extended left border.
	for (int n = 16; n--; ) *pixelPtr++ = bg;
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
	// Extended right border.
	for (int n = 16; n--; ) *pixelPtr++ = bg;
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

	// Extended left border.
	for (int n = 16; n--; ) *pixelPtr++ = bg;
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
	// Extended right border.
	for (int n = 16; n--; ) *pixelPtr++ = bg;
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
	int addr = line << 7;
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
	int addr = line << 7;
	do {
		byte colour = vdp->getVRAM(addr++);
		*pixelPtr++ = palFg[colour >> 6];
		*pixelPtr++ = palFg[(colour >> 4) & 3];
		*pixelPtr++ = palFg[(colour >> 2) & 3];
		*pixelPtr++ = palFg[colour & 3];
	} while (addr & 127);
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
	// Determine sprites visible on this line.
	VDP::SpriteInfo *visibleSprites;
	int visibleIndex =
		vdp->getSprites(absLine - lineDisplay, visibleSprites);

	// TODO: Calculate pointers incrementally outside this method.
	Pixel *pixelPtr0 = (Pixel *)( (byte *)screen->pixels
		+ ((absLine - lineRenderTop) * 2) * screen->pitch
		+ getLeftBorder() * sizeof(Pixel));
	Pixel *pixelPtr1 = (Pixel *)(((byte *)pixelPtr0) + screen->pitch);

	while (visibleIndex--) {
		// Get sprite info.
		VDP::SpriteInfo *sip = &visibleSprites[visibleIndex];
		Pixel colour = sip->colour;
		// Don't draw transparent sprites in sprite mode 1
		// or if transparency is enabled.
		// TODO: Verify on real V9938 that sprite mode 1 indeed
		//       ignores the transparency
		if (colour == 0
		&& (vdp->getDisplayMode() < 8 || vdp->getTransparency())) {
			continue;
		}
		colour = palBg[colour];
		int pattern = sip->pattern;
		int x = sip->x;
		// Skip any dots that end up in the border.
		if (x < 0) {
			pattern <<= -x;
			x = 0;
		}
		else if (x > 256 - 32) {
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

}

template <class Pixel> void SDLHiRenderer<Pixel>::blankPhase(
	int limit)
{
	// Check for end of phase.
	if (nextLine < lineBottomBorder && limit > lineDisplay
	&& vdp->isDisplayEnabled()) {
		// Display ends blank phase.
		phaseHandler = &SDLHiRenderer::displayPhase;
		limit = lineDisplay;
	}

	// Render lines up to limit.
	if (nextLine < limit) {
		// TODO: Only redraw if necessary.
		SDL_Rect rect;
		rect.x = 0;
		rect.y = nextLine - lineRenderTop;
		rect.w = WIDTH;
		rect.h = limit - nextLine;

		// Clip to area actually displayed.
		if (rect.y < 0) {
			rect.h += rect.y;
			rect.y = 0;
		}
		else if (rect.y + rect.h > HEIGHT) {
			rect.h = HEIGHT - rect.y;
		}

		if (rect.h > 0) {
			rect.y *= 2;
			rect.h *= 2;
			// SCREEN6 has separate even/odd pixels in the border.
			// TODO: Implement the case that even_colour != odd_colour.
			Pixel bgColour = palBg[
				( vdp->getDisplayMode() == 0x10
				? vdp->getBackgroundColour() & 3
				: vdp->getBackgroundColour()
				)];
			// Note: return code ignored.
			SDL_FillRect(screen, &rect, bgColour);
		}
		nextLine = limit;
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::displayPhase(
	int limit)
{
	// Check for end of phase.
	// TODO: Check whether overscan works with this implementation.
	if (!vdp->isDisplayEnabled()) {
		// Forced blanking ends display phase.
		phaseHandler = &SDLHiRenderer::blankPhase;
		return;
	}
	if (limit > lineBottomBorder) {
		// Bottom border ends display phase.
		phaseHandler = &SDLHiRenderer::blankPhase;
		limit = lineBottomBorder;
	}
	int numLines = limit - nextLine;
	if (numLines <= 0) return;

	// Perform vertical scroll.
	int scrolledLine =
		(nextLine - lineDisplay + vdp->getVerticalScroll()) & 0xFF;
	if (scrolledLine + numLines >= 256) {
		// If page wraps around, render it in two steps.
		// TODO: If wrap amount is lower (R#2 as mask in bitmap modes),
		//       split at the wrap point as well.
		numLines = 256 - scrolledLine;
		limit = nextLine + numLines;
	}

	// Lock surface, because we will access pixels directly.
	if (SDL_MUSTLOCK(displayCache) && SDL_LockSurface(displayCache) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}
	// Render background lines.
	// TODO: Unify MSX1 and MSX2 modes? Or separate them more?
	if (vdp->isBitmapMode()) {
		int line = scrolledLine;
		int n = numLines;
		do {
			// TODO: Use variables to store settings that are different
			//   in SCREEN7/8.
			int addr = vdp->getNameMask() & (0x18000 | (line << 7));
			int vramLine = addr >> 7;
			(this->*renderMethod)(getLinePtr(vramLine), vramLine);
			line++;
		} while (--n);
	} else {
		int line = scrolledLine;
		int n = numLines;
		do {
			(this->*renderMethod)(getLinePtr(line), line);
			line++;
		} while (--n);
	}
	// Unlock surface.
	if (SDL_MUSTLOCK(displayCache)) SDL_UnlockSurface(displayCache);

	// Copy background image.
	// TODO: Unify MSX1 and MSX2 modes?
	SDL_Rect source;
	source.x = 0;
	if (vdp->isBitmapMode()) {
		source.y =
			(vdp->getNameMask() & (0x18000 | (scrolledLine << 7))) >> 7;
	} else {
		source.y = scrolledLine;
	}
	source.w = 512;
	source.h = 1;
	SDL_Rect dest;
	dest.x = getLeftBorder();
	dest.y = (nextLine - lineRenderTop) * 2;
	for (int n = numLines; n--; ) {
		// TODO: Can we safely use SDL_LowerBlit?
		// Note: return value is ignored.
		SDL_BlitSurface(displayCache, &source, screen, &dest);
		dest.y++;
		SDL_BlitSurface(displayCache, &source, screen, &dest);
		dest.y++;
		source.y++;
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

	// SCREEN6 has separate even/odd pixels in the border.
	// TODO: Implement the case that even_colour != odd_colour.
	Pixel bgColour = palBg[
		( vdp->getDisplayMode() == 0x10
		? vdp->getBackgroundColour() & 3
		: vdp->getBackgroundColour()
		)];
	dest.x = 0;
	dest.y = (nextLine - lineRenderTop) * 2;
	dest.w = getLeftBorder();
	dest.h = numLines * 2;
	SDL_FillRect(screen, &dest, bgColour);
	dest.x = dest.w + 512;
	dest.w = WIDTH - dest.x;
	SDL_FillRect(screen, &dest, bgColour);

	nextLine = limit;
}

template <class Pixel> void SDLHiRenderer<Pixel>::frameStart()
{
	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	lineRenderTop = vdp->isPalTiming() ? 59 - 14 : 32 - 14;

	// Calculate start and end line of display.
	// TODO: Implement overscan: don't calculate end in advance,
	//       but recalculate it every line.
	lineDisplay = vdp->getDisplayStart() / VDP::TICKS_PER_LINE;
	lineBottomBorder = lineDisplay + vdp->getNumberOfLines();
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
	renderUntil(vdp->isPalTiming() ? 313 : 262);

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

	switch (screen->format->BytesPerPixel) {
	case 1:
		return new SDLHiRenderer<Uint8>(vdp, screen, time);
	case 2:
		return new SDLHiRenderer<Uint16>(vdp, screen, time);
	case 4:
		return new SDLHiRenderer<Uint32>(vdp, screen, time);
	default:
		printf("FAILED to open supported screen!");
		// TODO: Throw exception.
		return NULL;
	}
}

