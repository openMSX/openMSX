#ifndef MULTIPLY32_HH
#define MULTIPLY32_HH

#include <bit>
#include <cstdint>

namespace openmsx {

template<typename Pixel> class PixelOperations;

/**
 * Helper class to perform 'pixel x scalar' calculations.
 * Also converts the result to 32bpp.
 */
template<typename Pixel> class Multiply32;

template<> class Multiply32<uint32_t>
{
public:
	explicit Multiply32(const PixelOperations<uint32_t>& format);

	inline void setFactor32(unsigned f)
	{
		factor = f;
	}

	[[nodiscard]] inline uint32_t mul32(uint32_t p) const
	{
		return ((((p       & 0x00FF00FF) * factor) & 0xFF00FF00) >> 8)
		     | ((((p >> 8) & 0x00FF00FF) * factor) & 0xFF00FF00);
	}

	[[nodiscard]] inline uint32_t conv32(uint32_t p) const
	{
		return p;
	}

private:
	unsigned factor;
};

template<> class Multiply32<uint16_t>
{
public:
	explicit Multiply32(const PixelOperations<uint16_t>& pixelOps);

	void setFactor32(unsigned factor);

	[[nodiscard]] inline uint32_t mul32(uint16_t p) const
	{
		return tab[p];
	}

	[[nodiscard]] inline uint16_t conv32(uint32_t p) const
	{
		return (std::rotr(p, Rshift3) & Rmask1) |
		       (std::rotr(p, Gshift3) & Gmask1) |
		       (std::rotr(p, Bshift3) & Bmask1);
	}

private:
	uint32_t tab[0x10000];
	unsigned factor;
	unsigned Rshift1, Gshift1, Bshift1;
	unsigned Rshift2, Gshift2, Bshift2;
	unsigned Rshift3, Gshift3, Bshift3;
	uint16_t Rmask1,  Gmask1,  Bmask1;
	uint16_t Rmask2,  Gmask2,  Bmask2;
};

} // namespace openmsx

#endif
