// $Id$

#ifndef V9990P2CONVERTER_HH
#define V9990P2CONVERTER_HH

#include "openmsx.hh"

namespace openmsx {

class V9990;
class V9990VRAM;

template <class Pixel>
class V9990P2Converter
{
public:
	V9990P2Converter(V9990& vdp_, Pixel* palette64);

	void convertLine(Pixel* linePtr,
	                 int displayX, int displayWidth, int displayY);

private:
	Pixel raster(unsigned xA, unsigned yA,
	             int* visibleSprites, unsigned x, unsigned y);
	byte getPixel(unsigned x, unsigned y);
	void determineVisibleSprites(int* visibleSprites, int displayY);
	byte getSpritePixel(int* visibleSprites, int x, int y, bool front);

	V9990& vdp;
	V9990VRAM& vram;
	Pixel* palette64;
};

} // namespace openmsx

#endif
