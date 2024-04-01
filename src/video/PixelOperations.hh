#ifndef PIXELOPERATIONS_HH
#define PIXELOPERATIONS_HH

#include <bit>
#include <cstdint>
#include <span>

namespace openmsx {

using Pixel = uint32_t;

class PixelOperations
{
public:
	[[nodiscard]] Pixel getRmask() const { return 0x000000FF; }
	[[nodiscard]] Pixel getGmask() const { return 0x0000FF00; }
	[[nodiscard]] Pixel getBmask() const { return 0x00FF0000; }
	[[nodiscard]] Pixel getAmask() const { return 0xFF000000; }

	/** Extract RGBA components, each in range [0..255]
	  */
	[[nodiscard]] unsigned red(Pixel p) const;
	[[nodiscard]] unsigned green(Pixel p) const;
	[[nodiscard]] unsigned blue(Pixel p) const;
	[[nodiscard]] unsigned alpha(Pixel p) const;

	/** Combine RGB components to a pixel
	  */
	[[nodiscard]] Pixel combine(unsigned r, unsigned g, unsigned b) const;

	/** Blend the given colors into a single color.
	  * The special case for blending between two colors with
	  * an equal blend weight has an optimized implementation.
	  */
	template<unsigned w1, unsigned w2>
	[[nodiscard]] Pixel blend(Pixel p1, Pixel p2) const;
	template<unsigned w1, unsigned w2, unsigned w3>
	[[nodiscard]] Pixel blend(Pixel p1, Pixel p2, Pixel p3) const;
	template<unsigned w1, unsigned w2, unsigned w3, unsigned w4>
	[[nodiscard]] Pixel blend(Pixel p1, Pixel p2, Pixel p3, Pixel p4) const;
	template<unsigned w1, unsigned w2, unsigned w3,
	         unsigned w4, unsigned w5, unsigned w6>
	[[nodiscard]] Pixel blend(Pixel p1, Pixel p2, Pixel p3,
	                          Pixel p4, Pixel p5, Pixel p6) const;

	template<unsigned w1, unsigned w2>
	[[nodiscard]] Pixel blend(std::span<const Pixel, 2> p) const;
	template<unsigned w1, unsigned w2, unsigned w3>
	[[nodiscard]] Pixel blend(std::span<const Pixel, 3> p) const;
	template<unsigned w1, unsigned w2, unsigned w3, unsigned w4>
	[[nodiscard]] Pixel blend(std::span<const Pixel, 4> p) const;
	template<unsigned w1, unsigned w2, unsigned w3,
	         unsigned w4, unsigned w5, unsigned w6>
	[[nodiscard]] Pixel blend(std::span<const Pixel, 6> p) const;

	/** Perform a component wise multiplication of a pixel with an 8-bit
	  * fractional value:
	  *     result = (pixel * x) / 256  [component wise]
	  * 'x' must be in range [0..256].
	  * For x=0   the result is 0.
	  * For x=255 the result in the original value.
	  * Note: ATM only implemented for 32bpp.
	  */
	[[nodiscard]] static Pixel multiply(Pixel p, unsigned x);

	/** Perform linear interpolation between two pixels.
	 * This calculates component-wise:
	 *   (c1 * (256 - x) + c2 * x) / 256
	 * with c1, c2 the R,B,G,A components of the pixel.
	 * 'x' must be in range [0..256].
	 * For x=0   the result is p1.
	 * For x=256 the result is p2.
	 */
	[[nodiscard]] Pixel lerp(Pixel p1, Pixel p2, unsigned x) const;

