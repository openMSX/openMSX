// $Id$

/*
TODO:
- Use GL display lists for geometry speedup.
- Is it possible to combine dirtyPattern and dirtyColour into a single
  dirty array?
  Pitfalls:
  * in SCREEN1, a colour change invalidates 8 consequetive characters
    however, the code only checks dirtyPattern, because there is nothing
    to cache for the colour table (patterns are monochrome textures)
  * A12 and A11 of patternMask and colourMask may be different
    also, colourMask has A10..A6 as well
    in most realistic cases however the two will be of equal size
*/

#include "SDLGLRenderer.hh"

#include <cmath>
#include <cassert>
#include "util.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "RenderSettings.hh"
#include "CommandConsole.hh"
#include "GLConsole.hh"
#include "ScreenShotSaver.hh"
#include "EventDistributor.hh"
#include "FloatSetting.hh"

#ifdef __WIN32__
#include <windows.h>
static int lastWindowX = 0;
static int lastWindowY = 0;
#endif

namespace openmsx {

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

inline static SDLGLRenderer::Pixel GLMapRGB(int r, int g, int b)
{
	if (OPENMSX_BIGENDIAN) {
		return (r << 24) | (g << 16) | (b <<  8) | 0x000000FF;
	} else {
		return (r <<  0) | (g <<  8) | (b << 16) | 0xFF000000;
	}
}

inline static void GLSetColour(SDLGLRenderer::Pixel colour)
{
	glColor3ub(colour & 0xFF, (colour >> 8) & 0xFF, (colour >> 16) & 0xFF);
}

inline static void GLSetTexEnvCol(SDLGLRenderer::Pixel colour) {
	float colourVec[4] = {
		(colour & 0xFF) / 255.0f,
		((colour >> 8) & 0xFF) / 255.0f,
		((colour >> 16) & 0xFF) / 255.0f,
		1.0f
		};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, colourVec);
}

inline static void GLFillBlock(
	int x1, int y1, int x2, int y2,
	SDLGLRenderer::Pixel colour
) {
	GLSetColour(colour);
	glBegin(GL_QUADS);
	glVertex2i(x1, y2); // Bottom Left
	glVertex2i(x2, y2); // Bottom Right
	glVertex2i(x2, y1 ); // Top Right
	glVertex2i(x1, y1 ); // Top Left
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
	GLuint textureId, int x, int y, int width, int zoom,
	SDLGLRenderer::Pixel bg, int verticalScroll
) {
	glBindTexture(GL_TEXTURE_2D, textureId);
	GLSetColour(bg);
	glBegin(GL_QUADS);
	int x1 = x + width * zoom;
	int y1 = y + 16;
	GLfloat texR = width / 8.0f;
	GLfloat roll = verticalScroll / 8.0f;
	glTexCoord2f(0.0f, 1.0f + roll); glVertex2i(x,  y1); // Bottom Left
	glTexCoord2f(texR, 1.0f + roll); glVertex2i(x1, y1); // Bottom Right
	glTexCoord2f(texR,        roll); glVertex2i(x1, y ); // Top Right
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
	// Glow effect.
	// Must be applied before storedImage is updated.
	int glowSetting = settings.getGlow()->getValue();
	if (glowSetting != 0 && prevStored) {
		// Draw stored image.
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBindTexture(GL_TEXTURE_2D, storedImageTextureId);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		// Note:
		// 100% glow means current frame has no influence at all.
		// Values near 100% may have the same effect due to rounding.
		// This formula makes sure that on 15bpp R/G/B value 0 can still pull
		// down R/G/B value 31 of the previous frame.
		GLDrawBlur(0, 0, glowSetting * 31 / 3200.0);
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
	}

	// Store current frame as a texture.
	glBindTexture(GL_TEXTURE_2D, storedImageTextureId);
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1024, 512, 0);
	prevStored = true;

	// Avoid repainting the buffer by putImage.
	frameDirty = false;

	putImage();
	Event* finishFrameEvent = new SimpleEvent<FINISH_FRAME_EVENT>();
	EventDistributor::instance().distributeEvent(finishFrameEvent);
}

void SDLGLRenderer::drawRest()
{
	drawEffects();

	// Render consoles if needed.
	console->drawConsole();

	// Update screen.
	SDL_GL_SwapBuffers();
}

