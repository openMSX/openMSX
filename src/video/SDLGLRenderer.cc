// $Id$

/*
TODO:
- Use GL display lists for geometry speedup.
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
#include "RenderSettings.hh"
#include "RealTime.hh"
#include "GLConsole.hh"
#include <math.h>
#include "util.hh"


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

inline static void GLDrawTexture(
	GLuint textureId, int texX,
	int screenX, int screenY, int width, int height
) {
	float texL = texX / 512.0f;
	float texR = (texX + width) / 512.0f;
	int screenL = screenX;
	int screenR = screenL + width;
	glBindTexture(GL_TEXTURE_2D, textureId);
	glBegin(GL_QUADS);
	glTexCoord2f(texL, 1); glVertex2i(screenL, screenY);
	glTexCoord2f(texR, 1); glVertex2i(screenR, screenY);
	glTexCoord2f(texR, 0); glVertex2i(screenR, screenY + height);
	glTexCoord2f(texL, 0); glVertex2i(screenL, screenY + height);
	glEnd();
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

inline static void GLDrawBlur(int offsetX, int offsetY, float alpha)
{
	int left = offsetX;
	int right = offsetX + 1024;
	int top = HEIGHT - 512 + offsetY;
	int bottom = HEIGHT + offsetY;
	glColor4f(1.0, 1.0, 1.0, alpha);
	glBegin(GL_QUADS);
	glTexCoord2i(0, 1); glVertex2i(left, top);
	glTexCoord2i(1, 1); glVertex2i(right, top);
	glTexCoord2i(1, 0); glVertex2i(right, bottom);
	glTexCoord2i(0, 0); glVertex2i(left, bottom);
	glEnd();
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
	//       This is especially tricky because division of a negative number
	//       is rounded towards zero instead of down.
	int screenX = WIDTH / 2 +
		((absoluteX & ~1) - (TICKS_VISIBLE_MIDDLE & ~1)) / 2;
	return screenX < 0 ? 0 : screenX;
}

void SDLGLRenderer::finishFrame()
{
	// Determine which effects to apply.
	int blurSetting = settings->getHorizontalBlur()->getValue();
	bool horizontalBlur = blurSetting != 0;
	int scanlineAlpha = (settings->getScanlineAlpha()->getValue() * 255) / 100;
	// TODO: Turn off scanlines when deinterlacing.
	bool scanlines = scanlineAlpha != 0;

	// Blur effect.
	if (horizontalBlur || scanlines) {
		// Store current frame as a texture.
		glBindTexture(GL_TEXTURE_2D, blurTextureId);
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1024, 512, 0);
		// Settings for blur rendering.
		float blurFactor = blurSetting / 200.0;
		if (!scanlines) blurFactor *= 0.5;
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		if (horizontalBlur) {
			// Draw stored frame with 1-pixel offsets to create a
			// horizontal blur.
			// TODO: Create display list(s).
			GLDrawBlur(-1,  0, blurFactor / (1.0 - blurFactor));
			GLDrawBlur( 1,  0, blurFactor);
		}
		if (scanlines) {
			// Make the dark line contains the average of the visible lines
			// above and below it.
			GLDrawBlur( 0, -1, 0.5);
		}
		glDisable(GL_TEXTURE_2D);
	}

	// Scanlines effect.
	if (scanlines) {
		// TODO: These are always the same lines: use a display list.
		// TODO: If interlace is active, draw scanlines on even/odd lines
		//       for odd/even frames respectively.
		if (scanlineAlpha == 255) {
			glDisable(GL_BLEND);
		} else {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		glColor4ub(0, 0, 0, scanlineAlpha);
		glBegin(GL_LINES);
		for (int y = 0; y < HEIGHT; y += 2) {
			glVertex2i(0, y); glVertex2i(WIDTH, y);
		}
		glEnd();
	}
	glDisable(GL_BLEND);

	// Render console if needed.
	console->drawConsole();

	// Update screen.
	SDL_GL_SwapBuffers();

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
	int baseMode = vdp->getDisplayMode().getBase();
	return
		( baseMode == DisplayMode::GRAPHIC7
		? PALETTE256[
			vdp->getBackgroundColour() | (vdp->getForegroundColour() << 4) ]
		: palBg[ baseMode == DisplayMode::GRAPHIC5
		       ? vdp->getBackgroundColour() & 3
		       : vdp->getBackgroundColour()
		       ]
		);
}

inline void SDLGLRenderer::renderBitmapLine(
	byte mode, int vramLine)
{
	if (lineValidInMode[vramLine] != mode) {
		const byte *vramPtr = vram->bitmapWindow.readArea(vramLine << 7);
		bitmapConverter.convertLine(lineBuffer, vramPtr);
		GLUpdateTexture(bitmapTextureIds[vramLine], lineBuffer, lineWidth);
		lineValidInMode[vramLine] = mode;
	}
}

inline void SDLGLRenderer::renderBitmapLines(
	byte line, int count)
{
	byte mode = vdp->getDisplayMode().getByte();
	// Which bits in the name mask determine the page?
	int pageMask = 0x200 | vdp->getEvenOddMask();
	while (count--) {
		// TODO: Optimise addr and line; too many conversions right now.
		int vramLine = (vram->nameTable.getMask() >> 7) & (pageMask | line);
		renderBitmapLine(mode, vramLine);
		if (vdp->isMultiPageScrolling()) {
			vramLine &= ~0x100;
			renderBitmapLine(mode, vramLine);
		}
		line++; // is a byte, so wraps at 256
	}
}

inline void SDLGLRenderer::renderPlanarBitmapLine(
	byte mode, int vramLine)
{
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
}

inline void SDLGLRenderer::renderPlanarBitmapLines(
	byte line, int count)
{
	byte mode = vdp->getDisplayMode().getByte();
	// Which bits in the name mask determine the page?
	int pageMask = vdp->getEvenOddMask();
	while (count--) {
		// TODO: Optimise addr and line; too many conversions right now.
		int vramLine = (vram->nameTable.getMask() >> 7) & (pageMask | line);
		renderPlanarBitmapLine(mode, vramLine);
		if (vdp->isMultiPageScrolling()) {
			vramLine &= ~0x100;
			renderPlanarBitmapLine(mode, vramLine);
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

SDLGLRenderer::SDLGLRenderer(VDP *vdp, SDL_Surface *screen)
	: PixelRenderer(vdp)
	, characterConverter(vdp, palFg, palBg)
	, bitmapConverter(palFg, PALETTE256, V9958_COLOURS)
	, spriteConverter(vdp->getSpriteChecker())
{
	this->screen = screen;
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

	// Clear graphics buffers.
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClearAccum(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);

	// Init OpenGL settings.
	glViewport(0, 0, WIDTH, HEIGHT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, WIDTH, HEIGHT, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

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
	// Blur:
	glGenTextures(1, &blurTextureId);
	glBindTexture(GL_TEXTURE_2D, blurTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Create bitmap display cache.
	if (vdp->isMSX1VDP()) {
		//bitmapDisplayCache = NULL;
	} else {
		//bitmapDisplayCache = new Pixel[256 * 4 * 512];
		glGenTextures(4 * 256, bitmapTextureIds);
		for (int i = 0; i < 4 * 256; i++) {
			glBindTexture(GL_TEXTURE_2D, bitmapTextureIds[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
	}

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
	} else {
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
		for (int r = 0; r < 32; r++) {
			for (int g = 0; g < 32; g++) {
				for (int b = 0; b < 32; b++) {
					const float gamma = 2.2 / 2.8;
					V9958_COLOURS[(r<<10) + (g<<5) + b] =
						   (int)(pow((float)r / 31.0, gamma) * 255)
						| ((int)(pow((float)g / 31.0, gamma) * 255) << 8)
						| ((int)(pow((float)b / 31.0, gamma) * 255) << 16)
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
	}

}

SDLGLRenderer::~SDLGLRenderer()
{
	delete console;
	// TODO: Free textures.
}

void SDLGLRenderer::reset(const EmuTime &time)
{
	PixelRenderer::reset(time);
	
	// Init renderer state.
	setDisplayMode(vdp->getDisplayMode());
	spriteConverter.setTransparency(vdp->getTransparency());

	setDirty(true);
	dirtyForeground = dirtyBackground = true;
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));

	if (!vdp->isMSX1VDP()) {
		// Reset the palette.
		for (int i = 0; i < 16; i++) {
			setPalette(i, vdp->getPalette(i));
		}
	}
}

void SDLGLRenderer::frameStart(const EmuTime &time)
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

void SDLGLRenderer::setFullScreen(
	bool fullScreen)
{
	Renderer::setFullScreen(fullScreen);
	if (((screen->flags & SDL_FULLSCREEN) != 0) != fullScreen) {
		SDL_WM_ToggleFullScreen(screen);
	}
}

void SDLGLRenderer::setDisplayMode(DisplayMode mode)
{
	dirtyChecker = modeToDirtyChecker[mode.getBase()];
	if (mode.isBitmapMode()) {
		bitmapConverter.setDisplayMode(mode);
	} else {
		characterConverter.setDisplayMode(mode);
	}
	lineWidth = mode.getLineWidth();
	// TODO: Check what happens to sprites in Graphic5 + YJK/YAE.
	spriteConverter.setNarrow(mode.getByte() == DisplayMode::GRAPHIC5);
	spriteConverter.setPalette(
		mode.getByte() == DisplayMode::GRAPHIC7 ? palGraphic7Sprites : palBg
		);
}

void SDLGLRenderer::updateTransparency(
	bool enabled, const EmuTime &time)
{
	sync(time);
	spriteConverter.setTransparency(enabled);
	
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
	if (vdp->getDisplayMode().getBase() == DisplayMode::TEXT2) {
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
	setPalette(index, grb);
}

void SDLGLRenderer::setPalette(
	int index, int grb)
{
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

void SDLGLRenderer::updateDisplayMode(
	DisplayMode mode, const EmuTime &time)
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

void SDLGLRenderer::drawBorder(int fromX, int fromY, int limitX, int limitY)
{
	// TODO: Only redraw if necessary.
	GLSetColour(getBorderColour());
	int minX = translateX(fromX);
	int maxX = translateX(limitX);
	int minY = (fromY - lineRenderTop) * 2;
	int maxY = (limitY - lineRenderTop) * 2;
	glBegin(GL_QUADS);
	glVertex2i(minX, minY);	// top left
	glVertex2i(maxX, minY);	// top right
	glVertex2i(maxX, maxY);	// bottom right
	glVertex2i(minX, maxY);	// bottom left
	glEnd();
}

void SDLGLRenderer::renderText1(
	int vramLine, int screenLine, int count, int minX, int maxX
) {
	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
	int fgColour[4] = { fg & 0xFF, (fg >> 8) & 0xFF, (fg >> 16) & 0xFF, 0xFF };
	glTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, fgColour);

	int leftBackground = translateX(vdp->getLeftBackground());

	// Render complete characters and cut off the invisible part.
	int screenHeight = 2 * count;
	glScissor(
		leftBackground + minX, HEIGHT - screenLine - screenHeight,
		maxX - minX, screenHeight
		);
	glEnable(GL_SCISSOR_TEST);

	int begCol = minX / 12;
	int endCol = (maxX + 11) / 12;
	int endRow = (vramLine + count + 7) / 8;
	screenLine -= (vramLine & 7) * 2;
	int verticalScroll = vdp->getVerticalScroll();
	for (int row = vramLine / 8; row < endRow; row++) {
		for (int col = begCol; col < endCol; col++) {
			// TODO: Only bind texture once?
			//       Currently both subroutines bind the same texture.
			int name = (row & 31) * 40 + col;
			int charcode = vram->nameTable.readNP(name);
			GLuint textureId = characterCache[charcode];
			if (dirtyPattern[charcode]) {
				// Update cache for current character.
				dirtyPattern[charcode] = false;
				// TODO: Read byte ranges from VRAM?
				//       Otherwise, have CharacterConverter read individual
				//       bytes. But what is the advantage to that?
				byte charPixels[8 * 8];
				characterConverter.convertMonoBlock(
					charPixels,
					vram->patternTable.readArea((-1 << 11) | (charcode * 8))
					);
				GLBindMonoBlock(textureId, charPixels);
			}
			// Plot current character.
			// TODO: SCREEN 0.40 characters are 12 wide, not 16.
			GLDrawMonoBlock(
				textureId, leftBackground + col * 12,
				screenLine, bg, verticalScroll
				);
		}
		screenLine += 16;
	}
	glDisable(GL_SCISSOR_TEST);
}

void SDLGLRenderer::renderGraphic2(
	int vramLine, int screenLine, int count, int minX, int maxX
) {
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	
	// Render complete characters and cut off the invisible part.
	int screenHeight = 2 * count;
	glScissor(
		translateX(vdp->getLeftBackground()) + minX, // x
		HEIGHT - screenLine - screenHeight, // y
		maxX - minX, // w
		screenHeight // h
		);
	glEnable(GL_SCISSOR_TEST);

	int col = minX / 16;
	int endCol = (maxX + 15) / 16;
	int endRow = (vramLine + count + 7) / 8;
	screenLine -= (vramLine & 7) * 2;
	for (int row = vramLine / 8; row < endRow; row++) {
		renderGraphic2Row(row & 31, screenLine, col, endCol);
		screenLine += 16;
	}
	glDisable(GL_SCISSOR_TEST);
}

void SDLGLRenderer::renderGraphic2Row(
	int row, int screenLine, int col, int endCol
) {
	int nameStart = row * 32 + col;
	int nameEnd = row * 32 + endCol;
	int quarter = nameStart & ~0xFF;
	int patternMask = vram->patternTable.getMask() / 8;
	int colourMask = vram->colourTable.getMask() / 8;
	int x = translateX(vdp->getLeftBackground()) + col * 16;
	
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

void SDLGLRenderer::drawDisplay(
	int fromX, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	int screenX = translateX(fromX);
	int screenY = (fromY - lineRenderTop) * 2;
	if (!(settings->getDeinterlace()->getValue())
	&& vdp->isInterlaced()
	&& vdp->getEvenOdd()) {
		// Display odd field half a line lower.
		screenY++;
	}
	int screenLimitY = screenY + displayHeight * 2;

	DisplayMode mode = vdp->getDisplayMode();
	int hScroll = 
		  mode.isTextMode()
		? 0
		: 16 * (vdp->getHorizontalScrollHigh() & 0x1F);
	
	// Page border is display X coordinate where to stop drawing current page.
	// This is either the multi page split point, or the right edge of the
	// rectangle to draw, whichever comes first.
	// Note that it is possible for pageBorder to be to the left of displayX,
	// in that case only the second page should be drawn.
	int pageBorder = displayX + displayWidth;
	int scrollPage1, scrollPage2;
	if (vdp->isMultiPageScrolling()) {
		scrollPage1 = vdp->getHorizontalScrollHigh() >> 5;
		scrollPage2 = scrollPage1 ^ 1;
		int pageSplit = 512 - hScroll;
		if (pageSplit < pageBorder) {
			pageBorder = pageSplit;
		}
	} else {
		scrollPage1 = 0;
		scrollPage2 = 0;
	}

	// TODO: Complete separation of character and bitmap modes.
	glEnable(GL_TEXTURE_2D);
	if (mode.isBitmapMode()) {
		// Bring bitmap cache up to date.
		if (mode.isPlanar()) {
			renderPlanarBitmapLines(displayY, displayHeight);
		} else {
			renderBitmapLines(displayY, displayHeight);
		}

		// Which bits in the name mask determine the page?
		bool deinterlaced = settings->getDeinterlace()->getValue()
			&& vdp->isInterlaced() && vdp->isEvenOddEnabled();
		int pageMaskEven, pageMaskOdd;
		if (deinterlaced || vdp->isMultiPageScrolling()) {
			pageMaskEven = mode.isPlanar() ? 0x000 : 0x200;
			pageMaskOdd  = pageMaskEven | 0x100;
		} else {
			pageMaskEven = pageMaskOdd =
				(mode.isPlanar() ? 0x000 : 0x200) | vdp->getEvenOddMask();
		}
		
		// Copy from cache to screen.
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		for (int y = screenY; y < screenLimitY; y += 2) {
			int vramLine[2];
			vramLine[0] = (vram->nameTable.getMask() >> 7)
				& (pageMaskEven | displayY);
			vramLine[1] = (vram->nameTable.getMask() >> 7)
				& (pageMaskOdd  | displayY);
			if (deinterlaced) {
				GLDrawTexture(
					bitmapTextureIds[vramLine[0]], displayX + hScroll,
					screenX, y + 0, displayWidth, 1
					);
				GLDrawTexture(
					bitmapTextureIds[vramLine[1]], displayX + hScroll,
					screenX, y + 1, displayWidth, 1
					);
			} else {
				int firstPageWidth = pageBorder - displayX;
				if (firstPageWidth > 0) {
					GLDrawTexture(
						bitmapTextureIds[vramLine[scrollPage1]],
						displayX + hScroll,
						screenX, y,
						firstPageWidth, 2
						);
				} else {
					firstPageWidth = 0;
				}
				if (firstPageWidth < displayWidth) {
					GLDrawTexture(
						bitmapTextureIds[vramLine[scrollPage2]],
						displayX + firstPageWidth + hScroll,
						screenX + firstPageWidth, y,
						displayWidth - firstPageWidth, 2
						);
				}
			}
			displayY = (displayY + 1) & 255;
		}
	} else {
		switch (mode.getByte()) {
		case DisplayMode::TEXT1:
			renderText1(
				displayY, screenY, displayHeight,
				displayX, displayX + displayWidth
				);
			break;
		case DisplayMode::GRAPHIC2:
		case DisplayMode::GRAPHIC3:
			// TODO: Implement horizontal scroll high.
			renderGraphic2(
				displayY, screenY, displayHeight,
				displayX, displayX + displayWidth
				);
			break;
		default:
			renderCharacterLines(displayY, displayHeight);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			for (int y = screenY; y < screenLimitY; y += 2) {
				GLDrawTexture(
					charTextureIds[displayY], displayX + hScroll,
					screenX, y, displayWidth, 2
					);
				displayY = (displayY + 1) & 255;
			}
			break;
		}
	}
	glDisable(GL_TEXTURE_2D);
}

void SDLGLRenderer::drawSprites(
	int fromX, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	int spriteMode = vdp->getDisplayMode().getSpriteMode();
	if (spriteMode == 0) return;
	
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	int screenX = translateX(vdp->getLeftSprites()) + displayX * 2;
	// TODO: Code duplicated from drawDisplay.
	int screenY = (fromY - lineRenderTop) * 2;
	if (!(settings->getDeinterlace()->getValue())
	&& vdp->isInterlaced()
	&& vdp->getEvenOdd()) {
		// Display odd field half a line lower.
		screenY++;
	}
	
	int displayLimitX = displayX + displayWidth;
	int limitY = fromY + displayHeight;
	for (int y = fromY; y < limitY; y++) {
		
		// Buffer to render sprite pixels to; start with all transparent.
		memset(lineBuffer, 0, 256 * sizeof(Pixel));
		
		// TODO: Possible speedups:
		// - for mode 1: create a texture in 1bpp, or in luminance
		// - use VRAM to render sprite blocks instead of lines
		if (spriteMode == 1) {
			spriteConverter.drawMode1(y, displayX, displayLimitX, lineBuffer);
		} else {
			spriteConverter.drawMode2(y, displayX, displayLimitX, lineBuffer);
		}
		
		// Make line buffer into a texture and draw it.
		GLint textureId = spriteTextureIds[y];
		GLUpdateTexture(textureId, lineBuffer, 256);
		GLDrawTexture(
			textureId, displayX * 2,
			screenX, screenY, displayWidth * 2, 2
			);
		
		screenY += 2;
	}
	
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

#endif // __SDLGLRENDERER_AVAILABLE__
