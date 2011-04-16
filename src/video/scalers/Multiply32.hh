// $Id$

#ifndef MULTIPLY32_HH
#define MULTIPLY32_HH

#include "openmsx.hh"

namespace openmsx {

template<typename Pixel> class PixelOperations;

/**
 * Helper class to perform 'pixel x scalar' calculations.
 * Also converts the result to 32bpp.
 */
template<typename Pixel> class Multiply32;

template<> class Multiply32<unsigned>
{
public:
	explicit Multiply32(const PixelOperations<unsigned>& format);

	inline void setFactor32(unsigned f)
	{
		factor = f;
	}

	inline unsigned mul32(unsigned p) const
	{
		return ((((p       & 0x00FF00FF) * factor) & 0xFF00FF00) >> 8)
		     | ((((p >> 8) & 0x00FF00FF) * factor) & 0xFF00FF00);
	}

	inline unsigned conv32(unsigned p) const
	{
		return p;
	}

private:
	unsigned factor;
};

template<> class Multiply32<word>
{
	// Note that 0 <= n < 32; on x86 this doesn't matter but on PPC it does.
	inline unsigned rotRight(unsigned a, unsigned n) const
	{
		return (a >> n) | (a << (32 - n));
	}

public:
	explicit Multiply32(const PixelOperations<word>& format);

	void setFactor32(unsigned factor);

	inline unsigned mul32(word p) const
	{
		return tab[p];
	}

	inline word conv32(unsigned p) const
	{
		return (rotRight(p, Rshift3) & Rmask1) |
		       (rotRight(p, Gshift3) & Gmask1) |
		       (rotRight(p, Bshift3) & Bmask1);
	}

private:
	unsigned tab[0x10000];
	unsigned factor;
	unsigned Rshift1, Gshift1, Bshift1;
	unsigned Rshift2, Gshift2, Bshift2;
	unsigned Rshift3, Gshift3, Bshift3;
	word     Rmask1,  Gmask1,  Bmask1;
	word     Rmask2,  Gmask2,  Bmask2;
};

} // namespace openmsx

#endif
