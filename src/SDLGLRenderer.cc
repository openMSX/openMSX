// $Id$

/*
TODO:
- Implement blinking (of page mask) in bitmap modes.
- Idea: make an abstract superclass for line-based Renderers, this
  class would know when to sync etc, but not be SDL dependent.
  Since most of the abstraction is done using <Pixel>, most code
  is SDL independent already.
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

#include "SDLGLRenderer.hh"
#ifdef __SDLGLRENDERER_AVAILABLE__

#include "VDP.hh"
#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "RealTime.hh"
//#include "ConsoleSource/SDLConsole.hh"
#include <math.h>

/** Dimensions of screen.
  */
static const int WIDTH = 640;
static const int HEIGHT = 480;

/** Line number where top border starts.
  * This is independent of PAL/NTSC timing or number of lines per screen.
  */
static const int LINE_TOP_BORDER = 3 + 13;

inline static void GLSetColour(SDLGLRenderer::Pixel colour)
{
	glColor3ub(colour & 0xFF, (colour >> 8) & 0xFF, (colour >> 16) & 0xFF);
}

inline static void GLBlitLine(
	SDLGLRenderer::Pixel *line, int n, int x, int y)
{
	// Set pixel format.
	glPixelStorei(GL_UNPACK_ROW_LENGTH, n);
	//glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
	glPixelStorei(GL_UNPACK_LSB_FIRST, GL_TRUE);

	// Draw pixels in frame buffer.
	glRasterPos2i(x, y + 2);
	glDrawPixels(n, 1, GL_RGBA, GL_UNSIGNED_BYTE, line);
}

/** Fill a boolean array with a single value.
  * Optimised for byte-sized booleans,
  * but correct for every size.
  */
inline static void fillBool(bool *ptr, bool value, int nr)
{
#if SIZEOF_BOOL == 1
	memset(ptr, value, nr);
#else
	while (nr--) *ptr++ = value;
#endif
}

inline void SDLGLRenderer::renderUntil(
	int limit)
{
	if (nextLine < limit) {
		(this->*phaseHandler)(limit);
		nextLine = limit ;
	}
}

inline void SDLGLRenderer::sync(
	const EmuTime &time)
{
	int line = (vdp->getTicksThisFrame(time) + VDP::TICKS_PER_LINE - 400)
		/ VDP::TICKS_PER_LINE;
	renderUntil(line);
}

inline int SDLGLRenderer::getLeftBorder()
{
	return (WIDTH - 512) / 2 - 14 + vdp->getHorizontalAdjust() * 2
		+ (vdp->isTextMode() ? 18 : 0);
}

inline int SDLGLRenderer::getDisplayWidth()
{
	return vdp->isTextMode() ? 480 : 512;
}

inline SDLGLRenderer::Pixel *SDLGLRenderer::getLinePtr(
	Pixel *displayCache, int line)
{
	return displayCache + line * 512;
}

inline SDLGLRenderer::Pixel SDLGLRenderer::graphic7Colour(
	byte index)
{
	return V9938_COLOURS
			[(index & 0x1C) >> 2]
			[(index & 0xE0) >> 5]
			[((index & 0x03) << 1) | ((index & 0x02) >> 1)];
}

// TODO: Cache this?
inline SDLGLRenderer::Pixel SDLGLRenderer::getBorderColour()
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
SDLGLRenderer::RenderMethod
	SDLGLRenderer::modeToRenderMethod[] = {
		// M5 M4 = 0 0  (MSX1 modes)
		&SDLGLRenderer::renderGraphic1,
		&SDLGLRenderer::renderText1,
		&SDLGLRenderer::renderMulti,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderGraphic2,
		&SDLGLRenderer::renderText1Q,
		&SDLGLRenderer::renderMultiQ,
		&SDLGLRenderer::renderBogus,
		// M5 M4 = 0 1
		&SDLGLRenderer::renderGraphic2, // graphic 3, actually
		&SDLGLRenderer::renderText2,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderGraphic4,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus,
		// M5 M4 = 1 0
		&SDLGLRenderer::renderGraphic5,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderGraphic6,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus,
		// M5 M4 = 1 1
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderGraphic7,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus,
		&SDLGLRenderer::renderBogus
	};