	/** Perform alpha blending of two pixels.
	 * Pixel p1 contains the alpha value. For maximal alpha p1 is
	 * returned, for minimal alpha p2.
	 */
	[[nodiscard]] Pixel alphaBlend(Pixel p1, Pixel p2) const;

private:
	[[nodiscard]] Pixel avgDown(Pixel p1, Pixel p2) const;
	[[nodiscard]] Pixel avgUp  (Pixel p1, Pixel p2) const;
};


inline unsigned PixelOperations::red(Pixel p) const
{
	return (p >> 0) & 0xFF;
}
inline unsigned PixelOperations::green(Pixel p) const
{
	return (p >> 8) & 0xFF;
}
inline unsigned PixelOperations::blue(Pixel p) const
{
	return (p >> 16) & 0xFF;
}
inline unsigned PixelOperations::alpha(Pixel p) const
{
	return (p >> 24) & 0xFF;
}

inline Pixel PixelOperations::combine(
		unsigned r, unsigned g, unsigned b) const
{
	return Pixel((r << 0) | (g << 8) | (b << 16) | (0xFF << 24));
}

inline Pixel PixelOperations::avgDown(Pixel p1, Pixel p2) const
{
	// Average can be calculated as:
	//    floor((x + y) / 2.0) = (x & y) + (x ^ y) / 2
	// see "Average of Integers" on http://aggregate.org/MAGIC/
	return (p1 & p2) + (((p1 ^ p2) & 0xFEFEFEFE) >> 1);
}
inline Pixel PixelOperations::avgUp(Pixel p1, Pixel p2) const
{
	// Similar to above, but rounds up
	//    ceil((x + y) / 2.0) = (x | y) - (x ^ y) / 2
	return (p1 | p2) - (((p1 ^ p2) & 0xFEFEFEFE) >> 1);
}

template<unsigned w1, unsigned w2>
inline Pixel PixelOperations::blend(Pixel p1, Pixel p2) const
{
	constexpr unsigned total = w1 + w2;
	if constexpr (w1 == 0) {
		return p2;
	} else if constexpr (w1 > w2) {
		return blend<w2, w1>(p2, p1);

	} else if constexpr (w1 == w2) {
		// <1,1>
		return avgDown(p1, p2);
	} else if constexpr ((3 * w1) == w2) {
		// <1,3>
		Pixel p11 = avgDown(p1, p2);
		return avgUp(p11, p2);
	} else if constexpr ((7 * w1) == w2) {
		// <1,7>
		Pixel p11 = avgDown(p1, p2);
		Pixel p13 = avgDown(p11, p2);
		return avgUp(p13, p2);
	} else if constexpr ((5 * w1) == (3 * w2)) {
		// <3,5>   mix rounding up/down to get a more accurate result
		Pixel p11 = avgUp  (p1, p2);
		Pixel p13 = avgDown(p11, p2);
		return avgDown(p11, p13);

	} else if constexpr (!std::has_single_bit(total)) {
		// approximate with weights that sum to 256
		// e.g. approximate <1,2> as <85,171> (or <21,43>)
		//  ww1 = round(256 * w1 / total)   ww2 = 256 - ww1
		constexpr unsigned newTotal = 256;
		constexpr unsigned ww1 = (2 * w1 * newTotal + total) / (2 * total);
		constexpr unsigned ww2 = 256 - ww1;
		return blend<ww1, ww2>(p1, p2);

	} else {
		// 32bpp
		constexpr unsigned l2 = std::bit_width(total) - 1;
		unsigned c1 = (((p1 & 0x00FF00FF) * w1 +
				(p2 & 0x00FF00FF) * w2
			       ) >> l2) & 0x00FF00FF;
		unsigned c2 = (((p1 & 0xFF00FF00) >> l2) * w1 +
			       ((p2 & 0xFF00FF00) >> l2) * w2
			      ) & 0xFF00FF00;
		return c1 | c2;
	}
}

template<unsigned w1, unsigned w2, unsigned w3>
inline Pixel PixelOperations::blend(Pixel p1, Pixel p2, Pixel p3) const
{
	constexpr unsigned total = w1 + w2 + w3;
	if constexpr (std::has_single_bit(total)) {
		constexpr unsigned l2 = std::bit_width(total) - 1;
		unsigned c1 = (((p1 & 0x00FF00FF) * w1 +
		                (p2 & 0x00FF00FF) * w2 +
		                (p3 & 0x00FF00FF) * w3) >> l2) & 0x00FF00FF;
		unsigned c2 = (((p1 & 0xFF00FF00) >> l2) * w1 +
		               ((p2 & 0xFF00FF00) >> l2) * w2 +
		               ((p3 & 0xFF00FF00) >> l2) * w3) & 0xFF00FF00;
		return c1 | c2;
	} else {
		unsigned r = (red  (p1) * w1 + red  (p2) * w2 + red  (p3) * w3) / total;
		unsigned g = (green(p1) * w1 + green(p2) * w2 + green(p3) * w3) / total;
		unsigned b = (blue (p1) * w1 + blue (p2) * w2 + blue (p3) * w3) / total;
		return combine(r, g, b);
	}
}

template<unsigned w1, unsigned w2, unsigned w3, unsigned w4>
inline Pixel PixelOperations::blend(
		Pixel p1, Pixel p2, Pixel p3, Pixel p4) const
{
	constexpr unsigned total = w1 + w2 + w3 + w4;
	if constexpr (std::has_single_bit(total)) {
		constexpr unsigned l2 = std::bit_width(total) - 1;
		unsigned c1 = (((p1 & 0x00FF00FF) * w1 +
		                (p2 & 0x00FF00FF) * w2 +
		                (p3 & 0x00FF00FF) * w3 +
		                (p4 & 0x00FF00FF) * w4) >> l2) & 0x00FF00FF;
		unsigned c2 = (((p1 & 0xFF00FF00) >> l2) * w1 +
		               ((p2 & 0xFF00FF00) >> l2) * w2 +
		               ((p3 & 0xFF00FF00) >> l2) * w3 +
		               ((p4 & 0xFF00FF00) >> l2) * w4) & 0xFF00FF00;
		return c1 | c2;
	} else {
		unsigned r = (red  (p1) * w1 + red  (p2) * w2 +
			      red  (p3) * w3 + red  (p4) * w4) / total;
		unsigned g = (green(p1) * w1 + green(p2) * w2 +
			      green(p3) * w3 + green(p4) * w4) / total;
		unsigned b = (blue (p1) * w1 + blue (p2) * w2 +
			      blue (p3) * w3 + blue (p4) * w4) / total;
		return combine(r, g, b);
	}
}

template<unsigned w1, unsigned w2, unsigned w3,
          unsigned w4, unsigned w5, unsigned w6>
inline Pixel PixelOperations::blend(
	Pixel p1, Pixel p2, Pixel p3, Pixel p4, Pixel p5, Pixel p6) const
{
	constexpr unsigned total = w1 + w2 + w3 + w4 + w5 + w6;
	if constexpr (std::has_single_bit(total)) {
		constexpr unsigned l2 = std::bit_width(total) - 1;
		unsigned c1 = (((p1 & 0x00FF00FF) * w1 +
		                (p2 & 0x00FF00FF) * w2 +
		                (p3 & 0x00FF00FF) * w3 +
		                (p4 & 0x00FF00FF) * w4 +
		                (p5 & 0x00FF00FF) * w5 +
		                (p6 & 0x00FF00FF) * w6) >> l2) & 0x00FF00FF;
		unsigned c2 = (((p1 & 0xFF00FF00) >> l2) * w1 +
		               ((p2 & 0xFF00FF00) >> l2) * w2 +
		               ((p3 & 0xFF00FF00) >> l2) * w3 +
		               ((p4 & 0xFF00FF00) >> l2) * w4 +
		               ((p5 & 0xFF00FF00) >> l2) * w5 +
		               ((p6 & 0xFF00FF00) >> l2) * w6) & 0xFF00FF00;
		return c1 | c2;
	} else {
		unsigned r = (red  (p1) * w1 + red  (p2) * w2 +
		              red  (p3) * w3 + red  (p4) * w4 +
		              red  (p5) * w5 + red  (p6) * w6) / total;
		unsigned g = (green(p1) * w1 + green(p2) * w2 +
		              green(p3) * w3 + green(p4) * w4 +
		              green(p5) * w5 + green(p6) * w6) / total;
		unsigned b = (blue (p1) * w1 + blue (p2) * w2 +
		              blue (p3) * w3 + blue (p4) * w4 +
		              blue (p5) * w5 + blue (p6) * w6) / total;
		return combine(r, g, b);
	}
}


template<unsigned w1, unsigned w2>
inline Pixel PixelOperations::blend(std::span<const Pixel, 2> p) const
{
	return blend<w1, w2>(p[0], p[1]);
}

template<unsigned w1, unsigned w2, unsigned w3>
inline Pixel PixelOperations::blend(std::span<const Pixel, 3> p) const
{
	return blend<w1, w2, w3>(p[0], p[1], p[2]);
}

template<unsigned w1, unsigned w2, unsigned w3, unsigned w4>
inline Pixel PixelOperations::blend(std::span<const Pixel, 4> p) const
{
	return blend<w1, w2, w3, w4>(p[0], p[1], p[2], p[3]);
}

template<unsigned w1, unsigned w2, unsigned w3,
          unsigned w4, unsigned w5, unsigned w6>
inline Pixel PixelOperations::blend(std::span<const Pixel, 6> p) const
{
	return blend<w1, w2, w3, w4, w5, w6>(p[0], p[1], p[2], p[3], p[4], p[5]);
}

inline Pixel PixelOperations::multiply(Pixel p, unsigned x)
{
	return ((((p       & 0x00FF00FF) * x) & 0xFF00FF00) >> 8)
	     | ((((p >> 8) & 0x00FF00FF) * x) & 0xFF00FF00);
}

inline Pixel PixelOperations::lerp(Pixel p1, Pixel p2, unsigned x) const
{
	// 32bpp
	unsigned rb1 = (p1 >> 0) & 0x00FF00FF;
	unsigned ag1 = (p1 >> 8) & 0x00FF00FF;
	unsigned rb2 = (p2 >> 0) & 0x00FF00FF;
	unsigned ag2 = (p2 >> 8) & 0x00FF00FF;

	// Note: the subtraction for the lower component can 'borrow' from
	// the higher component. Though in the full calculation this error
	// magically cancels out.
	unsigned trb = ((rb2 - rb1) * x) >> 8;
	unsigned tag = ((ag2 - ag1) * x) >> 0;

	unsigned rb  = ((trb + rb1) << 0) & 0x00FF00FF;
	unsigned ag  = (tag + (ag1 << 8)) & 0xFF00FF00;

	return rb | ag;
}

inline Pixel PixelOperations::alphaBlend(Pixel p1, Pixel p2) const
{
	unsigned a = alpha(p1);
	// Note: 'a' is [0..255], while lerp() expects [0..256].
	//       We ignore this small error.
	return lerp(p2, p1, a);
}

} // namespace openmsx

#endif
