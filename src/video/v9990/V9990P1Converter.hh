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

	void convertLine(Pixel* linePtr,
	                 int displayX, int displayWidth, int displayY);

private:
	V9990& vdp;
	V9990VRAM& vram;
	Pixel* palette64;

	Pixel raster(int xA, int yA,
	             unsigned int nameTableA, unsigned int patternTableA,
	             int xB, int yB,
	             unsigned int nameTableB, unsigned int patternTableB,
	             int *visibleSprites, unsigned int x, unsigned int y);
	byte getPixel(int x, int y,
	              unsigned int nameTable, unsigned int patternTable);
	void determineVisibleSprites(int* visibleSprites, int displayY);
	byte getSpritePixel(int* visibleSprites, int x, int y, bool front);
};

} // namespace openmsx

#endif
