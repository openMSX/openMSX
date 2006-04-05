// $Id$

#ifndef MULTIPLY32_HH
#define MULTIPLY32_HH

#include "openmsx.hh"

struct SDL_PixelFormat;

namespace openmsx {

/**
 * Helper class to perform 'pixel x scalar' calculations.
 * Also converts the result to 32bpp.
 */
template<typename Pixel> class Multiply32;

template<> class Multiply32<unsigned>
{
public:
	Multiply32(const SDL_PixelFormat* format);

	inline void setFactor32(unsigned f)
	{
		factor = f;
	}

	inline unsigned mul32(unsigned p) const
	{
		return (((p & 0xFF00FF) * factor) & 0xFF00FF00) |
		       (((p & 0x00FF00) * factor) & 0x00FF0000);
	}

	inline unsigned conv32(unsigned p) const
	{
		return p >> 8;
	}

private:
	unsigned factor;
};

template<> class Multiply32<word>
{
	inline unsigned rotRight(unsigned a, unsigned n) const
	{
		return (a >> n) | (a << (32 - n));
	}

public:
	Multiply32(const SDL_PixelFormat* format);

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