int SDLGLRenderer::putPowerOffImage()
{
	// draw noise texture.
	float x = (float)rand() / RAND_MAX;
	float y = (float)rand() / RAND_MAX;
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, noiseTextureId);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f + x, 1.5f + y); glVertex2i(   0, HEIGHT - 512);
	glTexCoord2f(3.0f + x, 1.5f + y); glVertex2i(1024, HEIGHT - 512);
	glTexCoord2f(3.0f + x, 0.0f + y); glVertex2i(1024, HEIGHT);
	glTexCoord2f(0.0f + x, 0.0f + y); glVertex2i(   0, HEIGHT);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	
	// Render console if needed.
	console->drawConsole();

	// Update screen.
	SDL_GL_SwapBuffers();
	return 10;	// 10 fps
}

void SDLGLRenderer::putImage()
{
	if (frameDirty) {
		// Copy stored image to screen.
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, storedImageTextureId);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		// Note: Without blending enabled, this method is rather efficient.
		GLDrawBlur(0, 0, 1.0);
		glDisable(GL_TEXTURE_2D);
	}

	drawRest();
	frameDirty = true; // drawRest made it dirty...
}

void SDLGLRenderer::takeScreenShot(const string& filename)
	throw(CommandException)
{
	byte* row_pointers[HEIGHT];
	byte buffer[WIDTH * HEIGHT * 3];
	for (int i = 0; i < HEIGHT; ++i) {
		row_pointers[HEIGHT - 1 - i] = &buffer[WIDTH * 3 * i];
	}
	glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	ScreenShotSaver::save(WIDTH, HEIGHT, row_pointers, filename);
}

void SDLGLRenderer::drawEffects()
{
	// Determine which effects to apply.
	int blurSetting = settings.getHorizontalBlur()->getValue();
	int scanlineAlpha = (settings.getScanlineAlpha()->getValue() * 255) / 100;

	// TODO: Turn off scanlines when deinterlacing.
	bool horizontalBlur = blurSetting != 0;
	bool scanlines = scanlineAlpha != 0;

	// Blur effect.
	if (horizontalBlur || scanlines) {
		// Settings for blur rendering.
		float blurFactor = blurSetting / 200.0;
		if (!scanlines) blurFactor *= 0.5;
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBindTexture(GL_TEXTURE_2D, storedImageTextureId);
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
			glVertex2f(0, y + 1.5); glVertex2f(WIDTH, y + 1.5);
		}
		glEnd();
	}
	glDisable(GL_BLEND);
}

// TODO: Cache this?
inline SDLGLRenderer::Pixel SDLGLRenderer::getBorderColour()
{
	int mode = vdp->getDisplayMode().getByte();
	int bgColour = vdp->getBackgroundColour();
	if (vdp->getDisplayMode().getBase() == DisplayMode::GRAPHIC5) {
		// TODO: Border in SCREEN6 has separate colour for even and odd pixels.
		//       Until that is supported, only use odd pixel colour.
		bgColour &= 0x03;
	}
	return (mode == DisplayMode::GRAPHIC7 ? PALETTE256 : palBg)[bgColour];
}

