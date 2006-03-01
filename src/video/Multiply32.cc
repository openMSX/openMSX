// $Id$

#include "Multiply32.hh"

namespace openmsx {

// class Multiply32<unsigned>

Multiply32<unsigned>::Multiply32(const SDL_PixelFormat* /*format*/)
{
	// nothing
}


// class Multiply32<word>

// gcc can optimize these rotate functions to just one instruction.
// We don't really need a rotate, but we do need a shift over a positive or
// negative (not known at compile time) distance, rotate handles this just fine.
static inline unsigned rotLeft(unsigned a, unsigned n)
{
	return (a << n) | (a >> (32 - n));
}

Multiply32<word>::Multiply32(const SDL_PixelFormat* format)
{
	Rmask1 = format->Rmask;
	Gmask1 = format->Gmask;
	Bmask1 = format->Bmask;

	Rshift1 = (2 + format->Rloss) - format->Rshift;
	Gshift1 = (2 + format->Gloss) - format->Gshift;
	Bshift1 = (2 + format->Bloss) - format->Bshift;

	Rmask2 = ((1 << (2 + format->Rloss)) - 1) <<
	                (10 + format->Rshift - 2 * (2 + format->Rloss));
	Gmask2 = ((1 << (2 + format->Gloss)) - 1) <<
	                (10 + format->Gshift - 2 * (2 + format->Gloss));
	Bmask2 = ((1 << (2 + format->Bloss)) - 1) <<
	                (10 + format->Bshift - 2 * (2 + format->Bloss));

	Rshift2 = 2 * (2 + format->Rloss) - format->Rshift - 10;
	Gshift2 = 2 * (2 + format->Gloss) - format->Gshift - 10;
	Bshift2 = 2 * (2 + format->Bloss) - format->Bshift - 10;

	Rshift3 = Rshift1 + 0;
	Gshift3 = Gshift1 + 10;
	Bshift3 = Bshift1 + 20;

	factor = 0;
	memset(tab, 0, sizeof(tab));
}

void Multiply32<word>::setFactor32(unsigned f)
{
	if (factor == f) {
		return;
	}
	factor = f;

	for (unsigned p = 0; p < 0x10000; ++p) {
		unsigned r = rotLeft((p & Rmask1), Rshift1) |
			     rotLeft((p & Rmask2), Rshift2);
		unsigned g = rotLeft((p & Gmask1), Gshift1) |
			     rotLeft((p & Gmask2), Gshift2);
		unsigned b = rotLeft((p & Bmask1), Bshift1) |
			     rotLeft((p & Bmask2), Bshift2);
		tab[p] = (((r * factor) >> 8) <<  0) |
		         (((g * factor) >> 8) << 10) |
		         (((b * factor) >> 8) << 20);
	}
}


// Force template instantiation
template class Multiply32<word>;
template class Multiply32<unsigned>;

} // namespace openmsx