SDLGLRenderer::DirtyChecker
	// Use checkDirtyBitmap for every mode for which isBitmapMode is true.
	SDLGLRenderer::modeToDirtyChecker[] = {
		// M5 M4 = 0 0  (MSX1 modes)
		&SDLGLRenderer::checkDirtyMSX1, // Graphic 1
		&SDLGLRenderer::checkDirtyMSX1, // Text 1
		&SDLGLRenderer::checkDirtyMSX1, // Multicolour
		&SDLGLRenderer::checkDirtyNull,
		&SDLGLRenderer::checkDirtyMSX1, // Graphic 2
		&SDLGLRenderer::checkDirtyMSX1, // Text 1 Q
		&SDLGLRenderer::checkDirtyMSX1, // Multicolour Q
		&SDLGLRenderer::checkDirtyNull,
		// M5 M4 = 0 1
		&SDLGLRenderer::checkDirtyMSX1, // Graphic 3
		&SDLGLRenderer::checkDirtyText2,
		&SDLGLRenderer::checkDirtyNull,
		&SDLGLRenderer::checkDirtyNull,
		&SDLGLRenderer::checkDirtyBitmap, // Graphic 4
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap,
		// M5 M4 = 1 0
		&SDLGLRenderer::checkDirtyBitmap, // Graphic 5
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap, // Graphic 6
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap,
		// M5 M4 = 1 1
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap, // Graphic 7
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap,
		&SDLGLRenderer::checkDirtyBitmap
	};

SDLGLRenderer::SDLGLRenderer(
	VDP *vdp, SDL_Surface *screen, bool fullScreen, const EmuTime &time)
	: Renderer(fullScreen)
{
	this->vdp = vdp;
	this->screen = screen;
	vram = vdp->getVRAM();
	// TODO: Store current time.
	//       Does the renderer actually have to keep time?
	//       Keeping render position should be good enough.

	// Init OpenGL settings.
	glViewport(0, 0, WIDTH, HEIGHT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, WIDTH, HEIGHT, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Init renderer state.
	phaseHandler = &SDLGLRenderer::blankPhase;
	renderMethod = modeToRenderMethod[vdp->getDisplayMode()];
	dirtyChecker = modeToDirtyChecker[vdp->getDisplayMode()];
	palSprites = palBg;
	setDirty(true);
	dirtyForeground = dirtyBackground = true;

	// Create display caches.
	charDisplayCache = new Pixel[(vdp->isMSX1VDP() ? 192 : 256) * 512];
	bitmapDisplayCache =
		vdp->isMSX1VDP() ? NULL : new Pixel[256 * 4 * 512];
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));

	// Hide mouse cursor.
	SDL_ShowCursor(SDL_DISABLE);

	// Init the palette.
	if (vdp->isMSX1VDP()) {
		// Fixed palette.
		for (int i = 0; i < 16; i++) {
			palFg[i] = palBg[i] =
				   TMS99X8A_PALETTE[i][0]
				| (TMS99X8A_PALETTE[i][1] << 8)
				| (TMS99X8A_PALETTE[i][2] << 16)
				| 0xFF000000;
		}
	}
	else {
		// Precalculate SDL palette for V9938 colours.
		for (int r = 0; r < 8; r++) {
			for (int g = 0; g < 8; g++) {
				for (int b = 0; b < 8; b++) {
					const float gamma = 2.2 / 2.8;
					V9938_COLOURS[r][g][b] =
						   (int)(pow((float)r / 7.0, gamma) * 255)
						| ((int)(pow((float)g / 7.0, gamma) * 255) << 8)
						| ((int)(pow((float)b / 7.0, gamma) * 255) << 16)
						| 0xFF000000;
				}
			}
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

SDLGLRenderer::~SDLGLRenderer()
{
	delete charDisplayCache;
	delete bitmapDisplayCache;
}

void SDLGLRenderer::setFullScreen(
	bool fullScreen)
{
	Renderer::setFullScreen(fullScreen);
	if (((screen->flags & SDL_FULLSCREEN) != 0) != fullScreen) {
		SDL_WM_ToggleFullScreen(screen);
	}
}

void SDLGLRenderer::updateTransparency(
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

void SDLGLRenderer::updateForegroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	dirtyForeground = true;
}

void SDLGLRenderer::updateBackgroundColour(
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

void SDLGLRenderer::updateBlinkForegroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	dirtyForeground = true;
}

void SDLGLRenderer::updateBlinkBackgroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	dirtyBackground = true;
}

void SDLGLRenderer::updateBlinkState(
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
		fillBool(dirtyName, true, sizeof(dirtyName));
	}
}

