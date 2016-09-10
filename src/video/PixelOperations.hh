#ifndef PIXELOPERATIONS_HH
#define PIXELOPERATIONS_HH

#include "unreachable.hh"
#include "build-info.hh"
#include <SDL.h>

namespace openmsx {

template<typename Pixel> class PixelOpBase
{
public:
	explicit PixelOpBase(const SDL_PixelFormat& format_)
		: format(format_)
		, blendMask(calcBlendMask())
	{
	}

	const SDL_PixelFormat& getSDLPixelFormat() const { return format; }

	inline int getRmask()  const { return format.Rmask;  }
	inline int getGmask()  const { return format.Gmask;  }
	inline int getBmask()  const { return format.Bmask;  }
	inline int getAmask()  const { return format.Amask;  }
	inline int getRshift() const { return format.Rshift; }
	inline int getGshift() const { return format.Gshift; }
	inline int getBshift() const { return format.Bshift; }
	inline int getAshift() const { return format.Ashift; }
	inline int getRloss()  const { return format.Rloss;  }
	inline int getGloss()  const { return format.Gloss;  }
	inline int getBloss()  const { return format.Bloss;  }
	inline int getAloss()  const { return format.Aloss;  }

	/** Returns a constant that is useful to calculate the average of
	  * two pixel values. See the implementation of blend(p1, p2) for
	  * more details.
	  * For single pixels it's of course better to use the blend(p1, p2)
	  * method directly. This method is typically used as a helper in
	  * older SIMD (MMX/SSE1) routines.
	  */
	inline Pixel getBlendMask() const { return blendMask; }

	/** Return true if it's statically known that the pixelformat has
	  * a 5-6-5 format (not specified wihich component goes where, but
	  * usually it will be BGR). This method is currently used to pick
	  * a faster version for lerp() on dingoo. */
	static const bool IS_RGB565 = false;

private:
	inline Pixel calcBlendMask() const
	{
		int rBit = ~(getRmask() << 1) & getRmask();
		int gBit = ~(getGmask() << 1) & getGmask();
		int bBit = ~(getBmask() << 1) & getBmask();
		return ~(rBit | gBit | bBit);
	}

	const SDL_PixelFormat& format;

	/** Mask used for blending.
	  * The least significant bit of R,G,B must be 0,
	  * all other bits must be 1.
	  *     0000BBBBGGGGRRRR
	  * --> 1111111011101110
	  */
	const Pixel blendMask;
};

// Specialization for 32bpp
//  No need to store 'blendMask' in a member variable.
template<> class PixelOpBase<unsigned>
{
public:
	explicit PixelOpBase(const SDL_PixelFormat& format_)
		: format(format_) {}

	const SDL_PixelFormat& getSDLPixelFormat() const { return format; }

	inline int getRmask()  const { return format.Rmask;  }
	inline int getGmask()  const { return format.Gmask;  }
	inline int getBmask()  const { return format.Bmask;  }
	inline int getAmask()  const { return format.Amask;  }
	inline int getRshift() const { return format.Rshift; }
	inline int getGshift() const { return format.Gshift; }
	inline int getBshift() const { return format.Bshift; }
	inline int getAshift() const { return format.Ashift; }
	inline int getRloss()  const { return 0;             }
	inline int getGloss()  const { return 0;             }
	inline int getBloss()  const { return 0;             }
	inline int getAloss()  const { return 0;             }

	inline unsigned getBlendMask() const { return 0xFEFEFEFE; }

	static const bool IS_RGB565 = false;

private:
	const SDL_PixelFormat& format;
};


#if PLATFORM_DINGUX
// Specialization for dingoo (16bpp)
//   We know the exact pixel format for this platform. No need for any
//   members in this class. All values can also be compile-time constant.
template<> class PixelOpBase<uint16_t>
{
public:
	explicit PixelOpBase(const SDL_PixelFormat& /*format*/) {}

	const SDL_PixelFormat& getSDLPixelFormat() const
	{
		static SDL_PixelFormat format;
		format.palette = nullptr;
		format.BitsPerPixel = 16;
		format.BytesPerPixel = 2;
		format.Rloss = 3;
		format.Gloss = 2;
		format.Bloss = 3;
		format.Aloss = 8;
		format.Rshift =  0;
		format.Gshift =  5;
		format.Bshift = 11;
		format.Ashift =  0;
		format.Rmask = 0x001F;
		format.Gmask = 0x07E0;
		format.Bmask = 0xF800;
		format.Amask = 0x0000;
		return format;
	}

	inline int getRmask()  const { return 0x001F; }
	inline int getGmask()  const { return 0x07E0; }
	inline int getBmask()  const { return 0xF800; }
	inline int getAmask()  const { return 0x0000; }
	inline int getRshift() const { return  0; }
	inline int getGshift() const { return  5; }
	inline int getBshift() const { return 11; }
	inline int getAshift() const { return  0; }
	inline int getRloss()  const { return 3; }
	inline int getGloss()  const { return 2; }
	inline int getBloss()  const { return 3; }
	inline int getAloss()  const { return 8; }

	inline uint16_t getBlendMask() const { return 0xF7DE; }

	static const bool IS_RGB565 = true;
};
#endif



template<typename Pixel> class PixelOperations : public PixelOpBase<Pixel>
{
public:
	using PixelOpBase<Pixel>::getSDLPixelFormat;
	using PixelOpBase<Pixel>::getRmask;
	using PixelOpBase<Pixel>::getGmask;
	using PixelOpBase<Pixel>::getBmask;
	using PixelOpBase<Pixel>::getAmask;
	using PixelOpBase<Pixel>::getRshift;
	using PixelOpBase<Pixel>::getGshift;
	using PixelOpBase<Pixel>::getBshift;
	using PixelOpBase<Pixel>::getAshift;
	using PixelOpBase<Pixel>::getRloss;
	using PixelOpBase<Pixel>::getGloss;
	using PixelOpBase<Pixel>::getBloss;
	using PixelOpBase<Pixel>::getAloss;
	using PixelOpBase<Pixel>::getBlendMask;
	using PixelOpBase<Pixel>::IS_RGB565;

	explicit PixelOperations(const SDL_PixelFormat& format);

	/** Extract RGB componts
	  */
	inline unsigned red(Pixel p) const;
	inline unsigned green(Pixel p) const;
	inline unsigned blue(Pixel p) const;
	inline unsigned alpha(Pixel p) const;

	// alpha is maximum
	inline bool isFullyOpaque(Pixel p) const;
	// alpha is minimum
	inline bool isFullyTransparent(Pixel p) const;

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

	/** Perform a component wise multiplication of a pixel with an 8-bit
	  * fractional value:
	  *     result = (pixel * x) / 256  [component wise]
	  * 'x' must be in range [0..256].
	  * For x=0   the result is 0.
	  * For x=255 the result in the original value.
	  * Note: ATM only implemented for 32bpp.
	  */
	static inline Pixel multiply(Pixel p, unsigned x);

	/** Perform linear interpolation between two pixels.
	 * This calculates component-wise:
	 *   (c1 * (256 - x) + c2 * x) / 256
	 * with c1, c2 the R,B,G,A components of the pixel.
	 * 'x' must be in range [0..256].
	 * For x=0   the result is p1.
	 * For x=256 the result is p2.
	 */
	inline Pixel lerp(Pixel p1, Pixel p2, unsigned x) const;

	/** Perform alpha blending of two pixels.
	 * Pixel p1 contains the alpha value. For maximal alpha p1 is
	 * returned, for minimal alpha p2.
	 */
	inline Pixel alphaBlend(Pixel p1, Pixel p2) const;

private:
	inline Pixel avgDown(Pixel p1, Pixel p2) const;
	inline Pixel avgUp  (Pixel p1, Pixel p2) const;
};


template <typename Pixel>
PixelOperations<Pixel>::PixelOperations(const SDL_PixelFormat& format_)
	: PixelOpBase<Pixel>(format_)
{
}

template <typename Pixel>
inline unsigned PixelOperations<Pixel>::red(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> getRshift()) & 0xFF;
	} else {
		return (p & getRmask()) >> getRshift();
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::green(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> getGshift()) & 0xFF;
	} else {
		return (p & getGmask()) >> getGshift();
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::blue(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> getBshift()) & 0xFF;
	} else {
		return (p & getBmask()) >> getBshift();
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::alpha(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> getAshift()) & 0xFF;
	} else {
		UNREACHABLE; return 0;
		//return (p & getAmask()) >> getAshift();
	}
}

