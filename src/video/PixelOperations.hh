// $Id$

#ifndef PIXELOPERATIONS_HH
#define PIXELOPERATIONS_HH

#include <SDL.h>
#include <cassert>

namespace openmsx {

// TODO: Needed for the "friend" declarations later.
template <typename Pixel> class Scanline;
template <typename Pixel> class Simple2xScaler;
template <typename Pixel> class TransparentScaler;
template <typename Pixel> class Blur_1on3;

template <typename Pixel>
class PixelOperations
{
public:
	explicit PixelOperations(const SDL_PixelFormat& format);

	/** Extract RGB componts
	  */
	inline unsigned red(Pixel p) const;
	inline unsigned green(Pixel p) const;
	inline unsigned blue(Pixel p) const;
	inline unsigned alpha(Pixel p) const;

	/** Same as above, but result is scaled to [0..255]
	  */
	inline unsigned red256(Pixel p) const;
	inline unsigned green256(Pixel p) const;
	inline unsigned blue256(Pixel p) const;

	/** Combine RGB components to a pixel
	  */
	inline Pixel combine(unsigned r, unsigned g, unsigned b) const;
	inline Pixel combine256(unsigned r, unsigned g, unsigned b) const;

	/** Get maximum component value
	  */
	inline unsigned getMaxRed() const;
	inline unsigned getMaxGreen() const;
	inline unsigned getMaxBlue() const;

	/** Blend the given colors into a single color.
	  * The special case for blending between two colors with
	  * an equal blend weight has an optimized implementation.
	  */
	template <unsigned w1, unsigned w2>
	inline Pixel blend(Pixel p1, Pixel p2) const;
	template <unsigned w1, unsigned w2, unsigned w3>
	inline Pixel blend(Pixel p1, Pixel p2, Pixel p3) const;
	template <unsigned w1, unsigned w2, unsigned w3, unsigned w4>
	inline Pixel blend(Pixel p1, Pixel p2, Pixel p3, Pixel p4) const;
	template <unsigned w1, unsigned w2, unsigned w3,
	          unsigned w4, unsigned w5, unsigned w6>
	inline Pixel blend(Pixel p1, Pixel p2, Pixel p3,
	                   Pixel p4, Pixel p5, Pixel p6) const;

	template <unsigned w1, unsigned w2>
	inline Pixel blend2(const Pixel* p) const;
	template <unsigned w1, unsigned w2, unsigned w3>
	inline Pixel blend3(const Pixel* p) const;
	template <unsigned w1, unsigned w2, unsigned w3, unsigned w4>
	inline Pixel blend4(const Pixel* p) const;
	template <unsigned w1, unsigned w2, unsigned w3,
	          unsigned w4, unsigned w5, unsigned w6>
	inline Pixel blend6(const Pixel* p) const;

	/** Returns a constant that is useful to calculate the average of
	  * two pixel values. See the implementation of blend(p1, p2) for
	  * more details.
	  * For single pixels it's of course better to use the blend(p1, p2)
	  * method directly. This method is typically used as a helper in
	  * older SIMD (MMX/SSE1) routines.
	  */
	inline Pixel getBlendMask() const;

	/** Perform alpha blending of two pixels.
	 * Pixel p1 contains the alpha value. For maximal alpha p1 is
	 * returned, for minimal alpha p2.
	 */
	inline Pixel alphaBlend(Pixel p1, Pixel p2) const;

private:
	inline Pixel avgDown(Pixel p1, Pixel p2) const;
	inline Pixel avgUp  (Pixel p1, Pixel p2) const;

	const SDL_PixelFormat* format;

	/** Mask used for blending.
	  * The least significant bit of R,G,B must be 0,
	  * all other bits must be 1.
	  *     0000BBBBGGGGRRRR
	  * --> 1111111011101110
	  */
	Pixel blendMask;

	// TODO: These are workarounds for utility classes that should be
	//       centralized somehow.
	friend class Scanline<Pixel>;
	friend class Simple2xScaler<Pixel>;
	friend class TransparentScaler<Pixel>;
	friend class Blur_1on3<Pixel>;
};



template <typename Pixel>
PixelOperations<Pixel>::PixelOperations(const SDL_PixelFormat& format_)
	: format(&format_)
{
	int rBit = ~(format->Rmask << 1) & format->Rmask;
	int gBit = ~(format->Gmask << 1) & format->Gmask;
	int bBit = ~(format->Bmask << 1) & format->Bmask;
	blendMask = ~(rBit | gBit | bBit);
}


template <typename Pixel>
inline unsigned PixelOperations<Pixel>::red(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> format->Rshift) & 0xFF;
	} else {
		return (p & format->Rmask) >> format->Rshift;
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::green(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> format->Gshift) & 0xFF;
	} else {
		return (p & format->Gmask) >> format->Gshift;
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::blue(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> format->Bshift) & 0xFF;
	} else {
		return (p & format->Bmask) >> format->Bshift;
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::alpha(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> format->Ashift) & 0xFF;
	} else {
		assert(false);
		//return (p & format->Amask) >> format->Ashift;
	}
}

