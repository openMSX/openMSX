// $Id$

/*
TODO:
- Scanline emulation.
- Use 8x8 textures in character mode.
- Use GL display lists for geometry speedup.
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
*/

#include "SDLGLRenderer.hh"
#ifdef __SDLGLRENDERER_AVAILABLE__

#include "VDP.hh"
#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "RealTime.hh"
#include "GLConsole.hh"
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

inline static void GLUpdateTexture(
	GLuint textureId, const SDLGLRenderer::Pixel *data, int lineWidth)
{
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexImage2D(
		GL_TEXTURE_2D,
		0, // level
		GL_RGBA,
		lineWidth, // width
		1, // height
		0, // border
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		data
		);
}

inline static void GLDrawTexture(GLuint textureId, int leftBorder, int y, int minX, int maxX)
{
	glBindTexture(GL_TEXTURE_2D, textureId);
	glBegin(GL_QUADS);
	glTexCoord2f(minX / 512.0f, 0); glVertex2i(leftBorder + minX, y + 2);	// Bottom Left
	glTexCoord2f(maxX / 512.0f, 0); glVertex2i(leftBorder + maxX, y + 2);	// Bottom Right
	glTexCoord2f(maxX / 512.0f, 1); glVertex2i(leftBorder + maxX, y);	// Top Right
	glTexCoord2f(minX / 512.0f, 1); glVertex2i(leftBorder + minX, y);	// Top Left
	glEnd();
}

inline static void GLBlitLine(
	GLint textureId, const SDLGLRenderer::Pixel *data, int leftBorder, int y, int minX, int maxX)
{
	GLUpdateTexture(textureId, data, 256);
	GLDrawTexture(textureId, leftBorder, y, minX, maxX);
	
	/* Alternative (outdated)
	 
	GLSetColour(0xFFFFFFu);

	// Set pixel format.
	glPixelStorei(GL_UNPACK_ROW_LENGTH, n);
	//glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
	glPixelStorei(GL_UNPACK_LSB_FIRST, GL_TRUE);

	// Draw pixels in frame buffer.
	glRasterPos2i(x, y + 2);
	glDrawPixels(n, 1, GL_RGBA, GL_UNSIGNED_BYTE, line);
	*/
}

inline static void GLBindMonoBlock(GLuint textureId, const byte *pixels)
{
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexImage2D(
		GL_TEXTURE_2D,
		0, // level
		GL_LUMINANCE8,
		8, // width
		8, // height
		0, // border
		GL_LUMINANCE,
		GL_UNSIGNED_BYTE,
		pixels
		);
}

inline static void GLBindColourBlock(
	GLuint textureId, const SDLGLRenderer::Pixel *pixels)
{
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexImage2D(
		GL_TEXTURE_2D,
		0, // level
		GL_RGBA,
		8, // width
		8, // height
		0, // border
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		pixels
		);
}

inline static void GLDrawMonoBlock(
	GLuint textureId, int x, int y, SDLGLRenderer::Pixel bg, int verticalScroll)
{
	glBindTexture(GL_TEXTURE_2D, textureId);
	GLSetColour(bg);
	glBegin(GL_QUADS);
	int x1 = x + 16;
	int y1 = y + 16;
	GLfloat roll = verticalScroll / 8.0f;
	glTexCoord2f(0.0f, 1.0f + roll); glVertex2i(x,  y1); // Bottom Left
	glTexCoord2f(1.0f, 1.0f + roll); glVertex2i(x1, y1); // Bottom Right
	glTexCoord2f(1.0f,        roll); glVertex2i(x1, y ); // Top Right
	glTexCoord2f(0.0f,        roll); glVertex2i(x,  y ); // Top Left
	glEnd();
}

