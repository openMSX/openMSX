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
	V9990P1Converter(V9990& vdp, const Pixel* palette64);

	void convertLine(
		Pixel* linePtr, unsigned displayX, unsigned displayWidth,
		unsigned displayY, unsigned displayYA, unsigned displayYB,
		bool drawSprites);

private:
	V9990& vdp;
	V9990VRAM& vram;
	const Pixel* const palette64;

	void renderPattern(Pixel* buffer, unsigned width1, unsigned width2,
	                   unsigned displayAX, unsigned displayAY,
	                   unsigned nameA, unsigned patternA, byte palA,
	                   unsigned displayBX, unsigned displayBY,
	                   unsigned nameB, unsigned patternB, byte palB);
	void renderPattern2(Pixel* buffer, unsigned width,
	                    unsigned AX, unsigned AY, unsigned name,
	                    unsigned pattern, byte pal);
	void determineVisibleSprites(int* visibleSprites, unsigned displayY);
	void renderSprites(Pixel* buffer, int displayX, int displayEnd,
	                   unsigned displayY, const int* visibleSprites, bool front);
};

} // namespace openmsx

#endif
