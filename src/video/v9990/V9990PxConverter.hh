#ifndef V9990PXCONVERTER_HH
#define V9990PXCONVERTER_HH

#include <concepts>
#include <span>

namespace openmsx {

class V9990;
class V9990VRAM;

template<std::unsigned_integral Pixel>
class V9990P1Converter
{
public:
	V9990P1Converter(V9990& vdp, std::span<const Pixel, 64> palette64);

	void convertLine(
		Pixel* linePtr, unsigned displayX, unsigned displayWidth,
		unsigned displayY, unsigned displayYA, unsigned displayYB,
		bool drawSprites);

private:
	V9990& vdp;
	V9990VRAM& vram;
	std::span<const Pixel, 64> palette64;
};

template<std::unsigned_integral Pixel>
class V9990P2Converter
{
public:
	V9990P2Converter(V9990& vdp, std::span<const Pixel, 64> palette64);

	void convertLine(
		Pixel* linePtr, unsigned displayX, unsigned displayWidth,
		unsigned displayY, unsigned displayYA, bool drawSprites);

private:
	V9990& vdp;
	V9990VRAM& vram;
	std::span<const Pixel, 64> palette64;
};

} // namespace openmsx

#endif