inline static void GLDrawColourBlock(GLuint textureId, int x, int y)
{
	glBindTexture(GL_TEXTURE_2D, textureId);
	glBegin(GL_QUADS);
	int x1 = x + 16;
	int y1 = y + 16;
	glTexCoord2i(0, 1); glVertex2i(x,  y1); // Bottom Left
	glTexCoord2i(1, 1); glVertex2i(x1, y1); // Bottom Right
	glTexCoord2i(1, 0); glVertex2i(x1, y ); // Top Right
	glTexCoord2i(0, 0); glVertex2i(x,  y ); // Top Left
	glEnd();
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

inline void SDLGLRenderer::setDisplayMode(int mode)
{
	dirtyChecker = modeToDirtyChecker[mode];
	if (vdp->isBitmapMode(mode)) {
		bitmapConverter.setDisplayMode(mode);
	} else {
		characterConverter.setDisplayMode(mode);
	}
	lineWidth = (mode == 0x09 || mode == 0x10 || mode == 0x14 ? 512 : 256);
	palSprites = (mode == 0x1C ? palGraphic7Sprites : palBg);
}

inline void SDLGLRenderer::renderUntil(const EmuTime &time)
{
	switch (accuracy) {
	case ACC_PIXEL: {
		int limitTicks = vdp->getTicksThisFrame(time);
		int limitX = limitTicks % VDP::TICKS_PER_LINE;
		//limitX = (limitX - 100 - (VDP::TICKS_PER_LINE - 100) / 2 + WIDTH) / 2;
		limitX = (limitX - 100 / 2 - 102) / 2;
		int limitY = limitTicks / VDP::TICKS_PER_LINE - lineRenderTop;
		if (limitY < 0) {
			limitX = 0;
			limitY = 0;
		} else if (limitY >= HEIGHT / 2 - 1) {
			limitX = WIDTH;
			limitY = HEIGHT / 2 - 1;
		} else if (limitX < 0) {
			limitX = 0;
		} else if (limitX > WIDTH) {
			limitX = WIDTH;
		}
		assert(limitY <= (vdp->isPalTiming() ? 313 : 262));

		// split in rectangles
		if (nextY == limitY) {
			// only one line
			(this->*phaseHandler)(nextX, nextY, limitX, limitY);
		} else {
			if (nextX != 0) {
				// top
				(this->*phaseHandler)(nextX, nextY, WIDTH, nextY);
				nextY++;
			}
			if (limitX == WIDTH) {
				// middle + bottom
				(this->*phaseHandler)(0, nextY, WIDTH, limitY);
			} else { 
				if (limitY > nextY)
					// middle
					(this->*phaseHandler)(0, nextY, WIDTH, limitY - 1);
				// bottom
				(this->*phaseHandler)(0, limitY, limitX, limitY);
			}
		}
		nextX = limitX;
		nextY = limitY;
		break;
	}
	case ACC_LINE: {
		int limitTicks = vdp->getTicksThisFrame(time);
		int limitY = limitTicks / VDP::TICKS_PER_LINE - lineRenderTop;
		assert(limitY <= (vdp->isPalTiming() ? 313 : 262));
		if (nextY < limitY) {
			(this->*phaseHandler)(0, nextY, WIDTH, limitY);
			nextY = limitY ;
		}
		break;
	}
	case ACC_SCREEN: {
		// TODO
		break;
	}
	default:
		assert(false);
	}
}

inline void SDLGLRenderer::sync(const EmuTime &time)
{
	vram->sync(time);
	renderUntil(time);
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

// TODO: Cache this?
inline SDLGLRenderer::Pixel SDLGLRenderer::getBorderColour()
{
	// TODO: Used knowledge of V9938 to merge two 4-bit colours
	//       into a single 8 bit colour for SCREEN8.
	//       Keep doing that or make VDP handle SCREEN8 differently?
	return
		( vdp->getDisplayMode() == 0x1C
		? PALETTE256[
			vdp->getBackgroundColour() | (vdp->getForegroundColour() << 4) ]
		: palBg[ vdp->getDisplayMode() == 0x10
		       ? vdp->getBackgroundColour() & 3
		       : vdp->getBackgroundColour()
		       ]
		);
}

inline void SDLGLRenderer::renderBitmapLines(
	byte line, int count)
{
	int mode = vdp->getDisplayMode();
	// Which bits in the name mask determine the page?
	int pageMask = 0x200 | vdp->getEvenOddMask();
	while (count--) {
		// TODO: Optimise addr and line; too many conversions right now.
		int vramLine = (vram->nameTable.getMask() >> 7) & (pageMask | line);
		if (lineValidInMode[vramLine] != mode) {
			const byte *vramPtr = vram->bitmapWindow.readArea(vramLine << 7);
			bitmapConverter.convertLine(lineBuffer, vramPtr);
			GLUpdateTexture(bitmapTextureIds[vramLine], lineBuffer, lineWidth);
			lineValidInMode[vramLine] = mode;
		}
		line++; // is a byte, so wraps at 256
	}
}

inline void SDLGLRenderer::renderPlanarBitmapLines(
	byte line, int count)
{
	int mode = vdp->getDisplayMode();
	// Which bits in the name mask determine the page?
	int pageMask = vdp->getEvenOddMask();
	while (count--) {
		// TODO: Optimise addr and line; too many conversions right now.
		int vramLine = (vram->nameTable.getMask() >> 7) & (pageMask | line);
		if ( lineValidInMode[vramLine] != mode
		|| lineValidInMode[vramLine | 512] != mode ) {
			int addr0 = vramLine << 7;
			int addr1 = addr0 | 0x10000;
			const byte *vramPtr0 = vram->bitmapWindow.readArea(addr0);
			const byte *vramPtr1 = vram->bitmapWindow.readArea(addr1);
			bitmapConverter.convertLinePlanar(lineBuffer, vramPtr0, vramPtr1);
			GLUpdateTexture(bitmapTextureIds[vramLine], lineBuffer, lineWidth);
			lineValidInMode[vramLine] =
				lineValidInMode[vramLine | 512] = mode;
		}
		line++; // is a byte, so wraps at 256
	}
}

inline void SDLGLRenderer::renderCharacterLines(
	byte line, int count)
{
	while (count--) {
		// Render this line.
		characterConverter.convertLine(lineBuffer, line);
		GLUpdateTexture(charTextureIds[line], lineBuffer, lineWidth);
		line++; // is a byte, so wraps at 256
	}
}

SDLGLRenderer::DirtyChecker
	// Use checkDirtyBitmap for every mode for which isBitmapMode is true.
	SDLGLRenderer::modeToDirtyChecker[] = {
		// M5 M4 = 0 0  (MSX1 modes)
		&SDLGLRenderer::checkDirtyMSX1Graphic, // Graphic 1
		&SDLGLRenderer::checkDirtyMSX1Text, // Text 1
		&SDLGLRenderer::checkDirtyMSX1Graphic, // Multicolour
		&SDLGLRenderer::checkDirtyNull,
		&SDLGLRenderer::checkDirtyMSX1Graphic, // Graphic 2
		&SDLGLRenderer::checkDirtyMSX1Text, // Text 1 Q
		&SDLGLRenderer::checkDirtyMSX1Graphic, // Multicolour Q
		&SDLGLRenderer::checkDirtyNull,
		// M5 M4 = 0 1
		&SDLGLRenderer::checkDirtyMSX1Graphic, // Graphic 3
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
	, characterConverter(vdp, palFg, palBg)
	, bitmapConverter(palFg, PALETTE256)
{
	console = new GLConsole();

	GLint size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
	printf("Max texture size: %d\n", size);
	glTexImage2D(
		GL_PROXY_TEXTURE_2D,
		0,
		GL_RGBA,
		512,
		1,
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		NULL
		);
	size = -1;
	glGetTexLevelParameteriv(
		GL_PROXY_TEXTURE_2D,
		0,
		GL_TEXTURE_WIDTH,
		&size
		);
	printf("512*1 texture fits: %d (512 if OK, 0 if failed)\n", size);

	this->vdp = vdp;
	this->screen = screen;
	vram = vdp->getVRAM();
	spriteChecker = vdp->getSpriteChecker();
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
	setDisplayMode(vdp->getDisplayMode());
	setDirty(true);
	dirtyForeground = dirtyBackground = true;

	// Create character display cache.
	// Line based:
	glGenTextures(256, charTextureIds);
	for (int i = 0; i < 256; i++) {
		glBindTexture(GL_TEXTURE_2D, charTextureIds[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	// Block based:
	glGenTextures(4 * 256, characterCache);
	for (int i = 0; i < 4 * 256; i++) {
		glBindTexture(GL_TEXTURE_2D, characterCache[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	// Sprites:
	glGenTextures(313, spriteTextureIds);
	for (int i = 0; i < 313; i++) {
		glBindTexture(GL_TEXTURE_2D, spriteTextureIds[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	// Create bitmap display cache.
	if (vdp->isMSX1VDP()) {
		//bitmapDisplayCache = NULL;
	} else {
		//bitmapDisplayCache = new Pixel[256 * 4 * 512];
		glGenTextures(4 * 256, bitmapTextureIds);
		for (int i = 0; i < 4 * 256; i++) {
			glBindTexture(GL_TEXTURE_2D, bitmapTextureIds[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
	}
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

SDLGLRenderer::~SDLGLRenderer()
{
	delete console;
	// TODO: Free textures.
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
	fillBool(dirtyColour, true, sizeof(dirtyColour) / sizeof(bool));
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
		fillBool(dirtyColour, true, sizeof(dirtyColour) / sizeof(bool));
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
		fillBool(dirtyName, true, sizeof(dirtyName) / sizeof(bool));
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
	fillBool(dirtyColour, true, sizeof(dirtyColour) / sizeof(bool));
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
	setDisplayMode(mode);
	setDirty(true);
}

void SDLGLRenderer::updateNameBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyName = true;
	fillBool(dirtyName, true, sizeof(dirtyName) / sizeof(bool));
}

void SDLGLRenderer::updatePatternBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyPattern = true;
	fillBool(dirtyPattern, true, sizeof(dirtyPattern) / sizeof(bool));
}

void SDLGLRenderer::updateColourBase(
	int addr, const EmuTime &time)
{
	sync(time);
	anyDirtyColour = true;
	fillBool(dirtyColour, true, sizeof(dirtyColour) / sizeof(bool));
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
	if (vdp->isDisplayEnabled()) renderUntil(time);

	(this->*dirtyChecker)(addr, data);
}

void SDLGLRenderer::checkDirtyNull(
	int addr, byte data)
{
	// Do nothing: this is a bogus mode whose display doesn't depend
	// on VRAM contents.
}

void SDLGLRenderer::checkDirtyMSX1Text(
	int addr, byte data)
{
	if (vram->nameTable.isInside(addr)) {
		dirtyName[addr & ~(-1 << 10)] = anyDirtyName = true;
	}
	if (vram->colourTable.isInside(addr)) {
		dirtyColour[(addr / 8) & ~(-1 << 8)] = anyDirtyColour = true;
	}
	if (vram->patternTable.isInside(addr)) {
		dirtyPattern[(addr / 8) & ~(-1 << 8)] = anyDirtyPattern = true;
	}
}

void SDLGLRenderer::checkDirtyMSX1Graphic(
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

void SDLGLRenderer::checkDirtyText2(
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

void SDLGLRenderer::checkDirtyBitmap(
	int addr, byte data)
{
	lineValidInMode[addr >> 7] = 0xFF;
}

void SDLGLRenderer::setDirty(
	bool dirty)
{
	anyDirtyColour = anyDirtyPattern = anyDirtyName = dirty;
	fillBool(dirtyName, dirty, sizeof(dirtyName) / sizeof(bool));
	fillBool(dirtyColour, dirty, sizeof(dirtyColour) / sizeof(bool));
	fillBool(dirtyPattern, dirty, sizeof(dirtyPattern) / sizeof(bool));
}

void SDLGLRenderer::drawSprites(int screenLine, int leftBorder, int minX, int maxX)
{
	// Check whether this line is inside the host screen.
	int absLine = screenLine / 2 + lineRenderTop;

	// Determine sprites visible on this line.
	SpriteChecker::SpriteInfo *visibleSprites;
	int visibleIndex =
		spriteChecker->getSprites(absLine, visibleSprites);
	// Optimisation: return at once if no sprites on this line.
	// Lines without any sprites are very common in most programs.
	if (visibleIndex == 0) return;

	if (vdp->getDisplayMode() < 8) {
		// Sprite mode 1: render directly to screen using overdraw.

		// Buffer to render sprite pixel to; start with all transparent.
		memset(lineBuffer, 0, 256 * sizeof(Pixel));

		// Iterate over all sprites on this line.
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
			//Pixel buffer[32];
			//Pixel *p = buffer;
			Pixel *p = lineBuffer + x;
			while (pattern) {
				// Draw pixel if sprite has a dot.
				if (pattern & 0x80000000) *p = colour;
				// Advancing behaviour.
				p++;
				pattern <<= 1;
			}
			//int n = p - buffer;
			//if (n) GLBlitLine(spriteTextureIds[screenLine / 2], buffer, n, leftBorder + x * 2, screenLine);
		}
		// TODO:
		// Possible speedups:
		// - create a texture in 1bpp, or in luminance
		// - use VRAM to render sprite blocks instead of lines
		GLBlitLine(spriteTextureIds[absLine], lineBuffer, leftBorder, screenLine, minX, maxX);
	} else {
		// Sprite mode 2: single pass left-to-right render.

		// Buffer to render sprite pixel to; start with all transparent.
		memset(lineBuffer, 0, 256 * sizeof(Pixel));
		// Determine width of sprites.
		SpriteChecker::SpritePattern combined = 0;
		for (int i = 0; i < visibleIndex; i++) {
			combined |= visibleSprites[i].pattern;
		}
		int maxSize = SpriteChecker::patternWidth(combined);
		// Left-to-right scan.
		for (int pixelDone = minX / 2; pixelDone < maxX / 2; pixelDone++) {
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
				lineBuffer[pixelDone] = palSprites[colour];
			}
		}
		GLBlitLine(spriteTextureIds[absLine], lineBuffer, leftBorder, screenLine, minX, maxX);
	}
}

void SDLGLRenderer::blankPhase(
	int fromX, int fromY, int limitX, int limitY)
{
	if (fromX == limitX) return;
	assert(fromX < limitX);
	//PRT_DEBUG("BlankPhase: ("<<fromX<<","<<fromY<<")-("<<limitX-1<<","<<limitY<<")");

	// TODO: Only redraw if necessary.
	GLSetColour(getBorderColour());
	int minX = fromX;
	int maxX = limitX;
	int minY = fromY * 2;
	int maxY = limitY * 2 + 2;
	glBegin(GL_QUADS);
	glVertex2i(minX, minY);	// top left
	glVertex2i(maxX, minY);	// top right
	glVertex2i(maxX, maxY);	// bottom right
	glVertex2i(minX, maxY);	// bottom left
	glEnd();
}

void SDLGLRenderer::renderText1(int vramLine, int screenLine, int count)
{
	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
	float fgColour[4] = {
		(fg & 0xFF) / 255.0f,
		((fg >> 8) & 0xFF) / 255.0f,
		((fg >> 16) & 0xFF) / 255.0f,
		1.0f
		};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, fgColour);

	// Render complete characters and cut off the invisible part
	int screenHeight = 2 * count;
	glScissor(0, HEIGHT - screenLine - screenHeight, WIDTH, screenHeight);
	glEnable(GL_SCISSOR_TEST);
	
	int leftBorder = getLeftBorder();

	int endRow = (vramLine + count + 7) / 8;
	screenLine -= (vramLine & 7) * 2;
	int verticalScroll = vdp->getVerticalScroll();
	for (int row = vramLine / 8; row < endRow; row++) {
		for (int col = 0; col < 40; col++) {
			// TODO: Only bind texture once?
			//       Currently both subdirs bind the same texture.
			int name = (row & 31) * 40 + col;
			int charcode = vram->nameTable.readNP(name);
			GLuint textureId = characterCache[charcode];
			if (dirtyPattern[charcode]) {
				// Update cache for current character.
				dirtyPattern[charcode] = false;
				// TODO: Read byte ranges from VRAM?
				//       Otherwise, have CharacterConverter read individual
				//       bytes. But what is the advantage to that?
				// TODO: vertical scrolling in character 
				byte charPixels[8 * 8];
				characterConverter.convertMonoBlock(
					charPixels,
					vram->patternTable.readArea((-1 << 11) | (charcode * 8))
					);
				GLBindMonoBlock(textureId, charPixels);
			}
			// Plot current character.
			// TODO: SCREEN 0.40 characters are 12 wide, not 16.
			GLDrawMonoBlock(textureId, leftBorder + col * 12,
			                screenLine, bg, verticalScroll);
		}
		screenLine += 16;
	}
	glDisable(GL_SCISSOR_TEST);
}

void SDLGLRenderer::renderGraphic2(int vramLine, int screenLine, int count)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	
	// Render complete characters and cut off the invisible part
	int screenHeight = 2 * count;
	glScissor(0, HEIGHT - screenLine - screenHeight, WIDTH, screenHeight);
	glEnable(GL_SCISSOR_TEST);

	int endRow = (vramLine + count + 7) / 8;
	screenLine -= (vramLine & 7) * 2;
	for (int row = vramLine / 8; row < endRow; row++) {
		renderGraphic2Row(row & 31, screenLine);
		screenLine += 16;
	}
	glDisable(GL_SCISSOR_TEST);
}

void SDLGLRenderer::renderGraphic2Row(int row, int screenLine)
{
	int nameStart = row * 32;
	int nameEnd = nameStart + 32;
	int quarter = nameStart & ~0xFF;
	int patternMask = vram->patternTable.getMask() / 8;
	int colourMask = vram->colourTable.getMask() / 8;
	int x = getLeftBorder();
	for (int name = nameStart; name < nameEnd; name++) {
		int charNr = quarter | vram->nameTable.readNP((-1 << 10) | name);
		int colourNr = charNr & colourMask;
		int patternNr = charNr & patternMask;
		GLuint textureId = characterCache[charNr];
		if (dirtyPattern[patternNr] || dirtyColour[colourNr]) {
			// TODO: With all the masks and quarters, is this dirty
			//       checking really correct?
			dirtyPattern[patternNr] = false;
			dirtyColour[colourNr] = false;
			Pixel charPixels[8 * 8];
			int index = (-1 << 13) | (charNr * 8);
			characterConverter.convertColourBlock(
				charPixels,
				vram->patternTable.readArea(index),
				vram->colourTable.readArea(index)
				);
			GLBindColourBlock(textureId, charPixels);
		}
		GLDrawColourBlock(textureId, x, screenLine);
		x += 16;
	}
}

void SDLGLRenderer::displayPhase(
	int fromX, int fromY, int limitX, int limitY)
{
	if (fromX == limitX) return;
	assert(fromX < limitX);
	//PRT_DEBUG("DisplayPhase: ("<<fromX<<","<<fromY<<")-("<<limitX-1<<","<<limitY<<")");

	int n = limitY - fromY + 1;
	int minY = fromY * 2;
	if (vdp->isInterlaced() && vdp->getEvenOdd())
		minY++;
	int maxY = minY + 2 * n;

	// V9958 can extend the left border over the display area,
	// The extended border clips sprites as well.
	int leftBorder = getLeftBorder();
	int left = leftBorder;
	// if (vdp->maskedBorder()) left += 8;
	if (fromX < left) {
		GLSetColour(getBorderColour());
		glBegin(GL_QUADS);
		glVertex2i(fromX, minY);	// top left
		glVertex2i(left,  minY);	// top right
		glVertex2i(left,  maxY);	// bottom right
		glVertex2i(fromX, maxY);	// bottom left
		glEnd();
		fromX = left;
		if (fromX >= limitX) return;
	}
	
	// Render background lines
	int minX = fromX - leftBorder;
	assert(0 <= minX);
	if (minX < 512) {
		int maxX = limitX - leftBorder;
		if (maxX > 512) maxX = 512;
		
		// Perform vertical scroll (wraps at 256)
		byte line = lineRenderTop + fromY - vdp->getLineZero();
		if (!vdp->isTextMode())
			line += vdp->getVerticalScroll();

		// TODO: Complete separation of character and bitmap modes.
		glEnable(GL_TEXTURE_2D);
		if (vdp->isBitmapMode()) {
			if (vdp->isPlanar())
				renderPlanarBitmapLines(line, n);
			else
				renderBitmapLines(line, n);
			// Which bits in the name mask determine the page?
			int pageMask =
				(vdp->isPlanar() ? 0x000 : 0x200) | vdp->getEvenOddMask();
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			for (int y = minY; y < maxY; y += 2) {
				int vramLine = (vram->nameTable.getMask() >> 7) & (pageMask | line);
				GLDrawTexture(bitmapTextureIds[vramLine], leftBorder, y, minX, maxX);
				line++;	// wraps at 256
			}
		} else {
			switch (vdp->getDisplayMode()) {
			case 1:
				renderText1(line, minY, n);
				break;
			case 4: // graphic2
			case 8: // graphic4
				renderGraphic2(line, minY, n);
				break;
			default:
				renderCharacterLines(line, n);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				for (int y = minY; y < maxY; y += 2) {
					GLDrawTexture(charTextureIds[line], leftBorder, y, minX, maxX);
					line++;	// wraps at 256
				}
				break;
			}
		}

		// Render sprites.
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		//glPixelZoom(2.0, 2.0);
		for (int y = minY; y < maxY; y += 2) {
			drawSprites(y, leftBorder, minX, maxX);
		}
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
	}

	// Right border
	int right = leftBorder + getDisplayWidth();
	if (right < limitX) {
		GLSetColour(getBorderColour());
		glBegin(GL_QUADS);
		glVertex2i(right,  minY);	// top left
		glVertex2i(limitX, minY);	// top right
		glVertex2i(limitX, maxY);	// bottom right
		glVertex2i(right,  maxY);	// bottom left
		glEnd();
	}
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
	nextX = 0;
	nextY = 0;
	

	// Screen is up-to-date, so nothing is dirty.
	// TODO: Either adapt implementation to work with incremental
	//       rendering, or get rid of dirty tracking.
	//setDirty(false);
	//dirtyForeground = dirtyBackground = false;
}

void SDLGLRenderer::putImage(const EmuTime &time)
{
	// Render changes from this last frame.
	sync(time);

	// Render console if needed
	console->drawConsole();

	// Update screen.
	SDL_GL_SwapBuffers();

	// The screen will be locked for a while, so now is a good time
	// to perform real time sync.
	RealTime::instance()->sync();
}

Renderer *createSDLGLRenderer(VDP *vdp, bool fullScreen, const EmuTime &time)
{
	int flags = SDL_OPENGL | SDL_HWSURFACE |
	            (fullScreen ? SDL_FULLSCREEN : 0);

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

	return new SDLGLRenderer(vdp, screen, fullScreen, time);
}

#endif // __SDLGLRENDERER_AVAILABLE__
