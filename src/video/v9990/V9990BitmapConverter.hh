#ifndef V9990BITMAPCONVERTER_HH
#define V9990BITMAPCONVERTER_HH

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
	             const Pixel* palette64, const Pixel* palette256,
	             const Pixel* palette32768);

	/** Convert a line of VRAM into host pixels.
	  */
	void convertLine(Pixel* linePtr, unsigned x, unsigned y, int nrPixels,
		         int cursorY, bool drawSprites);

	/** Set the color mode
	  */
	void setColorMode(V9990ColorMode color, V9990DisplayMode display);

private:
	/** Reference to VDP
	  */
	V9990& vdp;

	/** Reference to VDP VRAM
	  */
	V9990VRAM& vram;

	/** Rastering method for the current color mode
	  */
	using RasterMethod = void (V9990BitmapConverter<Pixel>::*)
	             (Pixel* out, unsigned x, unsigned y, int nrPixels);
	RasterMethod rasterMethod;

	/** The 64 color palette for P1, P2 and BP* modes
	  * This is the palette manipulated through the palette port and register
	  */
	const Pixel* const palette64;

	/** The 256 color palette for BD8 mode
	  * A fixed palette; sub-color space within the 32768 color palette
	  */
	const Pixel* const palette256;

	/** The 15-bits color palette for BD16, BYJK* and BYUV modes
	  * This is the complete color space for the V9990
	  */
	const Pixel* const palette32768;

	/* private Raster methods */
	void rasterP       (Pixel* out, unsigned x, unsigned y, int nrPixels);
	void rasterBYUV    (Pixel* out, unsigned x, unsigned y, int nrPixels);
	void rasterBYUVP   (Pixel* out, unsigned x, unsigned y, int nrPixels);
	void rasterBYJK    (Pixel* out, unsigned x, unsigned y, int nrPixels);
	void rasterBYJKP   (Pixel* out, unsigned x, unsigned y, int nrPixels);
	void rasterBD16    (Pixel* out, unsigned x, unsigned y, int nrPixels);
	void rasterBD8     (Pixel* out, unsigned x, unsigned y, int nrPixels);
	void rasterBP6     (Pixel* out, unsigned x, unsigned y, int nrPixels);
	void rasterBP4     (Pixel* out, unsigned x, unsigned y, int nrPixels);
	void rasterBP2     (Pixel* out, unsigned x, unsigned y, int nrPixels);
	void rasterBP4HiRes(Pixel* out, unsigned x, unsigned y, int nrPixels);
	void rasterBP2HiRes(Pixel* out, unsigned x, unsigned y, int nrPixels);

	/* Cursor drawing methods */
	void drawCursor(Pixel* buffer, int displayY,
	                unsigned attrAddr, unsigned patAddr);
	void drawCursors(Pixel* buffer, int displayY);
};

} // namespace openmsx

#endif