inline void SDLGLRenderer::renderBitmapLine(
	byte mode, int vramLine)
{
	if (lineValidInMode[vramLine] != mode) {
		const byte *vramPtr =
			vram->bitmapCacheWindow.readArea(vramLine << 7);
		bitmapConverter.convertLine(lineBuffer, vramPtr);
		bitmapTextures[vramLine].update(lineBuffer, lineWidth);
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
		const byte *vramPtr0 =
			vram->bitmapCacheWindow.readArea(addr0);
		const byte *vramPtr1 =
			vram->bitmapCacheWindow.readArea(addr1);
		bitmapConverter.convertLinePlanar(lineBuffer, vramPtr0, vramPtr1);
		bitmapTextures[vramLine].update(lineBuffer, lineWidth);
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

SDLGLRenderer::SDLGLRenderer(
	RendererFactory::RendererID id, VDP *vdp, SDL_Surface *screen )
	: PixelRenderer(id, vdp)
	, dirtyPattern(1 << 10, "pattern")
	, dirtyColour(1 << 10, "colour")
	, characterConverter(vdp, palFg, palBg)
	, bitmapConverter(palFg, PALETTE256, V9958_COLOURS)
	, spriteConverter(vdp->getSpriteChecker())
{
	this->screen = screen;
	console = new GLConsole(CommandConsole::instance());
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
	glDrawBuffer(GL_FRONT);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawBuffer(GL_BACK);
	glClear(GL_COLOR_BUFFER_BIT);
	frameDirty = false;

	// Init OpenGL settings.
	glViewport(0, 0, WIDTH, HEIGHT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, WIDTH, HEIGHT, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Create character display cache.
	// Block based:
	glGenTextures(4 * 256, characterCache);
	for (int i = 0; i < 4 * 256; i++) {
		glBindTexture(GL_TEXTURE_2D, characterCache[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	// Stored image:
	glGenTextures(1, &storedImageTextureId);
	glBindTexture(GL_TEXTURE_2D, storedImageTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	prevStored = false;

	// create noise texture.
	byte buf[128 * 128];
	for (int i = 0; i < 128 * 128; ++i) {
		buf[i] = (byte)rand();
	}
	glGenTextures(1, &noiseTextureId);
	glBindTexture(GL_TEXTURE_2D, noiseTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 128, 128, 0,
	             GL_LUMINANCE, GL_UNSIGNED_BYTE, buf);

	// Create bitmap display cache.
	bitmapTextures = vdp->isMSX1VDP() ? NULL : new LineTexture[4 * 256];

	// Hide mouse cursor.
	SDL_ShowCursor(SDL_DISABLE);

	// Init the palette.
	precalcPalette(settings.getGamma()->getValue());

	// Register caches with VDPVRAM.
	vram->patternTable.setObserver(&dirtyPattern);
	vram->colourTable.setObserver(&dirtyColour);

#ifdef __WIN32__
	// Find our current location
	HWND handle = GetActiveWindow();
	RECT windowRect;
	GetWindowRect (handle, &windowRect);
	// and adjust if needed
	if ((windowRect.right < 0) || (windowRect.bottom < 0)){
		SetWindowPos(handle, HWND_TOP,lastWindowX,lastWindowY,0,0,SWP_NOSIZE);
	}
#endif
}

SDLGLRenderer::~SDLGLRenderer()
{
#ifdef __WIN32__
	// Find our current location
	if ((screen->flags & SDL_FULLSCREEN) == 0){
		HWND handle = GetActiveWindow();
		RECT windowRect;
		GetWindowRect (handle, &windowRect);
		lastWindowX = windowRect.left;
		lastWindowY = windowRect.top;
	}
#endif	
	
	// Unregister caches with VDPVRAM.
	vram->patternTable.resetObserver();
	vram->colourTable.resetObserver();

	delete console;
	// TODO: Free all textures.
	delete[] bitmapTextures;

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void SDLGLRenderer::precalcPalette(float gamma)
{
	prevGamma = gamma;

	// It's gamma correction, so apply in reverse.
	gamma = 1.0 / gamma;

	if (vdp->isMSX1VDP()) {
		// Fixed palette.
		for (int i = 0; i < 16; i++) {
			const byte *rgb = TMS99X8A_PALETTE[i];
			palFg[i] = palBg[i] = GLMapRGB(
				(int)(::pow((float)rgb[0] / 255.0, gamma) * 255),
				(int)(::pow((float)rgb[1] / 255.0, gamma) * 255),
				(int)(::pow((float)rgb[2] / 255.0, gamma) * 255)
				);
		}
	} else {
		// Precalculate palette for V9938 colours.
		for (int r = 0; r < 8; r++) {
			for (int g = 0; g < 8; g++) {
				for (int b = 0; b < 8; b++) {
					V9938_COLOURS[r][g][b] = GLMapRGB(
						(int)(::pow((float)r / 7.0, gamma) * 255),
						(int)(::pow((float)g / 7.0, gamma) * 255),
						(int)(::pow((float)b / 7.0, gamma) * 255)
						);
				}
			}
		}
		// Precalculate palette for V9958 colours.
		for (int r = 0; r < 32; r++) {
			for (int g = 0; g < 32; g++) {
				for (int b = 0; b < 32; b++) {
					V9958_COLOURS[(r<<10) + (g<<5) + b] = GLMapRGB(
						(int)(::pow((float)r / 31.0, gamma) * 255),
						(int)(::pow((float)g / 31.0, gamma) * 255),
						(int)(::pow((float)b / 31.0, gamma) * 255)
						);
				}
			}
		}
		// Precalculate Graphic 7 bitmap palette.
		for (int i = 0; i < 256; i++) {
			PALETTE256[i] = V9938_COLOURS
				[(i & 0x1C) >> 2]
				[(i & 0xE0) >> 5]
				[(i & 0x03) == 3 ? 7 : (i & 0x03) * 2];
		}
		// Precalculate Graphic 7 sprite palette.
		for (int i = 0; i < 16; i++) {
			word grb = GRAPHIC7_SPRITE_PALETTE[i];
			palGraphic7Sprites[i] =
				V9938_COLOURS[(grb >> 4) & 7][grb >> 8][grb & 7];
		}
	}
}

void SDLGLRenderer::reset(const EmuTime &time)
{
	PixelRenderer::reset(time);

	// Init renderer state.
	setDisplayMode(vdp->getDisplayMode());
	spriteConverter.setTransparency(vdp->getTransparency());

	// Invalidate bitmap cache.
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));

	resetPalette();
}

void SDLGLRenderer::resetPalette()
{
	if (!vdp->isMSX1VDP()) {
		// Reset the palette.
		for (int i = 0; i < 16; i++) {
			setPalette(i, vdp->getPalette(i));
		}
	}
}

bool SDLGLRenderer::checkSettings() {
	// First check this is the right renderer.
	if (!PixelRenderer::checkSettings()) return false;

	// Check full screen setting.
	bool fullScreenState = ((screen->flags & SDL_FULLSCREEN) != 0);
	bool fullScreenTarget = settings.getFullScreen()->getValue();
	if (fullScreenState == fullScreenTarget) return true;

#ifdef __WIN32__
	// Under win32, toggling full screen requires opening a new SDL screen.
	return false;
#else
	// Try to toggle full screen.
	SDL_WM_ToggleFullScreen(screen);
	fullScreenState =
		((((volatile SDL_Surface*)screen)->flags & SDL_FULLSCREEN) != 0);
	return fullScreenState == fullScreenTarget;
#endif
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

	float gamma = settings.getGamma()->getValue();
	// (gamma != prevGamma) gives compiler warnings
	if ((gamma > prevGamma) || (gamma < prevGamma)) {
		precalcPalette(gamma);
		resetPalette();
	}
}

void SDLGLRenderer::setDisplayMode(DisplayMode mode)
{
	if (mode.isBitmapMode()) {
		bitmapConverter.setDisplayMode(mode);
	} else {
		characterConverter.setDisplayMode(mode);
		if (characterCacheMode != mode) {
			characterCacheMode = mode;
			dirtyPattern.flush();
			dirtyColour.flush();
		}
	}
	lineWidth = mode.getLineWidth();
	precalcColourIndex0(mode, vdp->getTransparency());
	spriteConverter.setDisplayMode(mode);
	spriteConverter.setPalette(
		mode.getByte() == DisplayMode::GRAPHIC7 ? palGraphic7Sprites : palBg
		);
}

void SDLGLRenderer::updateTransparency(
	bool enabled, const EmuTime &time)
{
	sync(time);
	spriteConverter.setTransparency(enabled);
	precalcColourIndex0(vdp->getDisplayMode(), enabled);
}

void SDLGLRenderer::updateForegroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
}

void SDLGLRenderer::updateBackgroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	if (vdp->getTransparency()) {
		// Transparent pixels have background colour.
		palFg[0] = palBg[colour];
		// Any line containing pixels of colour 0 must be repainted.
		// We don't know which lines contain such pixels,
		// so we have to repaint them all.
		dirtyColour.flush();
		memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
	}
}

void SDLGLRenderer::updateBlinkForegroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
}

void SDLGLRenderer::updateBlinkBackgroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
}

void SDLGLRenderer::updateBlinkState(
	bool enabled, const EmuTime &time)
{
	// TODO: When the sync call is enabled, the screen flashes on
	//       every call to this method.
	//       I don't know why exactly, but it's probably related to
	//       being called at frame start.
	//sync(time);
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
	// Update GL colour in palette.
	palFg[index] = palBg[index] =
		V9938_COLOURS[(grb >> 4) & 7][grb >> 8][grb & 7];

	// Is this the background colour?
	if (vdp->getTransparency() && vdp->getBackgroundColour() == index) {
		// Transparent pixels have background colour.
		precalcColourIndex0(vdp->getDisplayMode());
		// Note: Relies on the fact that precalcColourIndex0 flushes the cache.
	} else {
		// Any line containing pixels of this colour must be repainted.
		// We don't know which lines contain which colours,
		// so we have to repaint them all.
		dirtyColour.flush();
		memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
	}
}

void SDLGLRenderer::precalcColourIndex0(DisplayMode mode, bool transparency) {
	// Graphic7 mode doesn't use transparency.
	if (mode.getByte() == DisplayMode::GRAPHIC7) {
		transparency = false;
	}
	
	int bgColour = 0;
	if (transparency) {
		bgColour = vdp->getBackgroundColour();
		if (mode.getBase() == DisplayMode::GRAPHIC5) {
			// TODO: Transparent pixels should be rendered in separate
			//       colours for even/odd x, just like the border.
			bgColour &= 0x03;
		}
	}
	palFg[0] = palBg[bgColour];
	
	// Any line containing pixels of colour 0 must be repainted.
	// We don't know which lines contain such pixels,
	// so we have to repaint them all.
	dirtyColour.flush();
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
	sync(time, true);
	setDisplayMode(mode);
}

void SDLGLRenderer::updateNameBase(
	int addr, const EmuTime &time)
{
	sync(time);
}

void SDLGLRenderer::updatePatternBase(
	int addr, const EmuTime &time)
{
	sync(time);
}

void SDLGLRenderer::updateColourBase(
	int addr, const EmuTime &time)
{
	sync(time);
}

void SDLGLRenderer::updateVRAMCache(int address)
{
	lineValidInMode[address >> 7] = 0xFF;
}

void SDLGLRenderer::drawBorder(int fromX, int fromY, int limitX, int limitY)
{
	GLFillBlock(
		translateX(fromX), (fromY - lineRenderTop) * 2,
		translateX(limitX), (limitY - lineRenderTop) * 2,
		getBorderColour()
		);
}

void SDLGLRenderer::renderText1(
	int vramLine, int screenLine, int count, int minX, int maxX
) {
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];
	GLSetTexEnvCol(fg);

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
			int charcode = vram->nameTable.readNP((name + 0xC00) | (-1 << 12));
			GLuint textureId = characterCache[charcode];
			if (!dirtyPattern.validate(charcode)) {
				// Update cache for current character.
				byte charPixels[8 * 8];
				characterConverter.convertMonoBlock(
					charPixels,
					vram->patternTable.readArea((-1 << 11) | (charcode * 8))
					);
				GLBindMonoBlock(textureId, charPixels);
			}
			// Plot current character.
			GLDrawMonoBlock(
				textureId,
				leftBackground + col * 12, screenLine, 6, 2,
				bg, verticalScroll
				);
		}
		screenLine += 16;
	}
	glDisable(GL_SCISSOR_TEST);
}

