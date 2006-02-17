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

#include "GLRasterizer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "Display.hh"
#include "OutputSurface.hh"
#include "Renderer.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "FloatSetting.hh"
#include "IntegerSetting.hh"
#include "build-info.hh"
#include <algorithm>
#include <cmath>

using std::string;

namespace openmsx {

/** VDP ticks between start of line and start of left border.
  */
static const int TICKS_LEFT_BORDER = 100 + 102;

/** The middle of the visible (display + borders) part of a line,
  * expressed in VDP ticks since the start of the line.
  * TODO: Move this to a central location?
  */
static const int TICKS_VISIBLE_MIDDLE =
	TICKS_LEFT_BORDER + (VDP::TICKS_PER_LINE - TICKS_LEFT_BORDER - 27) / 2;

inline static GLRasterizer::Pixel GLMapRGB(double dr, double dg, double db)
{
	int r = static_cast<int>(dr * 255.0);
	int g = static_cast<int>(dg * 255.0);
	int b = static_cast<int>(db * 255.0);
	if (OPENMSX_BIGENDIAN) {
		return (r << 24) | (g << 16) | (b <<  8) | 0x000000FF;
	} else {
		return (r <<  0) | (g <<  8) | (b << 16) | 0xFF000000;
	}
}

inline static void GLSetColour(GLRasterizer::Pixel colour)
{
	if (OPENMSX_BIGENDIAN) {
		glColor3ub((colour >> 24) & 0xFF,
		           (colour >> 16) & 0xFF,
		           (colour >>  8) & 0xFF);
	} else {
		glColor3ub((colour >>  0) & 0xFF,
		           (colour >>  8) & 0xFF,
		           (colour >> 16) & 0xFF);
	}
}

inline static void GLSetTexEnvCol(GLRasterizer::Pixel colour)
{
	GLfloat colourVec[4] = {
		(colour & 0xFF) / 255.0f,
		((colour >> 8) & 0xFF) / 255.0f,
		((colour >> 16) & 0xFF) / 255.0f,
		1.0f
	};
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, colourVec);
}

inline static void GLFillBlock(int x1, int y1, int x2, int y2,
                               GLRasterizer::Pixel colour)
{
	GLSetColour(colour);
	glRecti(x1, y1, x2, y2);
}

inline static void GLBindMonoBlock(GLuint textureId, const byte* pixels)
{
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexSubImage2D(GL_TEXTURE_2D,    // target
	                0,                // level
	                0,                // x-offset
	                0,                // y-offset
	                8,                // width
	                8,                // height
	                GL_LUMINANCE,     // format
	                GL_UNSIGNED_BYTE, // type
	                pixels);          // data
}

inline static void GLBindColourBlock(
	GLuint textureId, const GLRasterizer::Pixel *pixels)
{
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexSubImage2D(GL_TEXTURE_2D,    // target
	                0,                // level
	                0,                // x-offset
	                0,                // y-offset
	                8,                // width
	                8,                // height
	                GL_RGBA,          // format
	                GL_UNSIGNED_BYTE, // type
	                pixels);          // data
}