void SDLGLRenderer::updatePalette(
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
	fillBool(dirtyColour, true, sizeof(dirtyColour));
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
}

void SDLGLRenderer::updateVerticalScroll(
	int scroll, const EmuTime &time)
{
	sync(time);
}

void SDLGLRenderer::updateHorizontalAdjust(
	int adjust, const EmuTime &time)
{
	sync(time);
}

void SDLGLRenderer::updateDisplayEnabled(
	bool enabled, const EmuTime &time)
{
	sync(time);
	phaseHandler = ( enabled
		? &SDLGLRenderer::displayPhase : &SDLGLRenderer::blankPhase );
}

void SDLGLRenderer::updateDisplayMode(
	int mode, const EmuTime &time)
{
	sync(time);
	renderMethod = modeToRenderMethod[mode];
	dirtyChecker = modeToDirtyChecker[mode];
	palSprites = (mode == 0x1C ? palGraphic7Sprites : palBg);
	setDirty(true);
}

void SDLGLRenderer::updateNameBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyName = true;
	fillBool(dirtyName, true, sizeof(dirtyName));
}

void SDLGLRenderer::updatePatternBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyPattern = true;
	fillBool(dirtyPattern, true, sizeof(dirtyPattern));
}

void SDLGLRenderer::updateColourBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyColour = true;
	fillBool(dirtyColour, true, sizeof(dirtyColour));
}

void SDLGLRenderer::updateSpriteAttributeBase(
	int addr, const EmuTime &time)
{
}

void SDLGLRenderer::updateSpritePatternBase(
	int addr, const EmuTime &time)
{
}

void SDLGLRenderer::updateVRAM(
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
	if (vdp->isDisplayEnabled()) sync(time);

	(this->*dirtyChecker)(addr, data, time);
}

void SDLGLRenderer::checkDirtyNull(
	int addr, byte data, const EmuTime &time)
{
	// Do nothing: this is a bogus mode whose display doesn't depend
	// on VRAM contents.
}

