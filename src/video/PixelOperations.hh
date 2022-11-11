#ifndef PIXELOPERATIONS_HH
#define PIXELOPERATIONS_HH

#include "PixelFormat.hh"
#include "narrow.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include <bit>
#include <concepts>
#include <span>

namespace openmsx {

template<std::unsigned_integral Pixel> class PixelOpBase
{
public:
	explicit PixelOpBase(const PixelFormat& format_)
		: format(format_)
		, blendMask(calcBlendMask())
	{
	}

	[[nodiscard]] const PixelFormat& getPixelFormat() const { return format; }

	[[nodiscard]] inline unsigned getRmask()  const { return format.getRmask();  }
	[[nodiscard]] inline unsigned getGmask()  const { return format.getGmask();  }
	[[nodiscard]] inline unsigned getBmask()  const { return format.getBmask();  }
	[[nodiscard]] inline unsigned getAmask()  const { return format.getAmask();  }
	[[nodiscard]] inline unsigned getRshift() const { return format.getRshift(); }
	[[nodiscard]] inline unsigned getGshift() const { return format.getGshift(); }
	[[nodiscard]] inline unsigned getBshift() const { return format.getBshift(); }
	[[nodiscard]] inline unsigned getAshift() const { return format.getAshift(); }
	[[nodiscard]] inline unsigned getRloss()  const { return format.getRloss();  }
	[[nodiscard]] inline unsigned getGloss()  const { return format.getGloss();  }
	[[nodiscard]] inline unsigned getBloss()  const { return format.getBloss();  }
	[[nodiscard]] inline unsigned getAloss()  const { return format.getAloss();  }

	/** Returns a constant that is useful to calculate the average of
	  * two pixel values. See the implementation of blend(p1, p2) for
	  * more details.
	  * For single pixels it's of course better to use the blend(p1, p2)
	  * method directly. This method is typically used as a helper in
	  * older SIMD (MMX/SSE1) routines.
	  */
	[[nodiscard]] inline Pixel getBlendMask() const { return blendMask; }

	/** Return true if it's statically known that the pixelformat has
	  * a 5-6-5 format (not specified which component goes where, but
	  * usually it will be BGR). This method is currently used to pick
	  * a faster version for lerp() on dingoo. */
	static constexpr bool IS_RGB565 = false;

private:
	[[nodiscard]] inline Pixel calcBlendMask() const
	{
		auto rBit = ~(getRmask() << 1) & getRmask();
		auto gBit = ~(getGmask() << 1) & getGmask();
		auto bBit = ~(getBmask() << 1) & getBmask();
		return ~(rBit | gBit | bBit);
	}

private:
	const PixelFormat& format;

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
	explicit PixelOpBase(const PixelFormat& format_)
		: format(format_) {}

	[[nodiscard]] const PixelFormat& getPixelFormat() const { return format; }

	[[nodiscard]] inline unsigned getRmask()  const { return format.getRmask();  }
	[[nodiscard]] inline unsigned getGmask()  const { return format.getGmask();  }
	[[nodiscard]] inline unsigned getBmask()  const { return format.getBmask();  }
	[[nodiscard]] inline unsigned getAmask()  const { return format.getAmask();  }
	[[nodiscard]] inline unsigned getRshift() const { return format.getRshift(); }
	[[nodiscard]] inline unsigned getGshift() const { return format.getGshift(); }
	[[nodiscard]] inline unsigned getBshift() const { return format.getBshift(); }
	[[nodiscard]] inline unsigned getAshift() const { return format.getAshift(); }
	[[nodiscard]] inline unsigned getRloss()  const { return 0;             }
	[[nodiscard]] inline unsigned getGloss()  const { return 0;             }
	[[nodiscard]] inline unsigned getBloss()  const { return 0;             }
	[[nodiscard]] inline unsigned getAloss()  const { return 0;             }

	[[nodiscard]] inline unsigned getBlendMask() const { return 0xFEFEFEFE; }

	static constexpr bool IS_RGB565 = false;

private:
	const PixelFormat& format;
};


#if PLATFORM_DINGUX
// Specialization for dingoo (16bpp)
//   We know the exact pixel format for this platform. No need for any
//   members in this class. All values can also be compile-time constant.
template<> class PixelOpBase<uint16_t>
{
public:
	explicit PixelOpBase(const PixelFormat& /*format*/) {}

	[[nodiscard]] const PixelFormat& getPixelFormat() const
	{
		static PixelFormat format(16,
			0x001F,  0, 3,
			0x07E0,  5, 2,
			0xF800, 11, 3,
			0x0000,  0, 8);
		return format;
	}

	[[nodiscard]] inline unsigned getRmask()  const { return 0x001F; }
	[[nodiscard]] inline unsigned getGmask()  const { return 0x07E0; }
	[[nodiscard]] inline unsigned getBmask()  const { return 0xF800; }
	[[nodiscard]] inline unsigned getAmask()  const { return 0x0000; }
	[[nodiscard]] inline unsigned getRshift() const { return  0; }
	[[nodiscard]] inline unsigned getGshift() const { return  5; }
	[[nodiscard]] inline unsigned getBshift() const { return 11; }
	[[nodiscard]] inline unsigned getAshift() const { return  0; }
	[[nodiscard]] inline unsigned getRloss()  const { return 3; }
	[[nodiscard]] inline unsigned getGloss()  const { return 2; }
	[[nodiscard]] inline unsigned getBloss()  const { return 3; }
	[[nodiscard]] inline unsigned getAloss()  const { return 8; }

	[[nodiscard]] inline uint16_t getBlendMask() const { return 0xF7DE; }

	static constexpr bool IS_RGB565 = true;
};
#endif



template<std::unsigned_integral Pixel> class PixelOperations : public PixelOpBase<Pixel>
{
public:
	using PixelOpBase<Pixel>::getPixelFormat;
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

	explicit PixelOperations(const PixelFormat& format);

	/** Extract RGB components
	  */
	[[nodiscard]] inline unsigned red(Pixel p) const;
	[[nodiscard]] inline unsigned green(Pixel p) const;
	[[nodiscard]] inline unsigned blue(Pixel p) const;
	[[nodiscard]] inline unsigned alpha(Pixel p) const;

	// alpha is maximum
	[[nodiscard]] inline bool isFullyOpaque(Pixel p) const;
	// alpha is minimum
	[[nodiscard]] inline bool isFullyTransparent(Pixel p) const;

	/** Same as above, but result is scaled to [0..255]
	  */
	[[nodiscard]] inline unsigned red256(Pixel p) const;
	[[nodiscard]] inline unsigned green256(Pixel p) const;
	[[nodiscard]] inline unsigned blue256(Pixel p) const;

	/** Combine RGB components to a pixel
	  */
	[[nodiscard]] inline Pixel combine(unsigned r, unsigned g, unsigned b) const;
	[[nodiscard]] inline Pixel combine256(unsigned r, unsigned g, unsigned b) const;

	/** Get maximum component value
	  */
	[[nodiscard]] inline unsigned getMaxRed() const;
	[[nodiscard]] inline unsigned getMaxGreen() const;
	[[nodiscard]] inline unsigned getMaxBlue() const;

	/** Blend the given colors into a single color.
	  * The special case for blending between two colors with
	  * an equal blend weight has an optimized implementation.
	  */
	template<unsigned w1, unsigned w2>
	[[nodiscard]] inline Pixel blend(Pixel p1, Pixel p2) const;
	template<unsigned w1, unsigned w2, unsigned w3>
	[[nodiscard]] inline Pixel blend(Pixel p1, Pixel p2, Pixel p3) const;
	template<unsigned w1, unsigned w2, unsigned w3, unsigned w4>
	[[nodiscard]] inline Pixel blend(Pixel p1, Pixel p2, Pixel p3, Pixel p4) const;
	template<unsigned w1, unsigned w2, unsigned w3,
	         unsigned w4, unsigned w5, unsigned w6>
	[[nodiscard]] inline Pixel blend(Pixel p1, Pixel p2, Pixel p3,
	                                 Pixel p4, Pixel p5, Pixel p6) const;

	template<unsigned w1, unsigned w2>
	[[nodiscard]] inline Pixel blend(std::span<const Pixel, 2> p) const;
	template<unsigned w1, unsigned w2, unsigned w3>
	[[nodiscard]] inline Pixel blend(std::span<const Pixel, 3> p) const;
	template<unsigned w1, unsigned w2, unsigned w3, unsigned w4>
	[[nodiscard]] inline Pixel blend(std::span<const Pixel, 4> p) const;
	template<unsigned w1, unsigned w2, unsigned w3,
	          unsigned w4, unsigned w5, unsigned w6>
	[[nodiscard]] inline Pixel blend(std::span<const Pixel, 6> p) const;

	/** Perform a component wise multiplication of a pixel with an 8-bit
	  * fractional value:
	  *     result = (pixel * x) / 256  [component wise]
	  * 'x' must be in range [0..256].
	  * For x=0   the result is 0.
	  * For x=255 the result in the original value.
	  * Note: ATM only implemented for 32bpp.
	  */
	[[nodiscard]] static inline Pixel multiply(Pixel p, unsigned x);

	/** Perform linear interpolation between two pixels.
	 * This calculates component-wise:
	 *   (c1 * (256 - x) + c2 * x) / 256
	 * with c1, c2 the R,B,G,A components of the pixel.
	 * 'x' must be in range [0..256].
	 * For x=0   the result is p1.
	 * For x=256 the result is p2.
	 */
	[[nodiscard]] inline Pixel lerp(Pixel p1, Pixel p2, unsigned x) const;

	/** Perform alpha blending of two pixels.
	 * Pixel p1 contains the alpha value. For maximal alpha p1 is
	 * returned, for minimal alpha p2.
	 */
	[[nodiscard]] inline Pixel alphaBlend(Pixel p1, Pixel p2) const;

private:
	[[nodiscard]] inline Pixel avgDown(Pixel p1, Pixel p2) const;
	[[nodiscard]] inline Pixel avgUp  (Pixel p1, Pixel p2) const;
};


template<std::unsigned_integral Pixel>
PixelOperations<Pixel>::PixelOperations(const PixelFormat& format_)
	: PixelOpBase<Pixel>(format_)
{
}

template<std::unsigned_integral Pixel>
inline unsigned PixelOperations<Pixel>::red(Pixel p) const
{
	if constexpr (sizeof(Pixel) == 4) {
		return (p >> getRshift()) & 0xFF;
	} else {
		return (p & getRmask()) >> getRshift();
	}
}
template<std::unsigned_integral Pixel>
inline unsigned PixelOperations<Pixel>::green(Pixel p) const
{
	if constexpr (sizeof(Pixel) == 4) {
		return (p >> getGshift()) & 0xFF;
	} else {
		return (p & getGmask()) >> getGshift();
	}
}
template<std::unsigned_integral Pixel>
inline unsigned PixelOperations<Pixel>::blue(Pixel p) const
{
	if constexpr (sizeof(Pixel) == 4) {
		return (p >> getBshift()) & 0xFF;
	} else {
		return (p & getBmask()) >> getBshift();
	}
}
template<std::unsigned_integral Pixel>
inline unsigned PixelOperations<Pixel>::alpha(Pixel p) const
{
	if constexpr (sizeof(Pixel) == 4) {
		return (p >> getAshift()) & 0xFF;
	} else {
		UNREACHABLE; return 0;
		//return (p & getAmask()) >> getAshift();
	}
}

template<std::unsigned_integral Pixel>
inline bool PixelOperations<Pixel>::isFullyOpaque(Pixel p) const
{
	if constexpr (sizeof(Pixel) == 4) {
		return alpha(p) == 255;
	} else {
		return p != 0x0001;
	}
}
template<std::unsigned_integral Pixel>
inline bool PixelOperations<Pixel>::isFullyTransparent(Pixel p) const
{
	if constexpr (sizeof(Pixel) == 4) {
		return alpha(p) == 0;
	} else {
		return p == 0x0001;
	}
}

template<std::unsigned_integral Pixel>
inline unsigned PixelOperations<Pixel>::red256(Pixel p) const
{
	if constexpr (sizeof(Pixel) == 4) {
		return (p >> getRshift()) & 0xFF;
	} else {
		return ((p >> getRshift()) << getRloss()) & 0xFF;
	}
}
template<std::unsigned_integral Pixel>
inline unsigned PixelOperations<Pixel>::green256(Pixel p) const
{
	if constexpr (sizeof(Pixel) == 4) {
		return (p >> getGshift()) & 0xFF;
	} else {
		return ((p >> getGshift()) << getGloss()) & 0xFF;
	}
}
template<std::unsigned_integral Pixel>
inline unsigned PixelOperations<Pixel>::blue256(Pixel p) const
{
	if constexpr (sizeof(Pixel) == 4) {
		return (p >> getBshift()) & 0xFF;
	} else {
		return ((p >> getBshift()) << getBloss()) & 0xFF;
	}
}

template<std::unsigned_integral Pixel>
inline Pixel PixelOperations<Pixel>::combine(
		unsigned r, unsigned g, unsigned b) const
{
	return Pixel((r << getRshift()) |
	             (g << getGshift()) |
	             (b << getBshift()));
}

template<std::unsigned_integral Pixel>
inline Pixel PixelOperations<Pixel>::combine256(
		unsigned r, unsigned g, unsigned b) const
{
	if constexpr (sizeof(Pixel) == 4) {
		return Pixel((r << getRshift()) |
		             (g << getGshift()) |
		             (b << getBshift()));
	} else {
		return Pixel(((r >> getRloss()) << getRshift()) |
		             ((g >> getGloss()) << getGshift()) |
		             ((b >> getBloss()) << getBshift()));
	}
}

template<std::unsigned_integral Pixel>
inline unsigned PixelOperations<Pixel>::getMaxRed() const
{
	if constexpr (sizeof(Pixel) == 4) {
		return 255;
	} else {
		return 255 >> getRloss();
	}
}
template<std::unsigned_integral Pixel>
inline unsigned PixelOperations<Pixel>::getMaxGreen() const
{
	if constexpr (sizeof(Pixel) == 4) {
		return 255;
	} else {
		return 255 >> getGloss();
	}
}
template<std::unsigned_integral Pixel>
inline unsigned PixelOperations<Pixel>::getMaxBlue() const
{
	if constexpr (sizeof(Pixel) == 4) {
		return 255;
	} else {
		return 255 >> getBloss();
	}
}

template<std::unsigned_integral Pixel>
inline Pixel PixelOperations<Pixel>::avgDown(Pixel p1, Pixel p2) const
{
	// Average can be calculated as:
	//    floor((x + y) / 2.0) = (x & y) + (x ^ y) / 2
	// see "Average of Integers" on http://aggregate.org/MAGIC/
	return (p1 & p2) + (((p1 ^ p2) & getBlendMask()) >> 1);
}
template<std::unsigned_integral Pixel>
inline Pixel PixelOperations<Pixel>::avgUp(Pixel p1, Pixel p2) const
{
	// Similar to above, but rounds up
	//    ceil((x + y) / 2.0) = (x | y) - (x ^ y) / 2
	return (p1 | p2) - (((p1 ^ p2) & getBlendMask()) >> 1);
}

template<std::unsigned_integral Pixel>
template<unsigned w1, unsigned w2>
inline Pixel PixelOperations<Pixel>::blend(Pixel p1, Pixel p2) const
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
		// approximate with weights that sum to 256 (or 64)
		// e.g. approximate <1,2> as <85,171> (or <21,43>)
		//  ww1 = round(256 * w1 / total)   ww2 = 256 - ww1
		constexpr unsigned newTotal = IS_RGB565 ? 64 : 256;
		constexpr unsigned ww1 = (2 * w1 * newTotal + total) / (2 * total);
		constexpr unsigned ww2 = 256 - ww1;
		return blend<ww1, ww2>(p1, p2);

	} else if constexpr (sizeof(Pixel) == 4) {
		constexpr unsigned l2 = std::bit_width(total) - 1;
		unsigned c1 = (((p1 & 0x00FF00FF) * w1 +
				(p2 & 0x00FF00FF) * w2
			       ) >> l2) & 0x00FF00FF;
		unsigned c2 = (((p1 & 0xFF00FF00) >> l2) * w1 +
			       ((p2 & 0xFF00FF00) >> l2) * w2
			      ) & 0xFF00FF00;
		return c1 | c2;

	} else if constexpr (IS_RGB565) {
		if constexpr (total > 64) {
			// reduce to maximum 6-bit
			// note: DIV64 only exists to work around a
			//       division by zero in dead code
			constexpr unsigned DIV64 = (total > 64) ? 64 : 1;
			constexpr unsigned factor = total / DIV64;
			constexpr unsigned round = factor / 2;
			constexpr unsigned ww1 = (w1 + round) / factor;
			constexpr unsigned ww2 = 64 - ww1;
			return blend<ww1, ww2>(p1, p2);
		} else {
			unsigned l2 = std::bit_width(total) - 1;
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

template<std::unsigned_integral Pixel>
template<unsigned w1, unsigned w2, unsigned w3>
inline Pixel PixelOperations<Pixel>::blend(Pixel p1, Pixel p2, Pixel p3) const
{
	constexpr unsigned total = w1 + w2 + w3;
	if constexpr ((sizeof(Pixel) == 4) && std::has_single_bit(total)) {
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

template<std::unsigned_integral Pixel>
template<unsigned w1, unsigned w2, unsigned w3, unsigned w4>
inline Pixel PixelOperations<Pixel>::blend(
		Pixel p1, Pixel p2, Pixel p3, Pixel p4) const
{
	constexpr unsigned total = w1 + w2 + w3 + w4;
	if constexpr ((sizeof(Pixel) == 4) && std::has_single_bit(total)) {
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

template<std::unsigned_integral Pixel>
template<unsigned w1, unsigned w2, unsigned w3,
          unsigned w4, unsigned w5, unsigned w6>
inline Pixel PixelOperations<Pixel>::blend(
	Pixel p1, Pixel p2, Pixel p3, Pixel p4, Pixel p5, Pixel p6) const
{
	constexpr unsigned total = w1 + w2 + w3 + w4 + w5 + w6;
	if constexpr ((sizeof(Pixel) == 4) && std::has_single_bit(total)) {
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


template<std::unsigned_integral Pixel>
template<unsigned w1, unsigned w2>
inline Pixel PixelOperations<Pixel>::blend(std::span<const Pixel, 2> p) const
{
	return blend<w1, w2>(p[0], p[1]);
}

template<std::unsigned_integral Pixel>
template<unsigned w1, unsigned w2, unsigned w3>
inline Pixel PixelOperations<Pixel>::blend(std::span<const Pixel, 3> p) const
{
	return blend<w1, w2, w3>(p[0], p[1], p[2]);
}

template<std::unsigned_integral Pixel>
template<unsigned w1, unsigned w2, unsigned w3, unsigned w4>
inline Pixel PixelOperations<Pixel>::blend(std::span<const Pixel, 4> p) const
{
	return blend<w1, w2, w3, w4>(p[0], p[1], p[2], p[3]);
}

template<std::unsigned_integral Pixel>
template<unsigned w1, unsigned w2, unsigned w3,
          unsigned w4, unsigned w5, unsigned w6>
inline Pixel PixelOperations<Pixel>::blend(std::span<const Pixel, 6> p) const
{
	return blend<w1, w2, w3, w4, w5, w6>(p[0], p[1], p[2], p[3], p[4], p[5]);
}

template<std::unsigned_integral Pixel>
inline Pixel PixelOperations<Pixel>::multiply(Pixel p, unsigned x)
{
	if constexpr (sizeof(Pixel) == 4) {
		return ((((p       & 0x00FF00FF) * x) & 0xFF00FF00) >> 8)
		     | ((((p >> 8) & 0x00FF00FF) * x) & 0xFF00FF00);
	} else {
		UNREACHABLE; return 0;
	}
}

template<std::unsigned_integral Pixel>
inline Pixel PixelOperations<Pixel>::lerp(Pixel p1, Pixel p2, unsigned x) const
{
	if constexpr (sizeof(Pixel) == 4) { // 32 bpp
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

	} else if constexpr (IS_RGB565) {
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
		auto r = narrow<unsigned>(((r2 - r1) * narrow<int>(x)) / 256 + r1);
		auto g = narrow<unsigned>(((g2 - g1) * narrow<int>(x)) / 256 + g1);
		auto b = narrow<unsigned>(((b2 - b1) * narrow<int>(x)) / 256 + b1);

		return combine(r, g, b);
	}
}

template<std::unsigned_integral Pixel>
inline Pixel PixelOperations<Pixel>::alphaBlend(Pixel p1, Pixel p2) const
{
	if constexpr (sizeof(Pixel) == 2) {
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
