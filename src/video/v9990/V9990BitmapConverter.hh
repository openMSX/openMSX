// $Id$

#ifndef V9990BITMAPCONVERTER_HH
#define V9990BITMAPCONVERTER_HH

#include "openmsx.hh"
#include "V9990ModeEnum.hh"

namespace openmsx {

class V9990;
class V9990VRAM;

/** Utility class to convert VRAM content to host pixels.
  */
template <class Pixel>
class V9990BitmapConverter
{
public:
	V9990BitmapConverter(V9990& vdp,
	             Pixel* palette64, Pixel* palette256, Pixel* palette32768);

	/** Convert a line of VRAM into host pixels.
	  */
	void convertLine(Pixel* linePtr, unsigned address, int nrPixels, int displayY);

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
	typedef void (V9990BitmapConverter<Pixel>::*RasterMethod)
	             (Pixel* pixelPtr, unsigned address, int nrPixels);
	RasterMethod rasterMethod;

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
