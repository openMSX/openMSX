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
	V9990P2Converter(V9990& vdp_, const Pixel* palette64);

	void convertLine(
		Pixel* linePtr, unsigned displayX, unsigned displayWidth,
		unsigned displayY, unsigned displayYA, bool drawSprites);

private:
	void renderPattern(Pixel* buffer, unsigned width,
	                   unsigned x, unsigned y, byte pal);
	void determineVisibleSprites(int* visibleSprites, int displayY);
	void renderSprites(Pixel* buffer, int displayX, int displayEnd,
	                   unsigned displayY, const int* visibleSprites, bool front);

	V9990& vdp;
	V9990VRAM& vram;
	const Pixel* const palette64;
};

} // namespace openmsx

#endif