inline static void GLDrawMonoBlock(
	GLuint textureId, int x, int y, int width, int zoom,
	GLRasterizer::Pixel bg, int verticalScroll)
{
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

inline static void drawStripes(
	int x1, int y1, int x2, int y2,
	GLRasterizer::Pixel col0, GLRasterizer::Pixel col1,
	GLuint textureId)
{
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
	GLSetTexEnvCol(col0);
	GLSetColour(col1);
	glBegin(GL_QUADS);
	glTexCoord2f(x1 / 2.0f, 1.0f); glVertex2i(x1, y2); // Bottom Left
	glTexCoord2f(x2 / 2.0f, 1.0f); glVertex2i(x2, y2); // Bottom Right
	glTexCoord2f(x2 / 2.0f, 0.0f); glVertex2i(x2, y1); // Top Right
	glTexCoord2f(x1 / 2.0f, 0.0f); glVertex2i(x1, y1); // Top Left
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

/** Translate from absolute VDP coordinates to output coordinates.
  * TODO: Use fixed width 640 (artificial coordinate system) or use screen
  *       coordinates (which depend on scale_factor)?
  * Note: In reality, there are only 569.5 visible pixels on a line.
  *       Because it looks better, the borders are extended to 640.
  */
inline static int translateX(int absoluteX)
{
	static const int WIDTH = 640;
	if (absoluteX == VDP::TICKS_PER_LINE) return WIDTH;
	// Note: The "& ~1" forces the ticks to a pixel (2-tick) boundary.
	//       If this is not done, rounding errors will occur.
	//       This is especially tricky because division of a negative number
	//       is rounded towards zero instead of down.
	int screenX = WIDTH / 2 +
		((absoluteX & ~1) - (TICKS_VISIBLE_MIDDLE & ~1)) / 2;
	return screenX < 0 ? 0 : screenX;
}

inline void GLRasterizer::renderBitmapLine(byte mode, int vramLine)
{
	if (lineValidInMode[vramLine] != mode) {
		const byte *vramPtr =
			vram.bitmapCacheWindow.readArea(vramLine << 7);
		bitmapConverter.convertLine(lineBuffer, vramPtr);
		bitmapTexture->update(vramLine, lineBuffer, lineWidth);
		lineValidInMode[vramLine] = mode;
	}
}

inline void GLRasterizer::renderBitmapLines(byte line, int count)
{
	byte mode = vdp.getDisplayMode().getByte();
	// Which bits in the name mask determine the page?
	int pageMask = 0x200 | vdp.getEvenOddMask();
	while (count--) {
		// TODO: Optimise addr and line; too many conversions right now.
		int vramLine = (vram.nameTable.getMask() >> 7) & (pageMask | line);
		renderBitmapLine(mode, vramLine);
		if (vdp.isMultiPageScrolling()) {
			vramLine &= ~0x100;
			renderBitmapLine(mode, vramLine);
		}
		line++; // is a byte, so wraps at 256
	}
}

inline void GLRasterizer::renderPlanarBitmapLine(byte mode, int vramLine)
{
	if ((lineValidInMode[vramLine      ] != mode) ||
	    (lineValidInMode[vramLine | 512] != mode)) {
		int addr0 = vramLine << 7;
		int addr1 = addr0 | 0x10000;
		const byte* vramPtr0 = vram.bitmapCacheWindow.readArea(addr0);
		const byte* vramPtr1 = vram.bitmapCacheWindow.readArea(addr1);
		bitmapConverter.convertLinePlanar(lineBuffer, vramPtr0, vramPtr1);
		bitmapTexture->update(vramLine, lineBuffer, lineWidth);
		lineValidInMode[vramLine] =
			lineValidInMode[vramLine | 512] = mode;
	}
}

inline void GLRasterizer::renderPlanarBitmapLines(byte line, int count)
{
	byte mode = vdp.getDisplayMode().getByte();
	// Which bits in the name mask determine the page?
	int pageMask = vdp.getEvenOddMask();
	while (count--) {
		// TODO: Optimise addr and line; too many conversions right now.
		int vramLine = (vram.nameTable.getMask() >> 7) & (pageMask | line);
		renderPlanarBitmapLine(mode, vramLine);
		if (vdp.isMultiPageScrolling()) {
			vramLine &= ~0x100;
			renderPlanarBitmapLine(mode, vramLine);
		}
		line++; // is a byte, so wraps at 256
	}
}

GLRasterizer::GLRasterizer(
		CommandController& commandController,
		VDP& vdp_, Display& display, OutputSurface& screen_
		)
	: VideoLayer(VIDEO_MSX, commandController, display)
	, renderSettings(display.getRenderSettings())
	, vdp(vdp_), vram(vdp.getVRAM())
	, screen(screen_)
	, characterConverter(vdp, palFg, palBg)
	, bitmapConverter(palFg, PALETTE256, V9958_COLOURS)
	, spriteConverter(vdp.getSpriteChecker())
{
	GLint size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
	//printf("Max texture size: %d\n", size);
	glTexImage2D(GL_PROXY_TEXTURE_2D,
	             0,
	             GL_RGBA,
	             512,
	             1,
	             0,
	             GL_RGBA,
	             GL_UNSIGNED_BYTE,
	             NULL);
	size = -1;
	glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D,
	                         0,
	                         GL_TEXTURE_WIDTH,
	                         &size);
	//printf("512*1 texture fits: %d (512 if OK, 0 if failed)\n", size);

	// Clear graphics buffers.
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glDrawBuffer(GL_FRONT);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawBuffer(GL_BACK);
	glClear(GL_COLOR_BUFFER_BIT);
	frameDirty = false;

	// Create character display cache.
	// Block based:
	glGenTextures(4 * 256, colorChrTex);
	for (int i = 0; i < 4 * 256; ++i) {
		byte dummy[8 * 8 * 4];
		glBindTexture(GL_TEXTURE_2D, colorChrTex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D,    // target
			     0,                // level
			     GL_RGBA,          // internal format
			     8,                // width
			     8,                // height
			     0,                // border
			     GL_RGBA,          // format
			     GL_UNSIGNED_BYTE, // type
			     dummy);           // data
	}
	glGenTextures(256, monoChrTex);
	for (int i = 0; i < 256; ++i) {
		byte dummy[8 * 8];
		glBindTexture(GL_TEXTURE_2D, monoChrTex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D,    // target
			     0,                // level
			     GL_LUMINANCE8,    // internal format
			     8,                // width
			     8,                // height
			     0,                // border
			     GL_LUMINANCE,     // format
			     GL_UNSIGNED_BYTE, // type
			     dummy);           // data
	}

	// texture for drawing stripes
	glGenTextures(1, &stripeTexture);
	glBindTexture(GL_TEXTURE_2D, stripeTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	byte pattern[2] = { 0xFF, 0x00 };
	glTexImage2D(GL_TEXTURE_2D,
	             0, // level
	             GL_LUMINANCE8,
	             2, // width
	             1, // height
	             0, // border
	             GL_LUMINANCE,
	             GL_UNSIGNED_BYTE,
	             pattern);

	// Create bitmap display cache.
	bitmapTexture = vdp.isMSX1VDP() ? NULL : new BitmapTexture();

	// Init the palette.
	precalcPalette();

	// Initialize palette (avoid UMR)
	for (int i = 0; i < 16; ++i) {
		palFg[i] = palFg[i + 16] = palBg[i] = V9938_COLOURS[0][0][0];
	}

	// Store current (black) frame as a texture.
	storedFrame.store(screen.getWidth(), screen.getHeight());

	// Register caches with VDPVRAM.
	vram.patternTable.setObserver(&dirtyPattern);
	vram.colourTable.setObserver(&dirtyColour);

	renderSettings.getGamma()     .attach(*this);
	renderSettings.getBrightness().attach(*this);
	renderSettings.getContrast()  .attach(*this);
	renderSettings.getNoise()     .attach(*this);

	glGenTextures(2, noiseTextures);
	glBindTexture(GL_TEXTURE_2D, noiseTextures[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 256, 256, 0,
	             GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, noiseTextures[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 256, 256, 0,
	             GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

	noiseSeq = 0;
	preCalcNoise(renderSettings.getNoise().getValue());
}

GLRasterizer::~GLRasterizer()
{
	glDeleteTextures(2, noiseTextures);

	renderSettings.getNoise()     .detach(*this);
	renderSettings.getGamma()     .detach(*this);
	renderSettings.getBrightness().detach(*this);
	renderSettings.getContrast()  .detach(*this);

	// Unregister caches with VDPVRAM.
	vram.patternTable.resetObserver();
	vram.colourTable.resetObserver();

	// TODO: Free all textures.
	delete bitmapTexture;
}

static void gaussian2(double& r1, double& r2)
{
	static const double S = 2.0 / RAND_MAX;
	double x1, x2, w;
	do {
		x1 = S * rand() - 1.0;
		x2 = S * rand() - 1.0;
		w = x1 * x1 + x2 * x2;
	} while (w >= 1.0);
	w = sqrt((-2.0 * log(w)) / w);
	r1 = x1 * w;
	r2 = x2 * w;
}
static int clip(double r, double factor)
{
	int a = (int)round(r * factor);
	return std::min(std::max(a, -255), 255);
}

void GLRasterizer::preCalcNoise(double factor)
{
	byte buf1[256 * 256];
	byte buf2[256 * 256];
	for (int i = 0; i < 256 * 256; i += 2) {
		double r1, r2;
		gaussian2(r1, r2);
		int s1 = clip(r1, factor);
		buf1[i + 0] = (s1 > 0) ?  s1 : 0;
		buf2[i + 0] = (s1 < 0) ? -s1 : 0;
		int s2 = clip(r2, factor);
		buf1[i + 1] = (s2 > 0) ?  s2 : 0;
		buf2[i + 1] = (s2 < 0) ? -s2 : 0;
	}
	glBindTexture(GL_TEXTURE_2D, noiseTextures[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, 
	             GL_LUMINANCE, GL_UNSIGNED_BYTE, buf1);
	glBindTexture(GL_TEXTURE_2D, noiseTextures[1]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, 
	             GL_LUMINANCE, GL_UNSIGNED_BYTE, buf2);
}

bool GLRasterizer::isActive()
{
	return getZ() != Layer::Z_MSX_PASSIVE;
}

void GLRasterizer::reset()
{
	// Init renderer state.
	setDisplayMode(vdp.getDisplayMode());
	spriteConverter.setTransparency(vdp.getTransparency());

	// Invalidate bitmap cache.
	dirtyColour.flush();
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));

	resetPalette();
}

void GLRasterizer::resetPalette()
{
	if (!vdp.isMSX1VDP()) {
		// Reset the palette.
		for (int i = 0; i < 16; i++) {
			setPalette(i, vdp.getPalette(i));
		}
	}
}

void GLRasterizer::frameStart()
{
	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	lineRenderTop = vdp.isPalTiming() ? 59 - 14 : 32 - 14;
}

void GLRasterizer::frameEnd()
{
	drawNoise();

	// Glow effect.
	// Must be applied before storedImage is updated.
	int glowSetting = renderSettings.getGlow().getValue();
	if (glowSetting != 0 && storedFrame.isStored()) {
		// Note:
		// 100% glow means current frame has no influence at all.
		// Values near 100% may have the same effect due to rounding.
		// This formula makes sure that on 15bpp R/G/B value 0 can still pull
		// down R/G/B value 31 of the previous frame.
		storedFrame.drawBlend(
			0, 0, screen.getWidth(), screen.getHeight(),
			glowSetting * 31 / 3200.0
			);
	}

	// Store current frame as a texture.
	storedFrame.store(screen.getWidth(), screen.getHeight());

	// Avoid repainting the buffer by paint().
	frameDirty = false;
}

void GLRasterizer::drawNoise()
{
	if (renderSettings.getNoise().getValue() == 0) return;

	// rotate and mirror noise texture in consecutive frames to avoid
	// seeing 'patterns' in the noise
	static const int coord[8][4][2] = {
		{ {   0,   0 }, { 640,   0 }, { 640, 480 }, {   0, 480 } },
		{ {   0, 480 }, { 640, 480 }, { 640,   0 }, {   0,   0 } },
		{ {   0, 480 }, {   0,   0 }, { 640,   0 }, { 640, 480 } },
		{ { 640, 480 }, { 640,   0 }, {   0,   0 }, {   0, 480 } },
		{ { 640, 480 }, {   0, 480 }, {   0,   0 }, { 640,   0 } },
		{ { 640,   0 }, {   0,   0 }, {   0, 480 }, { 640, 480 } },
		{ { 640,   0 }, { 640, 480 }, {   0, 480 }, {   0,   0 } },
		{ {   0,   0 }, {   0, 480 }, { 640, 480 }, { 640,   0 } }
	};
	
	double x = (double)rand() / RAND_MAX;
	double y = (double)rand() / RAND_MAX;
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendEquation(GL_FUNC_ADD);
	glBindTexture(GL_TEXTURE_2D, noiseTextures[0]);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f + x, 1.875f + y);
	glVertex2i(coord[noiseSeq][0][0], coord[noiseSeq][0][1]);
	glTexCoord2f(2.5f + x, 1.875f + y);
	glVertex2i(coord[noiseSeq][1][0], coord[noiseSeq][1][1]);
	glTexCoord2f(2.5f + x, 0.000f + y);
	glVertex2i(coord[noiseSeq][2][0], coord[noiseSeq][2][1]);
	glTexCoord2f(0.0f + x, 0.000f + y);
	glVertex2i(coord[noiseSeq][3][0], coord[noiseSeq][3][1]);
	glEnd();
	glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
	glBindTexture(GL_TEXTURE_2D, noiseTextures[1]);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f + x, 1.875f + y);
	glVertex2i(coord[noiseSeq][0][0], coord[noiseSeq][0][1]);
	glTexCoord2f(2.5f + x, 1.875f + y);
	glVertex2i(coord[noiseSeq][1][0], coord[noiseSeq][1][1]);
	glTexCoord2f(2.5f + x, 0.000f + y);
	glVertex2i(coord[noiseSeq][2][0], coord[noiseSeq][2][1]);
	glTexCoord2f(0.0f + x, 0.000f + y);
	glVertex2i(coord[noiseSeq][3][0], coord[noiseSeq][3][1]);
	glEnd();
	noiseSeq = (noiseSeq + 1) & 7;
	glPopAttrib();
}

void GLRasterizer::setDisplayMode(DisplayMode mode)
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
	precalcColourIndex0(mode, vdp.getTransparency(),
	                    vdp.getBackgroundColour());
	spriteConverter.setDisplayMode(mode);
	spriteConverter.setPalette(mode.getByte() == DisplayMode::GRAPHIC7 ?
	                           palGraphic7Sprites : palBg);
}

void GLRasterizer::setPalette(
	int index, int grb)
{
	// Update GL colour in palette.
	Pixel newColor = V9938_COLOURS[(grb >> 4) & 7][grb >> 8][grb & 7];
	if ((palFg[index     ] != newColor) ||
	    (palFg[index + 16] != newColor) ||
	    (palBg[index     ] != newColor)) {
		palFg[index     ] = newColor;
		palFg[index + 16] = newColor;
		palBg[index     ] = newColor;
		// Any line containing pixels of this colour must be repainted.
		// We don't know which lines contain which colours,
		// so we have to repaint them all.
		dirtyColour.flush();
		memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
	}

	precalcColourIndex0(vdp.getDisplayMode(), vdp.getTransparency(),
	                    vdp.getBackgroundColour());
}

void GLRasterizer::setBackgroundColour(int index)
{
	precalcColourIndex0(
		vdp.getDisplayMode(), vdp.getTransparency(), index);
}

void GLRasterizer::setTransparency(bool enabled)
{
	spriteConverter.setTransparency(enabled);
	precalcColourIndex0(
		vdp.getDisplayMode(), enabled, vdp.getBackgroundColour());
}

void GLRasterizer::precalcPalette()
{
	if (vdp.isMSX1VDP()) {
		// Fixed palette.
		for (int i = 0; i < 16; i++) {
			const byte *rgb = Renderer::TMS99X8A_PALETTE[i];
			double dr = rgb[0] / 255.0;
			double dg = rgb[1] / 255.0;
			double db = rgb[2] / 255.0;
			renderSettings.transformRGB(dr, dg, db);
			palFg[i] = palFg[i + 16] = palBg[i] =
				GLMapRGB(dr, dg, db);
		}
	} else {
		// Precalculate palette for V9938 colours.
		for (int r = 0; r < 8; r++) {
			for (int g = 0; g < 8; g++) {
				for (int b = 0; b < 8; b++) {
					double dr = r / 7.0;
					double dg = g / 7.0;
					double db = b / 7.0;
					renderSettings.transformRGB(dr, dg, db);
					V9938_COLOURS[r][g][b] =
						GLMapRGB(dr, dg, db);
				}
			}
		}
		// Precalculate palette for V9958 colours.
		for (int r = 0; r < 32; r++) {
			for (int g = 0; g < 32; g++) {
				for (int b = 0; b < 32; b++) {
					double dr = r / 31.0;
					double dg = g / 31.0;
					double db = b / 31.0;
					renderSettings.transformRGB(dr, dg, db);
					V9958_COLOURS[(r<<10) + (g<<5) + b] =
						GLMapRGB(dr, dg, db);
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
			word grb = Renderer::GRAPHIC7_SPRITE_PALETTE[i];
			palGraphic7Sprites[i] =
				V9938_COLOURS[(grb >> 4) & 7][grb >> 8][grb & 7];
		}
	}
}

void GLRasterizer::precalcColourIndex0(
	DisplayMode mode, bool transparency, byte bgcolorIndex)
{
	// Graphic7 mode doesn't use transparency.
	if (mode.getByte() == DisplayMode::GRAPHIC7) {
		transparency = false;
	}

	int tpIndex = transparency ? bgcolorIndex : 0;
	if (mode.getBase() != DisplayMode::GRAPHIC5) {
		if (palFg[0] != palBg[tpIndex]) {
			palFg[0] = palBg[tpIndex];

			// Any line containing pixels of colour 0 must be repainted.
			// We don't know which lines contain such pixels,
			// so we have to repaint them all.
			dirtyColour.flush();
			memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
		}
	} else {
		if ((palFg[ 0] != palBg[tpIndex >> 2]) ||
		    (palFg[16] != palBg[tpIndex &  3])) {
			palFg[ 0] = palBg[tpIndex >> 2];
			palFg[16] = palBg[tpIndex &  3];

			// Any line containing pixels of colour 0 must be repainted.
			// We don't know which lines contain such pixels,
			// so we have to repaint them all.
			dirtyColour.flush();
			memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
		}
	}
}

void GLRasterizer::updateVRAMCache(int address)
{
	lineValidInMode[address >> 7] = 0xFF;
}

void GLRasterizer::paint()
{
	if (frameDirty) {
		storedFrame.draw(0, 0, screen.getWidth(), screen.getHeight());
	}

	// Determine which effects to apply.
	int blurSetting = renderSettings.getBlurFactor();
	int scanlineAlpha = renderSettings.getScanlineFactor();

	// TODO: Turn off scanlines when deinterlacing.
	bool horizontalBlur = blurSetting != 0;
	bool scanlines = scanlineAlpha != 255;

	// Blur effect.
	if (horizontalBlur || scanlines) {
		// Settings for blur rendering.
		double blurFactor = blurSetting / 512.0;
		if (!scanlines) blurFactor *= 0.5;
		if (horizontalBlur) {
			// Draw stored frame with 1-pixel offsets to create a
			// horizontal blur.
			storedFrame.drawBlend(
				-1,  0, screen.getWidth(), screen.getHeight(),
				blurFactor / (1.0 - blurFactor)
				);
			storedFrame.drawBlend(
				1,  0, screen.getWidth(), screen.getHeight(),
				blurFactor
				);
		}
		if (scanlines) {
			// Make the dark line contain the average of the visible lines
			// above and below it.
			storedFrame.drawBlend(
				0, -1, screen.getWidth(), screen.getHeight(), 0.5
				);
		}
	}

	// Scanlines effect.
	if (scanlines) {
		// TODO: These are always the same lines: use a display list.
		// TODO: If interlace is active, draw scanlines on even/odd lines
		//       for odd/even frames respectively.
		if (scanlineAlpha == 0) {
			glDisable(GL_BLEND);
		} else {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		glColor4ub(0, 0, 0, 255 - scanlineAlpha);
		const float width = screen.getWidth();
		const float height = screen.getHeight();
		const float deltaY = height / 240.0f;
		const float offsetY = deltaY - 0.5f;
		//glLineWidth(height / 480.0f);
		glBegin(GL_LINES);
		for (float y = offsetY; y < height; y += deltaY) {
			glVertex2f(0.0f, y); glVertex2f(width, y);
		}
		glEnd();
		glDisable(GL_BLEND);
	}

	frameDirty = true;
}

const string& GLRasterizer::getName()
{
	static const string NAME = "GLRasterizer";
	return NAME;
}

void GLRasterizer::drawBorder(int fromX, int fromY, int limitX, int limitY)
{
	int x1 = translateX(fromX);
	int x2 = translateX(limitX);
	int y1 = (fromY  - lineRenderTop) * 2;
	int y2 = (limitY - lineRenderTop) * 2;

	byte mode = vdp.getDisplayMode().getByte();
	int bgColor = vdp.getBackgroundColour();

	if (mode != DisplayMode::GRAPHIC5) {
		Pixel col = (mode != DisplayMode::GRAPHIC7)
		          ? palBg[bgColor & 0x0F]
		          : PALETTE256[bgColor];
		GLFillBlock(x1, y1, x2, y2, col);
	} else {
		Pixel col0 = palBg[(bgColor & 0x0C) >> 2];
		Pixel col1 = palBg[(bgColor & 0x03) >> 0];
		drawStripes(x1, y1, x2, y2, col0, col1, stripeTexture);
	}
}

void GLRasterizer::renderText1(
	int vramLine, int screenLine, int count, int minX, int maxX
) {
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
	Pixel fg = palFg[vdp.getForegroundColour()];
	Pixel bg = palBg[vdp.getBackgroundColour()];
	GLSetTexEnvCol(fg);

	int leftBackground = translateX(vdp.getLeftBackground());

	// Render complete characters and cut off the invisible part.
	int screenHeight = 2 * count;
	static const int HEIGHT = 480;
	glScissor(leftBackground + minX, HEIGHT - screenLine - screenHeight,
	          maxX - minX, screenHeight);
	glEnable(GL_SCISSOR_TEST);

	int begCol = minX / 12;
	int endCol = (maxX + 11) / 12;
	int endRow = (vramLine + count + 7) / 8;
	screenLine -= (vramLine & 7) * 2;
	int verticalScroll = vdp.getVerticalScroll();
	for (int row = vramLine / 8; row < endRow; row++) {
		for (int col = begCol; col < endCol; col++) {
			// TODO: Only bind texture once?
			//       Currently both subroutines bind the same texture.
			int name = (row & 31) * 40 + col;
			int charcode = vram.nameTable.readNP((name + 0xC00) | (-1 << 12));
			GLuint textureId = monoChrTex[charcode];
			if (!dirtyPattern.validate(charcode)) {
				// Update cache for current character.
				byte charPixels[8 * 8];
				characterConverter.convertMonoBlock(
					charPixels,
					vram.patternTable.readArea((-1 << 11) | (charcode * 8))
					);
				GLBindMonoBlock(textureId, charPixels);
			}
			// Plot current character.
			GLDrawMonoBlock(textureId,
			                leftBackground + col * 12, screenLine,
			                6, 2, bg, verticalScroll);
		}
		screenLine += 16;
	}
	glDisable(GL_SCISSOR_TEST);
}

void GLRasterizer::renderText2(
	int vramLine, int screenLine, int count, int minX, int maxX)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);

	int leftBackground = translateX(vdp.getLeftBackground());

	// Render complete characters and cut off the invisible part.
	int screenHeight = 2 * count;
	static const int HEIGHT = 480;
	glScissor(leftBackground + minX, HEIGHT - screenLine - screenHeight,
	          maxX - minX, screenHeight);
	glEnable(GL_SCISSOR_TEST);

	Pixel plainFg = palFg[vdp.getForegroundColour()];
	Pixel plainBg = palBg[vdp.getBackgroundColour()];
	Pixel blinkFg, blinkBg;
 	if (vdp.getBlinkState()) {
		int fg = vdp.getBlinkForegroundColour();
		blinkFg = palBg[fg ? fg : vdp.getBlinkBackgroundColour()];
		blinkBg = palBg[vdp.getBlinkBackgroundColour()];
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
	int verticalScroll = vdp.getVerticalScroll();
	for (int row = vramLine / 8; row < endRow; row++) {
		for (int col = begCol; col < endCol; col++) {
			// TODO: Only bind texture once?
			//       Currently both subroutines bind the same texture.
			int charcode = vram.nameTable.readNP(
				(-1 << 12) | (row * 80 + col) );
			GLuint textureId = monoChrTex[charcode];
			if (!dirtyPattern.validate(charcode)) {
				// Update cache for current character.
				byte charPixels[8 * 8];
				characterConverter.convertMonoBlock(
					charPixels,
					vram.patternTable.readArea((-1 << 11) | (charcode * 8)));
				GLBindMonoBlock(textureId, charPixels);
			}
			// Plot current character.
			int colourPattern =
				vram.colourTable.readNP((-1 << 9) | (row * 10 + col / 8));
			bool blink = (colourPattern << (col & 7)) & 0x80;
			if (blink != prevBlink) {
				GLSetTexEnvCol(blink ? blinkFg : plainFg);
				prevBlink = blink;
			}
			GLDrawMonoBlock(textureId,
			                leftBackground + col * 6, screenLine,
			                6, 1,
			                blink ? blinkBg : plainBg,
			                verticalScroll);
		}
		screenLine += 16;
	}
	glDisable(GL_SCISSOR_TEST);
}

void GLRasterizer::renderGraphic1(
	int vramLine, int screenLine, int count, int minX, int maxX
) {
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);

	// Render complete characters and cut off the invisible part.
	int screenHeight = 2 * count;
	static const int HEIGHT = 480;
	glScissor(translateX(vdp.getLeftBackground()) + minX, // x
	          HEIGHT - screenLine - screenHeight,          // y
	          maxX - minX,   // w
	          screenHeight); // h
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

void GLRasterizer::renderGraphic1Row(
	int row, int screenLine, int col, int endCol)
{
	int nameStart = row * 32 + col;
	int nameEnd = row * 32 + endCol;
	int x = translateX(vdp.getLeftBackground()) + col * 16;

	for (int name = nameStart; name < nameEnd; name++) {
		int charNr = vram.nameTable.readNP((-1 << 10) | name);
		GLuint textureId = monoChrTex[charNr];
		bool valid = dirtyPattern.validate(charNr);
		if (!valid) {
			byte charPixels[8 * 8];
			characterConverter.convertMonoBlock(
				charPixels,
				vram.patternTable.readArea((-1 << 11) | (charNr * 8)));
			GLBindMonoBlock(textureId, charPixels);
		}
		int colour = vram.colourTable.readNP((-1 << 6) | (charNr / 8));
		Pixel fg = palFg[colour >> 4];
		Pixel bg = palFg[colour & 0x0F];
		GLSetTexEnvCol(fg);
		GLDrawMonoBlock(textureId, x, screenLine, 8, 2, bg, 0);
		x += 16;
	}
}

void GLRasterizer::renderGraphic2(
	int vramLine, int screenLine, int count, int minX, int maxX)
{
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	// Render complete characters and cut off the invisible part.
	int screenHeight = 2 * count;
	static const int HEIGHT = 480;
	glScissor(translateX(vdp.getLeftBackground()) + minX, // x
	          HEIGHT - screenLine - screenHeight,          // y
	          maxX - minX,   // w
	          screenHeight); // h
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

void GLRasterizer::renderGraphic2Row(
	int row, int screenLine, int col, int endCol)
{
	int nameStart = row * 32 + col;
	int nameEnd = row * 32 + endCol;
	int quarter = nameStart & ~0xFF;
	int patternMask = (vram.patternTable.getMask() / 8) & 0x3FF;
	int colourMask = (vram.colourTable.getMask() / 8) & 0x3FF;
	int x = translateX(vdp.getLeftBackground()) + col * 16;

	// Note:
	// Because cached tiles depend on both the pattern table and the colour
	// table, cache validation is only simple if both tables use the same
	// mirroring. To keep the code simple, I disabled caching when they are
	// mirrored differently. In practice, this is very rare.
	bool useCache = patternMask == colourMask;

	for (int name = nameStart; name < nameEnd; name++) {
		int charNr = quarter | vram.nameTable.readNP((-1 << 10) | name);
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
		GLuint textureId = colorChrTex[charNr];

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
				vram.patternTable.readArea(index),
				vram.colourTable.readArea(index));
			GLBindColourBlock(textureId, charPixels);
		}
		GLDrawColourBlock(textureId, x, screenLine);
		x += 16;
	}
}

void GLRasterizer::renderMultiColour(
	int vramLine, int screenLine, int count, int minX, int maxX)
{
	int leftBackground = translateX(vdp.getLeftBackground());

	// Render complete characters and cut off the invisible part.
	int screenHeight = 2 * count;
	static const int HEIGHT = 480;
	glScissor(leftBackground + minX,              // x
	          HEIGHT - screenLine - screenHeight, // y
	          maxX - minX,   // w
	          screenHeight); // h
	glEnable(GL_SCISSOR_TEST);

	int colStart = minX / 16;
	int colEnd = (maxX + 15) / 16;
	int rowStart = vramLine / 4;
	int rowEnd = (vramLine + count + 3) / 4;
	screenLine -= (vramLine & 3) * 2;
	for (int row = rowStart; row < rowEnd; row++) {
		const byte *namePtr = vram.nameTable.readArea(
			(-1 << 10) | (row / 2) * 32 );
		for (int col = colStart; col < colEnd; col++) {
			int charcode = namePtr[col];
			int colour = vram.patternTable.readNP(
				(-1 << 11) | (charcode * 8) | (row & 7) );
			int x = leftBackground + col * 16;
			GLFillBlock(x, screenLine,
			            x + 8, screenLine + 8,
			            palFg[colour >> 4]);
			GLFillBlock(x + 8, screenLine,
			            x + 16, screenLine + 8,
			            palFg[colour & 0x0F]);
		}
		screenLine += 8;
	}

	glDisable(GL_SCISSOR_TEST);
}

void GLRasterizer::drawDisplay(
	int fromX, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight)
{
	// Determine display mode dependent info.
	const DisplayMode mode = vdp.getDisplayMode();
	const unsigned lineWidth = mode.getLineWidth();
	assert(lineWidth == 256 || lineWidth == 512);
	const unsigned zoom = 512 / lineWidth;

	// Round rectangle to whole source pixels.
	// TODO: It is unlikely the real VDP could apply changes mid-pixel,
	//       so probably this check should be done upstream.
	if (lineWidth == 256 && (displayX & 1) != 0) {
		displayX--;
		displayWidth++;
		fromX -= 2;
	}
	if (lineWidth == 256 && (displayWidth & 1) != 0) {
		displayWidth--;
	}
	assert(zoom == 1 || (displayX & 1) == 0);
	assert(zoom == 1 || (displayWidth & 1) == 0);

	const int screenX = translateX(fromX);
	int screenY = (fromY - lineRenderTop) * 2;
	const int screenLimitY = screenY + displayHeight * 2;

	// TODO: It is possible to apply horizontal scroll in PixelRenderer.
	//       Would that be an improvement? At least it would share the code
	//       between SDL and GL renderers.
	const int hScroll =
		  mode.isTextMode()
		? 0
		: 16 * (vdp.getHorizontalScrollHigh() & 0x1F);

	// Page border is display X coordinate where to stop drawing current page.
	// This is either the multi page split point, or the right edge of the
	// rectangle to draw, whichever comes first.
	// Note that it is possible for pageBorder to be to the left of displayX,
	// in that case only the second page should be drawn.
	const int pageBorder = std::min(512 - hScroll, displayX + displayWidth);
	int scrollPage1, scrollPage2;
	if (vdp.isMultiPageScrolling()) {
		scrollPage1 = vdp.getHorizontalScrollHigh() >> 5;
		scrollPage2 = scrollPage1 ^ 1;
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
		int pageMaskOdd = (mode.isPlanar() ? 0x000 : 0x200) |
		                  vdp.getEvenOddMask();
		int pageMaskEven = vdp.isMultiPageScrolling()
		                 ? (pageMaskOdd & ~0x100)
		                 : pageMaskOdd;

		// Copy from cache to screen.
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		while (displayHeight != 0) {
			const int nameMask = vram.nameTable.getMask() >> 7;
			// Determine the line at which the display will wrap.
			// This could be end of page, or earlier, depending on the name
			// table mask.
			// TODO: There is probably a quicker way to do this, but I'll
			//       leave that for a rainy afternoon.
			const int wrapMask = ~(nameMask & 0xFF);
			int displayEndY = displayY;
			do {
				displayEndY++;
				displayHeight--;
			} while (displayHeight != 0
				&& (wrapMask & displayEndY) == (wrapMask & displayY)
				);

			const int screenEndY = screenY + 2 * (displayEndY - displayY);

			// TODO: scrollPage + pageMask + vramLine is one more level of
			//       indirection than we actually need.
			int vramLine[] = {
				nameMask & (pageMaskEven | displayY),
				nameMask & (pageMaskOdd  | displayY)
				};
			int firstPageWidth = pageBorder - displayX;
			if (firstPageWidth > 0) {
				const int srcL = (displayX + hScroll) / zoom;
				const int srcT = vramLine[scrollPage1];
				const int srcR = (pageBorder + hScroll) / zoom;
				const int srcB = srcT + displayEndY - displayY;
				bitmapTexture->draw(
					srcL, srcT, srcR, srcB,
					screenX, screenY,
					screenX + firstPageWidth, screenEndY
					);
			} else {
				firstPageWidth = 0;
			}
			if (firstPageWidth < displayWidth) {
				const int srcL =
					(displayX + firstPageWidth + hScroll - 512) / zoom;
				const int srcT = vramLine[scrollPage2];
				const int srcR = srcL + (displayWidth - firstPageWidth) / zoom;
				const int srcB = srcT + displayEndY - displayY;
				bitmapTexture->draw(
					srcL, srcT, srcR, srcB,
					screenX + firstPageWidth, screenY,
					screenX + displayWidth, screenEndY
					);
			}
			screenY = screenEndY;
			displayY = displayEndY & 255;
		}
	} else {
		switch (mode.getByte()) {
		case DisplayMode::TEXT1:
			renderText1(displayY, screenY, displayHeight,
			            displayX, displayX + displayWidth);
			break;
		case DisplayMode::TEXT2:
			renderText2(displayY, screenY, displayHeight,
			            displayX, displayX + displayWidth);
			break;
		case DisplayMode::GRAPHIC1:
			// TODO: Implement horizontal scroll high.
			renderGraphic1(displayY, screenY, displayHeight,
			               displayX, displayX + displayWidth);
			break;
		case DisplayMode::GRAPHIC2:
		case DisplayMode::GRAPHIC3:
			// TODO: Implement horizontal scroll high.
			renderGraphic2(displayY, screenY, displayHeight,
			               displayX, displayX + displayWidth);
			break;
		case DisplayMode::MULTICOLOUR:
			glDisable(GL_TEXTURE_2D);
			renderMultiColour(displayY, screenY, displayHeight,
			                  displayX, displayX + displayWidth);
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

void GLRasterizer::drawSprites(
	int /*fromX*/, int fromY,
	int displayX, int /*displayY*/,
	int displayWidth, int displayHeight)
{
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	int screenX = translateX(vdp.getLeftSprites()) + displayX * 2;
	// TODO: Code duplicated from drawDisplay (introduce translateY?).
	int screenY = (fromY - lineRenderTop) * 2;

	int spriteMode = vdp.getDisplayMode().getSpriteMode();
	int displayLimitX = displayX + displayWidth;
	int limitY = fromY + displayHeight;
	byte mode = vdp.getDisplayMode().getByte();
	int pixelZoom =
		(mode == DisplayMode::GRAPHIC5 || mode == DisplayMode::GRAPHIC6)
		? 2 : 1;
	const SpriteChecker& spriteChecker = vdp.getSpriteChecker();
	for (int y = fromY; y < limitY; y++, screenY += 2) {
		// Optimisation: only draw sprites if there are actually sprites on
		// this line.
		if (!spriteChecker.hasSprites(y)) continue;

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
	}

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

void GLRasterizer::update(const Setting& setting)
{
	VideoLayer::update(setting);
	if ((&setting == &renderSettings.getGamma()) ||
	    (&setting == &renderSettings.getBrightness()) ||
	    (&setting == &renderSettings.getContrast())) {
		precalcPalette();
		resetPalette();
	
		// Invalidate bitmap cache (still needed for non-palette modes)
		dirtyColour.flush();
		memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
	}
	FloatSetting& noiseSetting = renderSettings.getNoise();
	if (&setting == &noiseSetting) {
		preCalcNoise(noiseSetting.getValue());
	}
}

} // namespace openmsx

