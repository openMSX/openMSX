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

	/** Combine RGB components to a pixel
	  */
	inline Pixel combine(unsigned r, unsigned g, unsigned b) const;
	
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
	return (p & format->Rmask) >> format->Rshift;
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::green(Pixel p) const
{
	return (p & format->Gmask) >> format->Gshift;
}
template <typename Pixel>
inline unsigned PixelOperations<Pixel>::blue(Pixel p) const
{
	return (p & format->Bmask) >> format->Bshift;
}

template <typename Pixel>
inline Pixel PixelOperations<Pixel>::combine(unsigned r, unsigned g, unsigned b) const
{
	return (Pixel)((r << format->Rshift) |
		       (g << format->Gshift) |
		       (b << format->Bshift));
}


template <typename Pixel>
template <unsigned w1, unsigned w2>
inline Pixel PixelOperations<Pixel>::blend(Pixel p1, Pixel p2) const
{
	if (w1 == w2) {
		return ((p1 & ~blendMask) >> 1) +
		       ((p2 & ~blendMask) >> 1) +
			(p1 &  blendMask);
	} else {
		unsigned total = w1 + w2;
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
	unsigned total = w1 + w2 + w3;
	unsigned r = (red  (p1) * w1 + red  (p2) * w2 + red  (p3) * w3) / total;
	unsigned g = (green(p1) * w1 + green(p2) * w2 + green(p3) * w3) / total;
	unsigned b = (blue (p1) * w1 + blue (p2) * w2 + blue (p3) * w3) / total;
	return combine(r, g, b);
}

template <typename Pixel>
template <unsigned w1, unsigned w2, unsigned w3, unsigned w4>
inline Pixel PixelOperations<Pixel>::blend(
		Pixel p1, Pixel p2, Pixel p3, Pixel p4) const
{
	unsigned total = w1 + w2 + w3 + w4;
	unsigned r = (red  (p1) * w1 + red  (p2) * w2 +
	              red  (p3) * w3 + red  (p4) * w4) / total;
	unsigned g = (green(p1) * w1 + green(p2) * w2 +
	              green(p3) * w3 + green(p4) * w4) / total;
	unsigned b = (blue (p1) * w1 + blue (p2) * w2 +
	              blue (p3) * w3 + blue (p4) * w4) / total;
	return combine(r, g, b);
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