template <typename Pixel>
inline bool PixelOperations<Pixel>::isFullyOpaque(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return alpha(p) == 255;
	} else {
		return p != 0x0001;
	}
}
template <typename Pixel>
inline bool PixelOperations<Pixel>::isFullyTransparent(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return alpha(p) == 0;
	} else {
		return p == 0x0001;
	}
}

template <typename Pixel>
inline unsigned PixelOperations<Pixel>::red256(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> getRshift()) & 0xFF;
	} else {
		return ((p >> getRshift()) << getRloss()) & 0xFF;
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::green256(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> getGshift()) & 0xFF;
	} else {
		return ((p >> getGshift()) << getGloss()) & 0xFF;
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::blue256(Pixel p) const
{
	if (sizeof(Pixel) == 4) {
		return (p >> getBshift()) & 0xFF;
	} else {
		return ((p >> getBshift()) << getBloss()) & 0xFF;
	}
}

template <typename Pixel>
inline Pixel PixelOperations<Pixel>::combine(
		unsigned r, unsigned g, unsigned b) const
{
	return Pixel((r << getRshift()) |
	             (g << getGshift()) |
	             (b << getBshift()));
}

template <typename Pixel>
inline Pixel PixelOperations<Pixel>::combine256(
		unsigned r, unsigned g, unsigned b) const
{
	if (sizeof(Pixel) == 4) {
		return Pixel((r << getRshift()) |
		             (g << getGshift()) |
		             (b << getBshift()));
	} else {
		return Pixel(((r >> getRloss()) << getRshift()) |
		             ((g >> getGloss()) << getGshift()) |
		             ((b >> getBloss()) << getBshift()));
	}
}

template <typename Pixel>
inline unsigned PixelOperations<Pixel>::getMaxRed() const
{
	if (sizeof(Pixel) == 4) {
		return 255;
	} else {
		return 255 >> getRloss();
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::getMaxGreen() const
{
	if (sizeof(Pixel) == 4) {
		return 255;
	} else {
		return 255 >> getGloss();
	}
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::getMaxBlue() const
{
	if (sizeof(Pixel) == 4) {
		return 255;
	} else {
		return 255 >> getBloss();
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
	return (p1 & p2) + (((p1 ^ p2) & getBlendMask()) >> 1);
}
template<typename Pixel>
inline Pixel PixelOperations<Pixel>::avgUp(Pixel p1, Pixel p2) const
{
	// Similar to above, but rounds up
	//    ceil((x + y) / 2.0) = (x | y) - (x ^ y) / 2
	return (p1 | p2) - (((p1 ^ p2) & getBlendMask()) >> 1);
}

template<typename Pixel>
template<unsigned w1, unsigned w2>
inline Pixel PixelOperations<Pixel>::blend(Pixel p1, Pixel p2) const
{
	static const unsigned total = w1 + w2;
	if (w1 == 0) {
		return p2;
	} else if (w1 > w2) {
		return blend<w2, w1>(p2, p1);

	} else if (w1 == w2) {
		// <1,1>
		return avgDown(p1, p2);
	} else if ((3 * w1) == w2) {
		// <1,3>
		Pixel p11 = avgDown(p1, p2);
		return avgUp(p11, p2);
	} else if ((7 * w1) == w2) {
		// <1,7>
		Pixel p11 = avgDown(p1, p2);
		Pixel p13 = avgDown(p11, p2);
		return avgUp(p13, p2);
	} else if ((5 * w1) == (3 * w2)) {
		// <3,5>   mix rounding up/down to get a more accurate result
		Pixel p11 = avgUp  (p1, p2);
		Pixel p13 = avgDown(p11, p2);
		return avgDown(p11, p13);

	} else if (!IsPow2<total>::result) {
		// approximate with weights that sum to 256 (or 64)
		// e.g. approximate <1,2> as <85,171> (or <21,43>)
		//  ww1 = round(256 * w1 / total)   ww2 = 256 - ww1
		static const unsigned newTotal = IS_RGB565 ? 64 : 256;
		static const unsigned ww1 = (2 * w1 * newTotal + total) / (2 * total);
		static const unsigned ww2 = 256 - ww1;
		return blend<ww1, ww2>(p1, p2);

	} else if (sizeof(Pixel) == 4) {
		unsigned l2 = IsPow2<total>::log2;
		unsigned c1 = (((p1 & 0x00FF00FF) * w1 +
				(p2 & 0x00FF00FF) * w2
			       ) >> l2) & 0x00FF00FF;
		unsigned c2 = (((p1 & 0xFF00FF00) >> l2) * w1 +
			       ((p2 & 0xFF00FF00) >> l2) * w2
			      ) & 0xFF00FF00;
		return c1 | c2;

	} else if (IS_RGB565) {
		if (total > 64) {
			// reduce to maximum 6-bit
			// note: DIV64 only exists to work around a
			//       division by zero in dead code
			static const unsigned DIV64 = (total > 64) ? 64 : 1;
			static const unsigned factor = total / DIV64;
			static const unsigned round = factor / 2;
			static const unsigned ww1 = (w1 + round) / factor;
			static const unsigned ww2 = 64 - ww1;
			return blend<ww1, ww2>(p1, p2);
		} else {
			unsigned l2 = IsPow2<total>::log2;
			unsigned c1 = (((unsigned(p1) & 0xF81F) * w1) +
			               ((unsigned(p2) & 0xF81F) * w2)) & (0xF81F << l2);
			unsigned c2 = (((unsigned(p1) & 0x07E0) * w1) +
				       ((unsigned(p2) & 0x07E0) * w2)) & (0x07E0 << l2);
			return (c1 | c2) >> l2;
		}

	} else {
		// generic version
		unsigned r = (red  (p1) * w1 + red  (p2) * w2) / total;
		unsigned g = (green(p1) * w1 + green(p2) * w2) / total;
		unsigned b = (blue (p1) * w1 + blue (p2) * w2) / total;
		return combine(r, g, b);
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
inline Pixel PixelOperations<Pixel>::multiply(Pixel p, unsigned x)
{
	if (sizeof(Pixel) == 4) {
		return ((((p       & 0x00FF00FF) * x) & 0xFF00FF00) >> 8)
		     | ((((p >> 8) & 0x00FF00FF) * x) & 0xFF00FF00);
	} else {
		UNREACHABLE; return 0;
	}
}

template <typename Pixel>
inline Pixel PixelOperations<Pixel>::lerp(Pixel p1, Pixel p2, unsigned x) const
{
	if (sizeof(Pixel) == 4) { // 32 bpp
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

	} else if (IS_RGB565) {
		unsigned rb1 = p1 & 0xF81F;
		unsigned rb2 = p2 & 0xF81F;
		unsigned g1  = p1 & 0x07E0;
		unsigned g2  = p2 & 0x07E0;

		x >>= 2;
		unsigned trb = ((rb2 - rb1) * x) >> 6;
		unsigned tg  = ((g2  - g1 ) * x) >> 6;

		unsigned rb = (trb + rb1) & 0xF81F;
		unsigned g  = (tg  + g1 ) & 0x07E0;

		return rb | g;

	} else {
		int r1 = red(p1),   r2 = red(p2);
		int g1 = green(p1), g2 = green(p2);
		int b1 = blue(p1),  b2 = blue(p2);

		// note: '/ 256' is not the same as '>> 8' for signed numbers
		int r = ((r2 - r1) * x) / 256 + r1;
		int g = ((g2 - g1) * x) / 256 + g1;
		int b = ((b2 - b1) * x) / 256 + b1;

		return combine(r, g, b);
	}
}

template <typename Pixel>
inline Pixel PixelOperations<Pixel>::alphaBlend(Pixel p1, Pixel p2) const
{
	if (sizeof(Pixel) == 2) {
		// TODO keep magic value in sync with OutputSurface::getKeyColor()
		return (p1 == 0x0001) ? p2 : p1;
	} else {
		unsigned a = alpha(p1);
		// Note: 'a' is [0..255], while lerp() expects [0..256].
		//       We ignore this small error.
		return lerp(p2, p1, a);
	}
}

} // namespace openmsx

#endif
