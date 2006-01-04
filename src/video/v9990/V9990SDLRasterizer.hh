// $Id$

#ifndef V9990SDLRASTERIZER_HH
#define V9990SDLRASTERIZER_HH

#include "V9990Rasterizer.hh"
#include "V9990BitmapConverter.hh"
#include "V9990P1Converter.hh"
#include "V9990P2Converter.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class Display;
class V9990;
class V9990VRAM;
class RawFrame;
class OutputSurface;
class BooleanSetting;
template <class Pixel> class PostProcessor;

/** Rasterizer using SDL.
  */
template <class Pixel>
class V9990SDLRasterizer : public V9990Rasterizer, private noncopyable
{
public:
	V9990SDLRasterizer(V9990& vdp, Display& display, OutputSurface& screen);
	virtual ~V9990SDLRasterizer();

	// Rasterizer interface:
	virtual bool isActive();
	virtual void reset();
	virtual void frameStart();
	virtual void frameEnd();
	virtual void setDisplayMode(V9990DisplayMode displayMode);
	virtual void setColorMode(V9990ColorMode colorMode);
	virtual void setPalette(int index, byte r, byte g, byte b);
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY);
	virtual void drawDisplay(int fromX, int fromY,
	                         int displayX, int displayY,
	                         int displayWidth, int displayHeight);

private:
	/** screen width for SDLLo
	  */
	static const int SCREEN_WIDTH  = 320;

	/** screenheight for SDLLo
	  */
	static const int SCREEN_HEIGHT = 240;

	/** The VDP of which the video output is being rendered.
	  */
	V9990& vdp;

	/** The VRAM whose contents are rendered.
	  */
	V9990VRAM& vram;

	/** The surface which is visible to the user.
	  */
	OutputSurface& screen;

	/** The next frame as it is delivered by the VDP, work in progress.
	  */
	RawFrame* workFrame;

	/** Line to render at top of display.
	  * After all, our screen is 240 lines while display is 262 or 313.
	  */
	int lineRenderTop;

	/** First display column to draw.  Since the width of the VDP lines <=
	  * the screen width, colZero is <= 0. The non-displaying parts of the
	  * screen will be filled as border.
	  */
	int colZero;

	/** The current screen mode
	  */
	V9990DisplayMode displayMode;
	V9990ColorMode   colorMode;

	/** Palette containing the complete V9990 Color space
	  */
	Pixel palette32768[32768];

	/** The 256 color palette. A fixed subset of the palette32768.
	  */
	Pixel palette256[256];

	/** The 64 palette entries of the VDP - a subset of the palette32768.
	  * These are colors influenced by the palette IO ports and registers
	  */
	Pixel palette64[64];

	/** The video post processor which displays the frames produced by this
	  *  rasterizer.
	  */
	std::auto_ptr<PostProcessor<Pixel> > postProcessor;

	/** Bitmap converter. Converts VRAM into pixels
	  */
	V9990BitmapConverter<Pixel> bitmapConverter;

	/** P1 Converter
	  */
	V9990P1Converter<Pixel> p1Converter;

	/** P2 Converter
	  */
	V9990P2Converter<Pixel> p2Converter;

	/** Deinterlace setting
	  */
	BooleanSetting& deinterlaceSetting;

	/** Fill the palettes.
	  */
	void precalcPalettes();

	/** Draw P1 mode.
	  */
	void drawP1Mode(int fromX, int fromY, int displayX, int displayY,
	                int displayWidth, int displayHeight);

	/** Draw P2 mode.
	  */
	void drawP2Mode(int fromX, int fromY, int displayX, int displayY,
	                int displayWidth, int displayHeight);

	/** Draw Bx mode.
	  */
	void drawBxMode(int fromX, int fromY, int displayX, int displayY,
	                int displayWidth, int displayHeight);
};

} // namespace openmsx

#endif
