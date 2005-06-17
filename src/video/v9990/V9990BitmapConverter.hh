// $Id$

#ifndef V9990BITMAPCONVERTER_HH
#define V9990BITMAPCONVERTER_HH

#include "openmsx.hh"
#include "Renderer.hh"
#include "V9990ModeEnum.hh"
#include <SDL.h>

namespace openmsx {

class V9990;
class V9990VRAM;

/** Utility class to convert VRAM content to host pixels.
  */
template <class Pixel, Renderer::Zoom zoom>
class V9990BitmapConverter
{
public:
	V9990BitmapConverter(V9990& vdp,
	                     SDL_PixelFormat fmt,
	                     Pixel* palette64,
	                     Pixel* palette256,
	                     Pixel* palette32768);
	~V9990BitmapConverter();

	/** Convert a line of VRAM into host pixels.
	  */
	void convertLine(Pixel* linePtr, unsigned address, int nrPixels, int displayY);

	/** Set the display mode: defines screen geometry.
	  */
	void setDisplayMode(V9990DisplayMode mode);

	/** Set the color mode
	  */
	void setColorMode(V9990ColorMode mode);

private:
	/** Reference to VDP
	  */
	V9990& vdp;

	/** Reference to VDP VRAM
	  */
	V9990VRAM& vram;

	/** Rastering method for the current color mode
	  */
	typedef void (V9990BitmapConverter<Pixel, zoom>::*RasterMethod)
	             (Pixel* pixelPtr, unsigned address, int nrPixels);
	RasterMethod rasterMethod;

	/** Blend method for the current display mode
	  */
	typedef void (V9990BitmapConverter<Pixel, zoom>::*BlendMethod)
	             (const Pixel* source, Pixel* dest, int nrPixels);
	BlendMethod blendMethod;

	/** Pixel format
	  */
	SDL_PixelFormat format;

	/** The 64 color palette for P1, P2 and BP* modes
	  * This is the palette manipulated through the palette port and register
	  */
	Pixel* palette64;

	/** The 256 color palette for BD8 mode
	  * A fixed palette; sub-color space within the 32768 color palette
	  */
	Pixel* palette256;

	/** The 15-bits color palette for BD16, BYJK* and BYUV modes
	  * This is the complete color space for the V9990
	  */
	Pixel* palette32768;

	/* private blending methods */
	inline unsigned int red(Pixel pixel);
	inline unsigned int green(Pixel pixel);
	inline unsigned int blue(Pixel pixel);
	inline Pixel combine(unsigned r, unsigned g, unsigned b);

	template <unsigned w1, unsigned w2>
	inline Pixel blendPixels2(const Pixel* source);
	template <unsigned w1, unsigned w2, unsigned w3>
	inline Pixel blendPixels3(const Pixel* source);
	template <unsigned w1, unsigned w2, unsigned w3, unsigned w4>
	inline Pixel blendPixels4(const Pixel* source);

	void blend_1on3(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void blend_1on2(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void blend_1on1(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void blend_2on1(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void blend_4on1(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void blend_2on3(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void blend_4on3(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void blend_8on3(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void blend_none(const Pixel* inPixels, Pixel* outPixels, int nrPixels);

	/* private Raster methods */
	void rasterP    (Pixel* outPixels, unsigned address, int nrPixels);
	void rasterBYUV (Pixel* outPixels, unsigned address, int nrPixels);
	void rasterBYUVP(Pixel* outPixels, unsigned address, int nrPixels);
	void rasterBYJK (Pixel* outPixels, unsigned address, int nrPixels);
	void rasterBYJKP(Pixel* outPixels, unsigned address, int nrPixels);
	void rasterBD16 (Pixel* outPixels, unsigned address, int nrPixels);
	void rasterBD8  (Pixel* outPixels, unsigned address, int nrPixels);
	void rasterBP6  (Pixel* outPixels, unsigned address, int nrPixels);
	void rasterBP4  (Pixel* outPixels, unsigned address, int nrPixels);
	void rasterBP2  (Pixel* outPixels, unsigned address, int nrPixels);

	/* Cursor drawing methods */
	void drawCursor(Pixel* buffer, int displayY,
	                unsigned attrAddr, unsigned patAddr);
	void drawCursors(Pixel* buffer, int displayY);
};

} // namespace openmsx

#endif
