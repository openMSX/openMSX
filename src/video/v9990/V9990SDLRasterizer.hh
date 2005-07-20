// $Id$

#ifndef V9990SDLRASTERIZER_HH
#define V9990SDLRASTERIZER_HH

#include "V9990Rasterizer.hh"
#include "V9990BitmapConverter.hh"
#include "V9990P1Converter.hh"
#include "V9990P2Converter.hh"
#include "Renderer.hh"

namespace openmsx {

class V9990;
class V9990VRAM;
class BooleanSetting;

/** Rasterizer using SDL.
  */
template <class Pixel>
class V9990SDLRasterizer : public V9990Rasterizer
{
public:
	/** Constructor.
	  */
	V9990SDLRasterizer(V9990& vdp, SDL_Surface* screen);

	/** Destructor.
	  */
	virtual ~V9990SDLRasterizer();

	// Layer interface:
	virtual void paint();
	virtual const std::string& getName();

	// Rasterizer interface:
	virtual void reset();
	virtual void frameStart();
	virtual void frameEnd();
	virtual void setDisplayMode(V9990DisplayMode displayMode);
	virtual void setColorMode(V9990ColorMode colorMode);
	virtual void setPalette(int index, byte r, byte g, byte b);
	virtual void setImageWidth(int width);
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

	/** Number of workscreens
	  */
	static const int NB_WORKSCREENS = 2;

	/** The VDP of which the video output is being rendered.
	  */
	V9990& vdp;

	/** The VRAM whose contents are rendered.
	  */
	V9990VRAM& vram;

	/** The surface which is visible to the user.
	  */
	SDL_Surface* screen;

	/** Work screens, 2nd surface is used for deinterlace mode
	  */
	SDL_Surface* workScreens[NB_WORKSCREENS];

	/** First display line to draw. Since the number of VDP lines >=
	  * the screen height, lineZero is >= 0. Only part of the video
	  * image is visible.
	  */
	int lineZero;

	/** First display column to draw.  Since the width of the VDP lines <=
	  * the screen width, colZero is <= 0. The non-displaying parts of the
	  * screen will be filled as border.
	  */
	int colZero;

	/** The current screen mode
	  */
	V9990DisplayMode displayMode;
	V9990ColorMode   colorMode;

	/** Image width in pixels
	  */
	int imageWidth;

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

	/** Get the active workscreen (for deinterlace)
	  */
	SDL_Surface* getWorkScreen(bool prev = false) const;

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
