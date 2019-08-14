#ifndef SCANLINE_HH
#define SCANLINE_HH

#include "PixelOperations.hh"
#include <cstdint>

namespace openmsx {

/**
 * Helper class to perform 'pixel x scalar' calculations.
 */
template<typename Pixel> class Multiply;

template<> class Multiply<uint16_t>
{
public:
	explicit Multiply(const PixelOperations<uint16_t>& pixelOps);
	void setFactor(unsigned f);
	inline uint16_t multiply(uint16_t p, unsigned factor) const;
	inline uint16_t multiply(uint16_t p) const;
	inline const uint16_t* getTable() const;
private:
	const PixelOperations<uint16_t>& pixelOps;
	unsigned factor;
	uint16_t tab[0x10000];
};

template<> class Multiply<uint32_t>
{
public:
	explicit Multiply(const PixelOperations<uint32_t>& pixelOps);
	void setFactor(unsigned f);
	inline uint32_t multiply(uint32_t p, unsigned factor) const;
	inline uint32_t multiply(uint32_t p) const;
	const uint32_t* getTable() const;
private:
	unsigned factor;
};

/**
 * Helper class to draw scalines
 */
template <class Pixel> class Scanline
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
	  * @param width Line width in pixels.
	  */
	void draw(const Pixel* src1, const Pixel* src2, Pixel* dst,
	          unsigned factor, size_t width);

	/** Darken one pixel. Typically used to implement drawBlank().
	 */
	Pixel darken(Pixel p, unsigned factor);

	/** Darken and blend two pixels.
	 */
	Pixel darken(Pixel p1, Pixel p2, unsigned factor);

private:
	Multiply<Pixel> darkener;
	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif
