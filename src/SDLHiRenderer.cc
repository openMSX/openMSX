// $Id$

/*
TODO:
- Move to double buffering.
  Current screen line cache performs double buffering,
  but when it is changed to a display line buffer it can no longer
  serve that function.
- Further optimise the background render routines.
  For example incremental computation of the name pointers (both
  VRAM and dirty).
- Apply vertical scroll to MSX1 modes.

Idea:
For bitmap modes, cache VRAM lines rather than screen lines.
Better performance when doing raster tricks or double buffering.
Probably also easier to implement when using line buffers.
*/

#include "SDLHiRenderer.hh"
#include "VDP.hh"
#include "config.h"

#include <math.h>

/** Dimensions of screen.
  */
static const int WIDTH = 640;
static const int HEIGHT = 480;
/** NTSC phase diagram
  *
  * 192    212
  * line:  line:  phase:           handler:
  *    0      0   bottom erase     offPhase
  *    3      3   sync             offPhase
  *    6      6   top erase        offPhase
  *   19     19   top border       blankPhase
  *   45     35   display          displayPhase
  *  237    247   bottom border    blankPhase
  *  262    262   end of screen
  */
static const int LINE_TOP_BORDER = 19;
static const int LINE_END_OF_SCREEN = 262;
/** Lines rendered are [21..261).
  */
static const int LINE_RENDER_TOP = 21;
/** Where does the display start? (equal to width of left border)
  */
static const int DISPLAY_X = (WIDTH - 512) / 2;

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

	// Init render state.
	phaseHandler = &SDLHiRenderer::offPhase;
	currLine = 0;
	updateDisplayMode(vdp->getDisplayMode(), time);
	dirtyForeground = dirtyBackground = true;

	// Create display cache.
	displayCache = SDL_CreateRGBSurface(
		SDL_HWSURFACE,
		512,
		212, // TODO: 256*4
		screen->format->BitsPerPixel,
		screen->format->Rmask,
		screen->format->Gmask,
		screen->format->Bmask,
		screen->format->Amask
		);

	// Init line pointers arrays.
	for (int line = 0; line < 212; line++) {
		cacheLinePtrs[line] = (Pixel *)(
			(byte *)displayCache->pixels
			+ line * displayCache->pitch
			);
	}

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
	dirtyForeground = true;
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateBackgroundColour(
	int colour, const EmuTime &time)
{
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

template <class Pixel> void SDLHiRenderer<Pixel>::updateDisplayEnabled(
	bool enabled, const EmuTime &time)
{
	// When display is re-enabled, consider every pixel dirty.
	if (enabled) {
		dirtyForeground = true;
		setDirty(true);
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateDisplayMode(
	int mode, const EmuTime &time)
{
	renderMethod = modeToRenderMethod[mode];
	dirtyChecker = modeToDirtyChecker[mode];
	setDirty(true);
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateNameBase(
	int addr, const EmuTime &time)
{
	anyDirtyName = true;
	fillBool(dirtyName, true, sizeof(dirtyName));
}

template <class Pixel> void SDLHiRenderer<Pixel>::updatePatternBase(
	int addr, const EmuTime &time)
{
	anyDirtyPattern = true;
	fillBool(dirtyPattern, true, sizeof(dirtyPattern));
}

template <class Pixel> void SDLHiRenderer<Pixel>::updateColourBase(
	int addr, const EmuTime &time)
{
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
	// TODO: Does updateVRAM need to know the time?

	(this->*dirtyChecker)(addr, data);
}

template <class Pixel> void SDLHiRenderer<Pixel>::checkDirtyNull(
	int addr, byte data)
{
	// Do nothing: this display mode doesn't have dirty checking.
}

template <class Pixel> void SDLHiRenderer<Pixel>::checkDirtyMSX1(
	int addr, byte data)
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
	int addr, byte data)
{
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
	int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel *pixelPtr = cacheLinePtrs[line];
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
	int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel *pixelPtr = cacheLinePtrs[line];
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
	int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel *pixelPtr = cacheLinePtrs[line];
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
	int line)
{
	if (!(anyDirtyName || anyDirtyPattern || anyDirtyColour)) return;

	Pixel *pixelPtr = cacheLinePtrs[line];
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
	int line)
{
	if (!(anyDirtyName || anyDirtyPattern || anyDirtyColour)) return;

	Pixel *pixelPtr = cacheLinePtrs[line];
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
	int line)
{
	Pixel *pixelPtr = cacheLinePtrs[line];

	int scrolledLine = (line + vdp->getVerticalScroll()) & 0xFF;
	int addr = vdp->getNameMask() & (0x18000 | (scrolledLine << 7));
	int addrEnd = addr + 128;
	for (; addr < addrEnd; addr++) {
		byte colour = vdp->getVRAM(addr);
		pixelPtr[0] = pixelPtr[1] = palFg[colour >> 4];
		pixelPtr[2] = pixelPtr[3] = palFg[colour & 15];
		pixelPtr += 4;
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderGraphic5(
	int line)
{
	Pixel *pixelPtr = cacheLinePtrs[line];

	int scrolledLine = (line + vdp->getVerticalScroll()) & 0xFF;
	int addr = vdp->getNameMask() & (0x18000 | (scrolledLine << 7));
	int addrEnd = addr + 128;
	for (; addr < addrEnd; addr++) {
		byte colour = vdp->getVRAM(addr);
		*pixelPtr++ = palFg[colour >> 6];
		*pixelPtr++ = palFg[(colour >> 4) & 3];
		*pixelPtr++ = palFg[(colour >> 2) & 3];
		*pixelPtr++ = palFg[colour & 3];
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderMulti(
	int line)
{
	if (!(anyDirtyName || anyDirtyPattern)) return;

	Pixel *pixelPtr = cacheLinePtrs[line];
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

template <class Pixel> void SDLHiRenderer<Pixel>::renderBogus(
	int line)
{
	if (!(dirtyForeground || dirtyBackground)) return;

	Pixel *pixelPtr = cacheLinePtrs[line];
	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];
	for (int n = 16; n--; ) *pixelPtr++ = bg;
	for (int c = 40; c--; ) {
		for (int n = 8; n--; ) *pixelPtr++ = fg;
		for (int n = 4; n--; ) *pixelPtr++ = bg;
	}
	for (int n = 16; n--; ) *pixelPtr++ = bg;
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderMultiQ(
	int line)
{
	if (!(anyDirtyName || anyDirtyPattern)) return;

	Pixel *pixelPtr = cacheLinePtrs[line];
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

template <class Pixel> void SDLHiRenderer<Pixel>::drawSprites(
	int absLine)
{
	// Determine sprites visible on this line.
	VDP::SpriteInfo *visibleSprites;
	int visibleIndex =
		vdp->getSprites(absLine - lineDisplay, visibleSprites);

	// TODO: Calculate pointers incrementally outside this method.
	Pixel *pixelPtr0 = (Pixel *)( (byte *)screen->pixels
		+ ((absLine - LINE_RENDER_TOP) * 2) * screen->pitch
		+ DISPLAY_X * sizeof(Pixel));
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

template <class Pixel> void SDLHiRenderer<Pixel>::offPhase(
	int limit)
{
	// Check for end of phase.
	if (limit > LINE_TOP_BORDER) {
		// Top border ends off phase.
		phaseHandler = &SDLHiRenderer::blankPhase;
		limit = LINE_TOP_BORDER;
	}

	// No rendering, only catch up.
	if (currLine < limit) currLine = limit;
}

template <class Pixel> void SDLHiRenderer<Pixel>::blankPhase(
	int limit)
{
	// Check for end of phase.
	if ((currLine < lineBottomBorder) && (limit > lineDisplay)
	&& vdp->isDisplayEnabled()) {
		// Display ends blank phase.
		phaseHandler = &SDLHiRenderer::displayPhase;
		limit = lineDisplay;
	}
	else if (limit == LINE_END_OF_SCREEN) {
		// End of screen ends blank phase.
		phaseHandler = &SDLHiRenderer::offPhase;
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
		currLine = limit;
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::displayPhase(
	int limit)
{
	// Check for end of phase.
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
	if (currLine >= limit) return;

	// Lock surface, because we will access pixels directly.
	if (SDL_MUSTLOCK(displayCache) && SDL_LockSurface(displayCache) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}
	// Render background lines.
	// TODO: Put loop into render methods?
	for (int line = currLine; line < limit; line++) {
		(this->*renderMethod)(line - lineDisplay);
	}
	// Unlock surface.
	if (SDL_MUSTLOCK(displayCache)) SDL_UnlockSurface(displayCache);

	// Copy background image.
	// TODO: Is it faster or slower to alternate this with line rendering?
	SDL_Rect source;
	source.x = 0;
	source.y = currLine - lineDisplay;
	source.w = 512;
	source.h = 1;
	SDL_Rect dest;
	dest.x = DISPLAY_X;
	dest.y = (currLine - LINE_RENDER_TOP) * 2;
	for (int line = currLine; line < limit; line++) {
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
	for (int line = currLine; line < limit; line++) {
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
	dest.y = (currLine - LINE_RENDER_TOP) * 2;
	dest.w = DISPLAY_X;
	dest.h = (limit - currLine) * 2;
	SDL_FillRect(screen, &dest, bgColour);
	dest.x = WIDTH - DISPLAY_X;
	SDL_FillRect(screen, &dest, bgColour);

	currLine = limit;
}

template <class Pixel> void SDLHiRenderer<Pixel>::renderUntil(
	int limit)
{
	while (currLine < limit) {
		(this->*phaseHandler)(limit);
	}
}

template <class Pixel> void SDLHiRenderer<Pixel>::putImage()
{
	// Calculate start and end line of display.
	// TODO: Implement overscan: don't calculate end in advance,
	//       but recalculate it every line.
	int extraLines = (vdp->getNumberOfLines() - 192) / 2;
	lineDisplay = 45 - extraLines;
	lineBottomBorder = 237 + extraLines;

	// Render changes from this last frame.
	// TODO: Support PAL.
	renderUntil(262);
	currLine = 0;

	// Update screen.
	// TODO: Move to double buffering?
	SDL_UpdateRect(screen, 0, 0, 0, 0);

	// Screen is up-to-date, so nothing is dirty.
	setDirty(false);
	dirtyForeground = dirtyBackground = false;
}

Renderer *createSDLHiRenderer(VDP *vdp, bool fullScreen, const EmuTime &time)
{
	int flags = SDL_HWSURFACE | (fullScreen ? SDL_FULLSCREEN : 0);

	// Try default bpp.
	SDL_Surface *screen = SDL_SetVideoMode(WIDTH, HEIGHT, 0, flags);

	// If no screen or unsupported screen,
	// try supported bpp in order of preference.
	int bytepp = (screen ? screen->format->BytesPerPixel : 0);
	if ((bytepp != 1) && (bytepp != 2) && (bytepp != 4)) {
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

