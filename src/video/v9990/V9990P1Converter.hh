// $Id$

#ifndef V9990P1CONVERTER_HH
#define V9990P1CONVERTER_HH

#include "openmsx.hh"

namespace openmsx {

class V9990;
class V9990VRAM;

template <class Pixel>
class V9990P1Converter
{
public:
	V9990P1Converter(V9990& vdp, Pixel* palette64);

	void convertLine(Pixel* linePtr, unsigned displayX,
	                 unsigned displayWidth, unsigned displayY);

private:
	V9990& vdp;
	V9990VRAM& vram;
	Pixel* palette64;

	Pixel raster(unsigned xA, unsigned yA,
	             unsigned nameA, unsigned patternA, byte palA,
	             unsigned xB, unsigned yB,
	             unsigned nameB, unsigned patternB, byte palB,
	             int* visibleSprites, unsigned x, unsigned y);
	byte getPixel(unsigned x, unsigned y,
	              unsigned nameTable, unsigned patternTable);
	void determineVisibleSprites(int* visibleSprites, unsigned displayY);
	byte getSpritePixel(int* visibleSprites, unsigned x, unsigned y,
	                    bool front);
};

} // namespace openmsx

#endif