void SDLGLRenderer::renderText2(
	int vramLine, int screenLine, int count, int minX, int maxX
) {
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);

	int leftBackground = translateX(vdp->getLeftBackground());

	// Render complete characters and cut off the invisible part.
	int screenHeight = 2 * count;
	glScissor(
		leftBackground + minX, HEIGHT - screenLine - screenHeight,
		maxX - minX, screenHeight
		);
	glEnable(GL_SCISSOR_TEST);

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
	GLSetTexEnvCol(plainFg);
	bool prevBlink = false;

	int begCol = minX / 6;
	int endCol = (maxX + 5) / 6;
	int endRow = (vramLine + count + 7) / 8;
	screenLine -= (vramLine & 7) * 2;
	int verticalScroll = vdp->getVerticalScroll();
	for (int row = vramLine / 8; row < endRow; row++) {
		for (int col = begCol; col < endCol; col++) {
			// TODO: Only bind texture once?
			//       Currently both subroutines bind the same texture.
			int charcode = vram->nameTable.readNP(
				(-1 << 12) | (row * 80 + col) );
			GLuint textureId = characterCache[charcode];
			if (!dirtyPattern.validate(charcode)) {
				// Update cache for current character.
				byte charPixels[8 * 8];
				characterConverter.convertMonoBlock(
					charPixels,
					vram->patternTable.readArea((-1 << 11) | (charcode * 8))
					);
				GLBindMonoBlock(textureId, charPixels);
			}
			// Plot current character.
			int colourPattern =
				vram->colourTable.readNP((-1 << 9) | (row * 10 + col / 8));
			bool blink = (colourPattern << (col & 7)) & 0x80;
			if (blink != prevBlink) {
				GLSetTexEnvCol(blink ? blinkFg : plainFg);
				prevBlink = blink;
			}
			GLDrawMonoBlock(
				textureId,
				leftBackground + col * 6, screenLine, 6, 1,
				blink ? blinkBg : plainBg, verticalScroll
				);
		}
		screenLine += 16;
	}
	glDisable(GL_SCISSOR_TEST);
}

