// $Id$

#include "Multiply32.hh"
#include "build-info.hh"
#include <cstring>
#include <SDL.h> // TODO

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
// Note that 0 <= n < 32; on x86 this doesn't matter but on PPC it does.
static inline unsigned rotLeft(unsigned a, unsigned n)
{
	return (a << n) | (a >> (32 - n));
}

Multiply32<word>::Multiply32(const SDL_PixelFormat* format)
{
	Rmask1 = format->Rmask;
	Gmask1 = format->Gmask;
	Bmask1 = format->Bmask;

	Rshift1 = ((2 + format->Rloss) - format->Rshift) & 31;
	Gshift1 = ((2 + format->Gloss) - format->Gshift) & 31;
	Bshift1 = ((2 + format->Bloss) - format->Bshift) & 31;

	Rmask2 = ((1 << (2 + format->Rloss)) - 1) <<
	                (10 + format->Rshift - 2 * (2 + format->Rloss));
	Gmask2 = ((1 << (2 + format->Gloss)) - 1) <<
	                (10 + format->Gshift - 2 * (2 + format->Gloss));
	Bmask2 = ((1 << (2 + format->Bloss)) - 1) <<
	                (10 + format->Bshift - 2 * (2 + format->Bloss));

	Rshift2 = (2 * (2 + format->Rloss) - format->Rshift - 10) & 31;
	Gshift2 = (2 * (2 + format->Gloss) - format->Gshift - 10) & 31;
	Bshift2 = (2 * (2 + format->Bloss) - format->Bshift - 10) & 31;

	Rshift3 = (Rshift1 +  0) & 31;
	Gshift3 = (Gshift1 + 10) & 31;
	Bshift3 = (Bshift1 + 20) & 31;

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
#if HAVE_16BPP
template class Multiply32<word>;
#endif
#if HAVE_32BPP
template class Multiply32<unsigned>;
#endif

} // namespace openmsx