template <typename Pixel>
inline unsigned PixelOperations<Pixel>::red256(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> format->Rshift) & 0xFF;
	} else {
		return ((p >> format->Rshift) << format->Rloss) & 0xFF;
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::green256(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> format->Gshift) & 0xFF;
	} else {
		return ((p >> format->Gshift) << format->Gloss) & 0xFF;
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::blue256(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> format->Bshift) & 0xFF;
	} else {
		return ((p >> format->Bshift) << format->Bloss) & 0xFF;
	}
}

template <typename Pixel>
inline Pixel PixelOperations<Pixel>::combine(
		unsigned r, unsigned g, unsigned b) const
{
	return Pixel((r << format->Rshift) |
	             (g << format->Gshift) |
	             (b << format->Bshift));
}

template <typename Pixel>
inline Pixel PixelOperations<Pixel>::combine256(
		unsigned r, unsigned g, unsigned b) const
{
	if (sizeof(Pixel) == 4) {
		return Pixel((r << format->Rshift) |
		             (g << format->Gshift) |
		             (b << format->Bshift));
	} else {
		return Pixel(((r >> format->Rloss) << format->Rshift) |
		             ((g >> format->Gloss) << format->Gshift) |
		             ((b >> format->Bloss) << format->Bshift));
	}
}

