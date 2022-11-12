#ifndef SCANLINE_HH
#define SCANLINE_HH

#include "PixelOperations.hh"
#include <array>
#include <cstdint>
#include <cstddef>
#include <span>

namespace openmsx {

/**
 * Helper class to perform 'pixel x scalar' calculations.
 */
template<std::unsigned_integral Pixel> class Multiply;

template<> class Multiply<uint16_t>
{
public:
	explicit Multiply(const PixelOperations<uint16_t>& pixelOps);
	void setFactor(unsigned f);
	[[nodiscard]] inline uint16_t multiply(uint16_t p, unsigned factor) const;
	[[nodiscard]] inline uint16_t multiply(uint16_t p) const;
	[[nodiscard]] inline std::span<const uint16_t, 0x10000> getTable() const { return tab; }
private:
	const PixelOperations<uint16_t>& pixelOps;
	unsigned factor = 0;
	std::array<uint16_t, 0x10000> tab = {};
};

template<> class Multiply<uint32_t>
{
public:
	explicit Multiply(const PixelOperations<uint32_t>& pixelOps);
	void setFactor(unsigned f);
	[[nodiscard]] inline uint32_t multiply(uint32_t p, unsigned factor) const;
	[[nodiscard]] inline uint32_t multiply(uint32_t p) const;
private:
	unsigned factor;
};

/**
 * Helper class to draw scanlines
 */
template<std::unsigned_integral Pixel> class Scanline
{
public:
	explicit Scanline(const PixelOperations<Pixel>& pixelOps);

	/** Draws a scanline. The scanline will be the average of the two
	  * input lines and darkened by a certain factor.
	  * @param src1 First input line.
	  * @param src2 Second input line.
	  * @param dst Output line.
	  * @param factor Darkness factor, 0 means completely black,
	  *               255 means no darkening.
	  */
	void draw(std::span<const Pixel> src1, std::span<const Pixel> src2,
	          std::span<Pixel> dst, unsigned factor);

	/** Darken one pixel. Typically used to implement drawBlank().
	 */
	[[nodiscard]] Pixel darken(Pixel p, unsigned factor) const;

	/** Darken and blend two pixels.
	 */
	[[nodiscard]] Pixel darken(Pixel p1, Pixel p2, unsigned factor) const;

private:
	Multiply<Pixel> darkener;
	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif
