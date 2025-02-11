#ifndef V9990PXCONVERTER_HH
#define V9990PXCONVERTER_HH

#include <cstdint>
#include <span>

namespace openmsx {

class V9990;
class V9990VRAM;

class V9990P1Converter
{
public:
	using Pixel = uint32_t;

	V9990P1Converter(V9990& vdp, std::span</*const*/ Pixel, 64> palette64);

	void convertLine(
		std::span<Pixel> buf, unsigned displayX, unsigned displayY,
		unsigned displayYA, unsigned displayYB, bool drawSprites);

private:
	V9990& vdp;
	V9990VRAM& vram;
	std::span</*const*/ Pixel, 64> palette64;
};

class V9990P2Converter
{
public:
	using Pixel = uint32_t;

	V9990P2Converter(V9990& vdp, std::span</*const*/ Pixel, 64> palette64);

	void convertLine(
		std::span<Pixel> buf, unsigned displayX, unsigned displayY,
		unsigned displayYA, bool drawSprites);

private:
	V9990& vdp;
	V9990VRAM& vram;
	std::span</*const*/ Pixel, 64> palette64;
};

} // namespace openmsx

#endif
