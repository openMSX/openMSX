// $Id$

#ifndef PIXELOPERATIONS_HH
#define PIXELOPERATIONS_HH

#include <SDL.h>

namespace openmsx {

// TODO optimizations for Pixel=unsigned

template <typename Pixel> 
class PixelOperations
{
public:
	explicit PixelOperations(const SDL_PixelFormat* format_);

	/** Extract RGB componts
	  */
	inline unsigned red(Pixel p) const;
	inline unsigned green(Pixel p) const;
	inline unsigned blue(Pixel p) const;

	/** Same as above, but result is scaled to [0..255]
	  */
	inline unsigned red256(Pixel p) const;
	inline unsigned green256(Pixel p) const;
	inline unsigned blue256(Pixel p) const;

	/** Combine RGB components to a pixel
	  */
	inline Pixel combine(unsigned r, unsigned g, unsigned b) const;
	inline Pixel combine256(unsigned r, unsigned g, unsigned b) const;
	
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

	template <unsigned w1, unsigned w2>
	inline Pixel blend2(const Pixel* p) const;
	template <unsigned w1, unsigned w2, unsigned w3>
	inline Pixel blend3(const Pixel* p) const;
	template <unsigned w1, unsigned w2, unsigned w3, unsigned w4>
	inline Pixel blend4(const Pixel* p) const;
	
	inline Pixel getBlendMask() const;

private:
	/** Mask used for blending.
	  * The least significant bit of R,G,B must be 1,
	  * all other bits must be 0.
	  *     0000BBBBGGGGRRRR
	  * --> 0000000100010001
	  */
	const SDL_PixelFormat* format;
	Pixel blendMask;
};



template <typename Pixel>
PixelOperations<Pixel>::PixelOperations(const SDL_PixelFormat* format_)
	: format(format_)
{
	int rBit = ~(format->Rmask << 1) & format->Rmask;
	int gBit = ~(format->Gmask << 1) & format->Gmask;
	int bBit = ~(format->Bmask << 1) & format->Bmask;
	blendMask = rBit | gBit | bBit;
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
	return (Pixel)((r << format->Rshift) |
	               (g << format->Gshift) |
	               (b << format->Bshift));
}

template <typename Pixel>
inline Pixel PixelOperations<Pixel>::combine256(
		unsigned r, unsigned g, unsigned b) const
{
	if (sizeof(Pixel) == 4) {
		return (Pixel)((r << format->Rshift) |
		               (g << format->Gshift) |
		               (b << format->Bshift));
	} else {
		return (Pixel)(((r >> format->Rloss) << format->Rshift) |
		               ((g >> format->Gloss) << format->Gshift) |
		               ((b >> format->Bloss) << format->Bshift));
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

template <typename Pixel>
template <unsigned w1, unsigned w2>
inline Pixel PixelOperations<Pixel>::blend(Pixel p1, Pixel p2) const
{
	if (w1 == w2) {
		return ((p1 & ~blendMask) >> 1) +
		       ((p2 & ~blendMask) >> 1) +
			(p1 &  blendMask);
	} else {
		static const unsigned total = w1 + w2;
		if ((sizeof(Pixel) == 4) && IsPow2<total>::result) {
			unsigned l2 = IsPow2<total>::log2;
			unsigned rb = ((p1 & 0xFF00FF) * w1 +
			               (p2 & 0xFF00FF) * w2) & (0xFF00FF << l2);
			unsigned g  = ((p1 & 0x00FF00) * w1 +
			               (p2 & 0x00FF00) * w2) & (0x00FF00 << l2);
			return (rb | g) >> l2;
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
		unsigned rb = ((p1 & 0xFF00FF) * w1 +
		               (p2 & 0xFF00FF) * w2 +
		               (p3 & 0xFF00FF) * w3) & (0xFF00FF << l2);
		unsigned g  = ((p1 & 0x00FF00) * w1 +
		               (p2 & 0x00FF00) * w2 +
		               (p3 & 0x00FF00) * w3) & (0x00FF00 << l2);
		return (rb | g) >> l2;
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
		unsigned rb = ((p1 & 0xFF00FF) * w1 +
		               (p2 & 0xFF00FF) * w2 +
		               (p3 & 0xFF00FF) * w3 +
		               (p4 & 0xFF00FF) * w4) & (0xFF00FF << l2);
		unsigned g  = ((p1 & 0x00FF00) * w1 +
		               (p2 & 0x00FF00) * w2 +
		               (p3 & 0x00FF00) * w3 +
		               (p4 & 0x00FF00) * w4) & (0x00FF00 << l2);
		return (rb | g) >> l2;
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
inline Pixel PixelOperations<Pixel>::getBlendMask() const
{
	return blendMask;
}

} // namespace openmsx

#endif