void SDLGLRenderer::checkDirtyMSX1(
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

void SDLGLRenderer::checkDirtyText2(
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
	// TODO: Implement dirty check on colour table (used for blinking).
}

void SDLGLRenderer::checkDirtyBitmap(
	int addr, byte data, const EmuTime &time)
{
	sync(time);
	lineValidInMode[addr >> 7] = 0xFF;
}

void SDLGLRenderer::setDirty(
	bool dirty)
{
	anyDirtyColour = anyDirtyPattern = anyDirtyName = dirty;
	fillBool(dirtyName, dirty, sizeof(dirtyName));
	fillBool(dirtyColour, dirty, sizeof(dirtyColour));
	fillBool(dirtyPattern, dirty, sizeof(dirtyPattern));
}

void SDLGLRenderer::renderText1(
	Pixel *pixelPtr, int line, int destX, int destY)
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
		int charcode = vram->getVRAM(nameBase | name);
		if (dirtyColours || dirtyName[name] || dirtyPattern[charcode]) {
			int pattern = vram->getVRAM(patternBaseLine | (charcode * 8));
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

void SDLGLRenderer::renderText1Q(
	Pixel *pixelPtr, int line, int destX, int destY)
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
		int patternNr = vram->getVRAM(nameBase | name) | patternQuarter;
		if (dirtyColours || dirtyName[name] || dirtyPattern[patternNr]) {
			int pattern = vram->getVRAM(patternBaseLine | (patternNr * 8));
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

void SDLGLRenderer::renderText2(
	Pixel *pixelPtr, int line, int destX, int destY)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel plainFg = palFg[vdp->getForegroundColour()];
	Pixel plainBg = palBg[vdp->getBackgroundColour()];
	Pixel blinkFg, blinkBg;
 	if (vdp->getBlinkState()) {
		int fg = vdp->getBlinkForegroundColour();
		blinkFg = palBg[fg ? fg : vdp->getBlinkBackgroundColour()];
		blinkBg = palBg[vdp->getBlinkBackgroundColour()];
	} else {
		blinkFg = plainFg;
		blinkBg = plainBg;
	}

	// TODO: This name masking seems unnecessarily complex.
	int nameBase = (-1 << 12) & vdp->getNameMask();
	int nameMask = ~(-1 << 12) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | (line & 7)) & vdp->getPatternMask();

	// Actual display.
	// TODO: Implement blinking.
	int nameStart = (line / 8) * 80;
	int nameEnd = nameStart + 80;
	int colourPattern = 0; // avoid warning
	for (int name = nameStart; name < nameEnd; name++) {
		// Colour table contains one bit per character.
		if ((name & 7) == 0) {
			colourPattern = vram->getVRAM(
				((-1 << 9) | (name >> 3)) & vdp->getColourMask() );
		} else {
			colourPattern <<= 1;
		}
		int maskedName = name & nameMask;
		int charcode = vram->getVRAM(nameBase | maskedName);
		if (dirtyColours || dirtyName[maskedName] || dirtyPattern[charcode]) {
			Pixel fg, bg;
			if (colourPattern & 0x80) {
				fg = blinkFg;
				bg = blinkBg;
			} else {
				fg = plainFg;
				bg = plainBg;
			}
			int pattern = vram->getVRAM(patternBaseLine | (charcode * 8));
			for (int i = 6; i--; ) {
				*pixelPtr++ = (pattern & 0x80) ? fg : bg;
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 6;
		}
	}
}

void SDLGLRenderer::renderGraphic1(
	Pixel *pixelPtr, int line, int destX, int destY)
{
	if (!(anyDirtyName || anyDirtyPattern || anyDirtyColour)) return;

	int name = (line / 8) * 32;

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | (line & 7)) & vdp->getPatternMask();
	int colourBase = (-1 << 6) & vdp->getColourMask();

	for (int x = 0; x < 256; x += 8) {
		int charcode = vram->getVRAM(nameBase | name);
		if (dirtyName[name] || dirtyPattern[charcode]
		|| dirtyColour[charcode / 64]) {
			int colour = vram->getVRAM(colourBase | (charcode / 8));
			Pixel fg = palFg[colour >> 4];
			Pixel bg = palFg[colour & 0x0F];

			int pattern = vram->getVRAM(patternBaseLine | (charcode * 8));
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

void SDLGLRenderer::renderGraphic2(
	Pixel *pixelPtr, int line, int destX, int destY)
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
		int charCode = vram->getVRAM(nameBase | name);
		int colourNr = (quarter | charCode) & colourNrBase;
		int patternNr = patternQuarter | charCode;
		if (dirtyName[name] || dirtyPattern[patternNr]
		|| dirtyColour[colourNr]) {
			int pattern = vram->getVRAM(patternBaseLine | (patternNr * 8));
			int colour = vram->getVRAM(colourBaseLine | (colourNr * 8));
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

void SDLGLRenderer::renderGraphic4(
	Pixel *pixelPtr, int line, int destX, int destY)
{
	int addr = (line << 7) & vdp->getNameMask();
	do {
		byte colour = vram->getVRAM(addr++);
		pixelPtr[0] = palFg[colour >> 4];
		pixelPtr[1] = palFg[colour & 15];
		pixelPtr += 2;
	} while (addr & 127);

	glPixelZoom(2.0, 2.0);
	GLBlitLine(pixelPtr - 256, 256, destX, destY);
}

void SDLGLRenderer::renderGraphic5(
	Pixel *pixelPtr, int line, int destX, int destY)
{
	int addr = (line << 7) & vdp->getNameMask();
	do {
		byte colour = vram->getVRAM(addr++);
		*pixelPtr++ = palFg[colour >> 6];
		*pixelPtr++ = palFg[(colour >> 4) & 3];
		*pixelPtr++ = palFg[(colour >> 2) & 3];
		*pixelPtr++ = palFg[colour & 3];
	} while (addr & 127);

	glPixelZoom(1.0, 2.0);
	GLBlitLine(pixelPtr - 512, 512, destX, destY);
}

void SDLGLRenderer::renderGraphic6(
	Pixel *pixelPtr, int line, int destX, int destY)
{
	int addr = (line << 7) & vdp->getNameMask();
	do {
		byte colour = vram->getVRAM(addr);
		*pixelPtr++ = palFg[colour >> 4];
		*pixelPtr++ = palFg[colour & 0x0F];
		colour = vram->getVRAM(0x10000 | addr);
		*pixelPtr++ = palFg[colour >> 4];
		*pixelPtr++ = palFg[colour & 0x0F];
	} while (++addr & 127);

	glPixelZoom(1.0, 2.0);
	GLBlitLine(pixelPtr - 512, 512, destX, destY);
}

void SDLGLRenderer::renderGraphic7(
	Pixel *pixelPtr, int line, int destX, int destY)
{
	int addr = (line << 7) & vdp->getNameMask();
	do {
		pixelPtr[0] = pixelPtr[1] =
			graphic7Colour(vram->getVRAM(addr));
		pixelPtr[2] = pixelPtr[3] =
			graphic7Colour(vram->getVRAM(0x10000 | addr));
		pixelPtr += 4;
	} while (++addr & 127);

	glPixelZoom(1.0, 2.0);
	GLBlitLine(pixelPtr - 512, 512, destX, destY);
}

void SDLGLRenderer::renderMulti(
	Pixel *pixelPtr, int line, int destX, int destY)
{
	if (!(anyDirtyName || anyDirtyPattern)) return;

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | ((line / 4) & 7))
		& vdp->getPatternMask();
	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	for (int name = nameStart; name < nameEnd; name++) {
		int charcode = vram->getVRAM(nameBase | name);
		if (dirtyName[name] || dirtyPattern[charcode]) {
			int colour = vram->getVRAM(patternBaseLine | (charcode * 8));
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

void SDLGLRenderer::renderMultiQ(
	Pixel *pixelPtr, int line, int destX, int destY)
{
	if (!(anyDirtyName || anyDirtyPattern)) return;

	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternQuarter = (nameStart & ~0xFF) & (vdp->getPatternMask() / 8);
	int patternBaseLine = ((-1 << 13) | ((line / 4) & 7))
		& vdp->getPatternMask();
	for (int name = nameStart; name < nameEnd; name++) {
		int patternNr = patternQuarter | vram->getVRAM(nameBase | name);
		if (dirtyName[name] || dirtyPattern[patternNr]) {
			int colour = vram->getVRAM(patternBaseLine | (patternNr * 8));
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

void SDLGLRenderer::renderBogus(
	Pixel *pixelPtr, int line, int destX, int destY)
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

void SDLGLRenderer::drawSprites(
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

	int leftBorder = getLeftBorder();
	glPixelZoom(2.0, 2.0);

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
			colour = palSprites[colour];
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
			Pixel buffer[32];
			Pixel *p = buffer;
			while (pattern) {
				// Draw pixel if sprite has a dot.
				*p++ = pattern & 0x80000000 ? colour : 0;
				// Advancing behaviour.
				pattern <<= 1;
			}
			int n = p - buffer;
			if (n) GLBlitLine(buffer, n, leftBorder + x * 2, screenLine);
		}
	} else {
		// Sprite mode 2: single pass left-to-right render.

		// Buffer to render sprite pixel to; start with all transparent.
		Pixel buffer[256];
		memset(buffer, 0, sizeof(buffer));
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
				buffer[pixelDone] = palSprites[colour];
			}
		}
		GLBlitLine(buffer, 256, leftBorder, screenLine);
	}
}

void SDLGLRenderer::blankPhase(
	int limit)
{
	// TODO: Only redraw if necessary.
	GLSetColour(getBorderColour());
	int y1 = (nextLine - lineRenderTop) * 2;
	int y2 = (limit - lineRenderTop) * 2;
	glBegin(GL_QUADS);
	glVertex2i(0, y1); // top left
	glVertex2i(WIDTH, y1); // top right
	glVertex2i(WIDTH, y2); // bottom right
	glVertex2i(0, y2); // bottom left
	glEnd();
}

// TODO: Instead of modifying nextLine field, pass it as a parameter.
//       After all, so is limit.
//       Having nextLine as a parameter would make it easier to put this
//       method in a separate class.
void SDLGLRenderer::displayPhase(
	int limit)
{
	//cerr << "displayPhase from " << nextLine << " until " << limit << "\n";

	// Check for bottom erase; even on overscan this suspends display.
	if (limit > lineBottomErase) {
		limit = lineBottomErase;
	}
	if (limit > lineRenderTop + HEIGHT / 2) {
		limit = lineRenderTop + HEIGHT / 2;
	}
	if (nextLine >= limit) return;

	// No blending.
	glDisable(GL_BLEND);

	// Perform vertical scroll.
	int scrolledLine =
		(nextLine - vdp->getLineZero() + vdp->getVerticalScroll()) & 0xFF;

	// Character mode or bitmap mode?
	Pixel *displayCache =
		vdp->isBitmapMode() ? bitmapDisplayCache : charDisplayCache;

	// Which bits in the name mask determine the page?
	int pageMask =
		(vdp->isPlanar() ? 0x000 : 0x200) | vdp->getEvenOddMask();

	// Render background lines.
	// TODO: Complete separation of character and bitmap modes.
	int leftBorder = getLeftBorder();
	int y = (nextLine - lineRenderTop) * 2;
	if (vdp->isBitmapMode()) {
		int line = scrolledLine;
		int n = limit - nextLine;
		bool planar = vdp->isPlanar();
		do {
			int vramLine = (vdp->getNameMask() >> 7) & (pageMask | line);
			Pixel lineBuffer[512];
			/*
			if ( (lineValidInMode[vramLine] != vdp->getDisplayMode())
			|| (planar && (lineValidInMode[vramLine | 512] != vdp->getDisplayMode())) ) {
				(this->*renderMethod)
					(getLinePtr(displayCache, vramLine), vramLine);
				lineValidInMode[vramLine] = vdp->getDisplayMode();
				if (planar) {
					lineValidInMode[vramLine | 512] = vdp->getDisplayMode();
				}
			}
			GLBlitLine(getLinePtr(displayCache, vramLine),
				leftBorder, y);
			*/
			(this->*renderMethod)(lineBuffer, vramLine, leftBorder, y);
			line = (line + 1) & 0xFF;
			y += 2;
		} while (--n);
	} else {
		int line = scrolledLine;
		int n = limit - nextLine;
		do {
			(this->*renderMethod)
				(getLinePtr(displayCache, line), line, leftBorder, y);
			glPixelZoom(1.0, 2.0);
			GLBlitLine(getLinePtr(displayCache, line), 512,
				leftBorder, y);
			line = (line + 1) & 0xFF;
			y += 2;
		} while (--n);
	}

	/*
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
	int pageMaskEven, pageMaskOdd;
	if (vdp->isInterlaced() && vdp->isEvenOddEnabled()) {
		pageMaskEven = vdp->isPlanar() ? 0x000 : 0x200;
		pageMaskOdd  = vdp->isPlanar() ? 0x100 : 0x300;
	} else {
		pageMaskEven = pageMaskOdd = pageMask;
	}
	// TODO: Optimise.
	for (int n = limit - nextLine; n--; ) {
		source.y =
			( vdp->isBitmapMode()
			? (vdp->getNameMask() >> 7) & (pageMaskEven | line)
			: line
			);
		// TODO: Can we safely use SDL_LowerBlit?
		// Note: return value is ignored.
		//SDL_BlitSurface(displayCache, &source, screen, &dest);
		glPixelZoom(1.0, 2.0);
		GLBlitLine(getLinePtr(displayCache, source.y), dest.x, dest.y);
		dest.y++;
		source.y =
			( vdp->isBitmapMode()
			? (vdp->getNameMask() >> 7) & (pageMaskOdd | line)
			: line
			);
		//SDL_BlitSurface(displayCache, &source, screen, &dest);
		//GLBlitLine(getLinePtr(displayCache, source.y), dest.x, dest.y);
		dest.y++;
		line = (line + 1) & 0xFF;
	}
	*/

	// Render sprites.
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GEQUAL, 0.5f);
	for (int line = nextLine; line < limit; line++) {
		drawSprites(line);
	}
	glDisable(GL_ALPHA_TEST);

	// Borders are drawn after the display area:
	// V9958 can extend the left border over the display area,
	// this is implemented using overdraw.
	// TODO: Does the extended border clip sprites as well?
	GLSetColour(getBorderColour());
	int y1 = (nextLine - lineRenderTop) * 2;
	int y2 = (limit - lineRenderTop) * 2;
	glBegin(GL_QUADS);
	// Left border:
	int left = getLeftBorder();
	glVertex2i(0, y1); // top left
	glVertex2i(left, y1); // top right
	glVertex2i(left, y2); // bottom right
	glVertex2i(0, y2); // bottom left
	// Right border:
	int right = left + getDisplayWidth();
	glVertex2i(right, y1); // top left
	glVertex2i(WIDTH, y1); // top right
	glVertex2i(WIDTH, y2); // bottom right
	glVertex2i(right, y2); // bottom left
	glEnd();
}

void SDLGLRenderer::frameStart(const EmuTime &time)
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
	nextLine = lineRenderTop;

	// Screen is up-to-date, so nothing is dirty.
	// TODO: Either adapt implementation to work with incremental
	//       rendering, or get rid of dirty tracking.
	//setDirty(false);
	//dirtyForeground = dirtyBackground = false;
}

void SDLGLRenderer::putImage(const EmuTime &time)
{
	// Render changes from this last frame.
	// TODO: Use sync instead?
	renderUntil(vdp->isPalTiming() ? 313 : 262);

	// Render console if needed
	//Console::instance()->drawConsole();

	// Update screen.
	SDL_GL_SwapBuffers();

	// The screen will be locked for a while, so now is a good time
	// to perform real time sync.
	RealTime::instance()->sync();
}

Renderer *createSDLGLRenderer(VDP *vdp, bool fullScreen, const EmuTime &time)
{
	int flags = SDL_OPENGL | SDL_HWSURFACE
		| (fullScreen ? SDL_FULLSCREEN : 0);

	// Enables OpenGL double buffering.
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);

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
	//SDLConsole::instance()->hookUpSDLConsole(screen);

	return new SDLGLRenderer(vdp, screen, fullScreen, time);
}

#endif // __SDLGLRENDERER_AVAILABLE__
