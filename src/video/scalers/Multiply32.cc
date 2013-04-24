#include "Multiply32.hh"
#include "PixelOperations.hh"
#include "build-info.hh"
#include <cstring>

namespace openmsx {

// class Multiply32<unsigned>

Multiply32<unsigned>::Multiply32(const PixelOperations<unsigned>& /*pixelOps*/)
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

Multiply32<word>::Multiply32(const PixelOperations<word>& pixelOps)
{
	Rmask1 = pixelOps.getRmask();
	Gmask1 = pixelOps.getGmask();
	Bmask1 = pixelOps.getBmask();

	Rshift1 = ((2 + pixelOps.getRloss()) - pixelOps.getRshift()) & 31;
	Gshift1 = ((2 + pixelOps.getGloss()) - pixelOps.getGshift()) & 31;
	Bshift1 = ((2 + pixelOps.getBloss()) - pixelOps.getBshift()) & 31;

	Rmask2 = ((1 << (2 + pixelOps.getRloss())) - 1) <<
	                (10 + pixelOps.getRshift() - 2 * (2 + pixelOps.getRloss()));
	Gmask2 = ((1 << (2 + pixelOps.getGloss())) - 1) <<
	                (10 + pixelOps.getGshift() - 2 * (2 + pixelOps.getGloss()));
	Bmask2 = ((1 << (2 + pixelOps.getBloss())) - 1) <<
	                (10 + pixelOps.getBshift() - 2 * (2 + pixelOps.getBloss()));

	Rshift2 = (2 * (2 + pixelOps.getRloss()) - pixelOps.getRshift() - 10) & 31;
	Gshift2 = (2 * (2 + pixelOps.getGloss()) - pixelOps.getGshift() - 10) & 31;
	Bshift2 = (2 * (2 + pixelOps.getBloss()) - pixelOps.getBshift() - 10) & 31;

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
#ifndef _MSC_VER
#if HAVE_16BPP
template class Multiply32<word>;
#endif
#if HAVE_32BPP
template class Multiply32<unsigned>;
#endif
#else
// In VC++ we hit this problem again (see also V9990BitmapConverter)
//    http://msdn.microsoft.com/en-us/library/9045w50z.aspx
// But luckily we don't get link errors without this in VC++.
#endif

} // namespace openmsx