void SDLGLRenderer::renderGraphic1(
	int vramLine, int screenLine, int count, int minX, int maxX
) {
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);

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
		renderGraphic1Row(row & 31, screenLine, col, endCol);
		screenLine += 16;
	}
	glDisable(GL_SCISSOR_TEST);
}

void SDLGLRenderer::renderGraphic1Row(
	int row, int screenLine, int col, int endCol
) {
	int nameStart = row * 32 + col;
	int nameEnd = row * 32 + endCol;
	int x = translateX(vdp->getLeftBackground()) + col * 16;

	for (int name = nameStart; name < nameEnd; name++) {
		int charNr = vram->nameTable.readNP((-1 << 10) | name);
		GLuint textureId = characterCache[charNr];
		bool valid = dirtyPattern.validate(charNr);
		if (!valid) {
			byte charPixels[8 * 8];
			characterConverter.convertMonoBlock(
				charPixels,
				vram->patternTable.readArea((-1 << 11) | (charNr * 8))
				);
			GLBindMonoBlock(textureId, charPixels);
		}
		int colour = vram->colourTable.readNP((-1 << 6) | (charNr / 8));
		Pixel fg = palFg[colour >> 4];
		Pixel bg = palFg[colour & 0x0F];
		GLSetTexEnvCol(fg);
		GLDrawMonoBlock(
			textureId,
			x, screenLine, 8, 2,
			bg, 0
			);
		x += 16;
	}
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
	int patternMask = (vram->patternTable.getMask() / 8) & 0x3FF;
	int colourMask = (vram->colourTable.getMask() / 8) & 0x3FF;
	int x = translateX(vdp->getLeftBackground()) + col * 16;

	// Note:
	// Because cached tiles depend on both the pattern table and the colour
	// table, cache validation is only simple if both tables use the same
	// mirroring. To keep the code simple, I disabled caching when they are
	// mirrored differently. In practice, this is very rare.
	bool useCache = patternMask == colourMask;

	for (int name = nameStart; name < nameEnd; name++) {
		int charNr = quarter | vram->nameTable.readNP((-1 << 10) | name);
		bool valid;
		if (useCache) {
			// If mirrored, use lowest-numbered character that looks the same,
			// to make optimal use of cache.
			charNr &= patternMask;
			valid = dirtyPattern.validate(charNr);
			valid &= dirtyColour.validate(charNr);
		} else {
			valid = false;
		}
		GLuint textureId = characterCache[charNr];

		// Print cache stats, approx once per frame.
		/*
		static int hits = 0;
		static int lookups = 0;
		if (valid) hits++;
		if (++lookups == 1024) {
			printf("cache: %d hits, %d misses\n", hits, lookups - hits);
			hits = lookups = 0;
		}
		*/

		if (!valid) {
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

void SDLGLRenderer::renderMultiColour(
	int vramLine, int screenLine, int count, int minX, int maxX
) {
	int leftBackground = translateX(vdp->getLeftBackground());

	// Render complete characters and cut off the invisible part.
	int screenHeight = 2 * count;
	glScissor(
		leftBackground + minX, // x
		HEIGHT - screenLine - screenHeight, // y
		maxX - minX, // w
		screenHeight // h
		);
	glEnable(GL_SCISSOR_TEST);

	int colStart = minX / 16;
	int colEnd = (maxX + 15) / 16;
	int rowStart = vramLine / 4;
	int rowEnd = (vramLine + count + 3) / 4;
	screenLine -= (vramLine & 3) * 2;
	for (int row = rowStart; row < rowEnd; row++) {
		const byte *namePtr = vram->nameTable.readArea(
			(-1 << 10) | (row / 2) * 32 );
		for (int col = colStart; col < colEnd; col++) {
			int charcode = namePtr[col];
			int colour = vram->patternTable.readNP(
				(-1 << 11) | (charcode * 8) | (row & 7) );
			int x = leftBackground + col * 16;
			GLFillBlock(
				x, screenLine,
				x + 8, screenLine + 8,
				palFg[colour >> 4]
				);
			GLFillBlock(
				x + 8, screenLine,
				x + 16, screenLine + 8,
				palFg[colour & 0x0F]
				);
		}
		screenLine += 8;
	}

	glDisable(GL_SCISSOR_TEST);
}

void SDLGLRenderer::drawDisplay(
	int fromX, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	int screenX = translateX(fromX);
	int screenY = (fromY - lineRenderTop) * 2;
	if (!(settings.getDeinterlace()->getValue())
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
		bool deinterlaced = settings.getDeinterlace()->getValue()
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
				bitmapTextures[vramLine[0]].draw(
					displayX + hScroll, screenX, y + 0, displayWidth, 1
					);
				bitmapTextures[vramLine[1]].draw(
					displayX + hScroll, screenX, y + 1, displayWidth, 1
					);
			} else {
				int firstPageWidth = pageBorder - displayX;
				if (firstPageWidth > 0) {
					bitmapTextures[vramLine[scrollPage1]].draw(
						displayX + hScroll,
						screenX, y,
						firstPageWidth, 2
						);
				} else {
					firstPageWidth = 0;
				}
				if (firstPageWidth < displayWidth) {
					bitmapTextures[vramLine[scrollPage2]].draw(
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
		case DisplayMode::TEXT2:
			renderText2(
				displayY, screenY, displayHeight,
				displayX, displayX + displayWidth
				);
			break;
		case DisplayMode::GRAPHIC1:
			// TODO: Implement horizontal scroll high.
			renderGraphic1(
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
		case DisplayMode::MULTICOLOUR:
			glDisable(GL_TEXTURE_2D);
			renderMultiColour(
				displayY, screenY, displayHeight,
				displayX, displayX + displayWidth
				);
			break;
		default:
			// TEXT1Q / MULTIQ / BOGUS
			LineTexture charTexture;
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			for (int y = screenY; y < screenLimitY; y += 2) {
				characterConverter.convertLine(lineBuffer, displayY);
				charTexture.update(lineBuffer, lineWidth);
				charTexture.draw(displayX + hScroll, screenX,
						 y, displayWidth, 2);
				displayY++; // is a byte, so wraps at 256
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
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	int screenX = translateX(vdp->getLeftSprites()) + displayX * 2;
	// TODO: Code duplicated from drawDisplay.
	int screenY = (fromY - lineRenderTop) * 2;
	if (!(settings.getDeinterlace()->getValue())
	&& vdp->isInterlaced()
	&& vdp->getEvenOdd()) {
		// Display odd field half a line lower.
		screenY++;
	}

	int spriteMode = vdp->getDisplayMode().getSpriteMode();
	int displayLimitX = displayX + displayWidth;
	int limitY = fromY + displayHeight;
	byte mode = vdp->getDisplayMode().getByte();
	int pixelZoom =
		(mode == DisplayMode::GRAPHIC5 || mode == DisplayMode::GRAPHIC6)
		? 2 : 1;
	for (int y = fromY; y < limitY; y++) {

		// Buffer to render sprite pixels to; start with all transparent.
		memset(lineBuffer, 0, pixelZoom * 256 * sizeof(Pixel));

		// TODO: Possible speedups:
		// - for mode 1: create a texture in 1bpp, or in luminance
		// - use VRAM to render sprite blocks instead of lines
		if (spriteMode == 1) {
			spriteConverter.drawMode1(y, displayX, displayLimitX, lineBuffer);
		} else {
			spriteConverter.drawMode2(y, displayX, displayLimitX, lineBuffer);
		}

		// Make line buffer into a texture and draw it.
		// TODO: Make a texture of only the portion that will be drawn.
		//       Or skip that and use block sprites instead.
		LineTexture &texture = spriteTextures[y];
		texture.update(lineBuffer, pixelZoom * 256);
		texture.draw(displayX * 2, screenX, screenY, displayWidth * 2, 2);

		screenY += 2;
	}

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

} // namespace openmsx