template <typename Pixel>
inline unsigned PixelOperations<Pixel>::getMaxRed() const
{
	if (sizeof(Pixel) == 4) {
		return 255;
	} else {
		return 255 >> format->Rloss;
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::getMaxGreen() const
{
	if (sizeof(Pixel) == 4) {
		return 255;
	} else {
		return 255 >> format->Gloss;
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::getMaxBlue() const
{
	if (sizeof(Pixel) == 4) {
		return 255;
	} else {
		return 255 >> format->Bloss;
	}
}

template<unsigned N> struct IsPow2 {
	static const bool result = ((N & 1) == 0) && IsPow2<N / 2>::result;
	static const unsigned log2 = 1 + IsPow2<N / 2>::log2;
};
template<> struct IsPow2<1> {
	static const bool result = true;
	static const unsigned log2 = 0;
};


template<typename Pixel>
inline Pixel PixelOperations<Pixel>::avgDown(Pixel p1, Pixel p2) const
{
	// Average can be calculated as:
	//    floor((x + y) / 2.0) = (x & y) + (x ^ y) / 2
	// see "Average of Integers" on http://aggregate.org/MAGIC/
	Pixel mask = (sizeof(Pixel) == 4) ? 0xFEFEFEFE : blendMask;
	return (p1 & p2) + (((p1 ^ p2) & mask) >> 1);
}
template<typename Pixel>
inline Pixel PixelOperations<Pixel>::avgUp(Pixel p1, Pixel p2) const
{
	// Similar to above, but rounds up
	//    ceil((x + y) / 2.0) = (x | y) - (x ^ y) / 2
	Pixel mask = (sizeof(Pixel) == 4) ? 0xFEFEFEFE : blendMask;
	return (p1 | p2) - (((p1 ^ p2) & mask) >> 1);
}

template<typename Pixel>
template<unsigned w1, unsigned w2>
inline Pixel PixelOperations<Pixel>::blend(Pixel p1, Pixel p2) const
{
	if (w1 > w2) {
		return blend<w2, w1>(p2, p1);
	} else if (w1 == w2) {
		// <1, 1>
		return avgDown(p1, p2);
	} else if ((3 * w1) == w2) {
		// <1, 3>
		Pixel p11 = avgDown(p1, p2);
		return avgUp(p11, p2);
	} else if ((7 * w1) == w2) {
		// <1, 7>
		Pixel p11 = avgDown(p1, p2);
		Pixel p13 = avgDown(p11, p2);
		return avgUp(p13, p2);
	} else if ((5 * w1) == (3 * w2)) {
		// <3, 5>   mix rounding up/down to get a more accurate result
		Pixel p11 = avgUp  (p1, p2);
		Pixel p13 = avgDown(p11, p2);
		return avgDown(p11, p13);
	} else if ((2 * w1) == w2) {
		// <1, 2>
		Pixel a1b1    = avgUp  (p1,     p2);
		Pixel a1b3    = avgDown(a1b1,   p2);
		Pixel a5b3    = avgUp  (a1b3,   p2);
		Pixel a5b11   = avgDown(a5b3,   p2);
		Pixel a21b11  = avgUp  (a5b11,  p1);
		Pixel a21b43  = avgDown(a21b11, p2);
		if (sizeof(Pixel) == 4) {
			// approx as <85, 171>, enough for 8 bit precision
			Pixel a85b43  = avgUp  (a21b43, p1);
			Pixel a85b171 = avgDown(a85b43, p2);
			return a85b171;
		} else {
			// approx as <21, 43>, enough for 6 bit precision
			return a21b43;
		}
	} else {
		static const unsigned total = w1 + w2;
		if ((sizeof(Pixel) == 4) && IsPow2<total>::result) {
			unsigned l2 = IsPow2<total>::log2;
			unsigned c1 = (((p1 & 0x00FF00FF) * w1 +
			                (p2 & 0x00FF00FF) * w2
			               ) >> l2) & 0x00FF00FF;
			unsigned c2 = (((p1 & 0xFF00FF00) >> l2) * w1 +
			               ((p2 & 0xFF00FF00) >> l2) * w2
			              ) & 0xFF00FF00;
			return c1 | c2;
		} else {
			unsigned r = (red  (p1) * w1 + red  (p2) * w2) / total;
			unsigned g = (green(p1) * w1 + green(p2) * w2) / total;
			unsigned b = (blue (p1) * w1 + blue (p2) * w2) / total;
			return combine(r, g, b);
		}
	}
}

template <typename Pixel>
template <unsigned w1, unsigned w2, unsigned w3>
inline Pixel PixelOperations<Pixel>::blend(Pixel p1, Pixel p2, Pixel p3) const
{
	static const unsigned total = w1 + w2 + w3;
	if ((sizeof(Pixel) == 4) && IsPow2<total>::result) {
		unsigned l2 = IsPow2<total>::log2;
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

template <typename Pixel>
template <unsigned w1, unsigned w2, unsigned w3, unsigned w4>
inline Pixel PixelOperations<Pixel>::blend(
		Pixel p1, Pixel p2, Pixel p3, Pixel p4) const
{
	static const unsigned total = w1 + w2 + w3 + w4;
	if ((sizeof(Pixel) == 4) && IsPow2<total>::result) {
		unsigned l2 = IsPow2<total>::log2;
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

template <typename Pixel>
template <unsigned w1, unsigned w2, unsigned w3,
          unsigned w4, unsigned w5, unsigned w6>
inline Pixel PixelOperations<Pixel>::blend(
	Pixel p1, Pixel p2, Pixel p3, Pixel p4, Pixel p5, Pixel p6) const
{
	static const unsigned total = w1 + w2 + w3 + w4 + w5 + w6;
	if ((sizeof(Pixel) == 4) && IsPow2<total>::result) {
		unsigned l2 = IsPow2<total>::log2;
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


template <typename Pixel>
template <unsigned w1, unsigned w2>
inline Pixel PixelOperations<Pixel>::blend2(const Pixel* p) const
{
	return blend<w1, w2>(p[0], p[1]);
}

template <typename Pixel>
template <unsigned w1, unsigned w2, unsigned w3>
inline Pixel PixelOperations<Pixel>::blend3(const Pixel* p) const
{
	return blend<w1, w2, w3>(p[0], p[1], p[2]);
}

template <typename Pixel>
template <unsigned w1, unsigned w2, unsigned w3, unsigned w4>
inline Pixel PixelOperations<Pixel>::blend4(const Pixel* p) const
{
	return blend<w1, w2, w3, w4>(p[0], p[1], p[2], p[3]);
}

template <typename Pixel>
template <unsigned w1, unsigned w2, unsigned w3,
          unsigned w4, unsigned w5, unsigned w6>
inline Pixel PixelOperations<Pixel>::blend6(const Pixel* p) const
{
	return blend<w1, w2, w3, w4, w5, w6>(p[0], p[1], p[2], p[3], p[4], p[5]);
}


template <typename Pixel>
inline Pixel PixelOperations<Pixel>::getBlendMask() const
{
	return blendMask;
}

template <typename Pixel>
inline Pixel PixelOperations<Pixel>::alphaBlend(Pixel p1, Pixel p2) const
{
	if (sizeof(Pixel) == 2) {
		// TODO keep magic value in sync with OutputSurface::getKeyColor()
		return (p1 == 0x0001) ? p2 : p1;
	} else {
		unsigned a = alpha(p1);

		unsigned a1 = p1 & 0x000000FF;
		unsigned b1 = p2 & 0x000000FF;
		int      d1 = a1 - b1;
		unsigned e1 = (((a * d1) >> 8) + b1) & 0x000000FF;

		unsigned a2 = p1 & 0x0000FF00;
		unsigned b2 = p2 & 0x0000FF00;
		int      d2 = a2 - b2;
		unsigned e2 = (((a * d2) >> 8) + b2) & 0x0000FF00;

		unsigned a3 = p1 & 0x00FF0000;
		unsigned b3 = p2 & 0x00FF0000;
		int      d3 = a3 - b3;
		unsigned e3 = (((a * d3) >> 8) + b3) & 0x00FF0000;

		unsigned a4 = (p1 >> 8) & 0x00FF0000;
		unsigned b4 = (p2 >> 8) & 0x00FF0000;
		int      d4 = a4 - b4;
		unsigned e4 = ((a * d4) + (b4 << 8)) & 0xFF000000;

		return e1 | e2 | e3 | e4;
	}
}

} // namespace openmsx

#endif
