#include "Multiply32.hh"
#include "PixelOperations.hh"
#include "enumerate.hh"
#include <cstring>

namespace openmsx {

// class Multiply32<uint32_t>

Multiply32<uint32_t>::Multiply32(const PixelOperations<uint32_t>& /*pixelOps*/)
{
	// nothing
}


// class Multiply32<uint16_t>

// gcc can optimize these rotate functions to just one instruction.
// We don't really need a rotate, but we do need a shift over a positive or
// negative (not known at compile time) distance, rotate handles this just fine.
// Note that 0 <= n < 32; on x86 this doesn't matter but on PPC it does.
static constexpr uint32_t rotLeft(uint32_t a, unsigned n)
{
	return (a << n) | (a >> (32 - n));
}

Multiply32<uint16_t>::Multiply32(const PixelOperations<uint16_t>& pixelOps)
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

void Multiply32<uint16_t>::setFactor32(unsigned f)
{
	if (factor == f) return;
	factor = f;

	for (auto [p, t] : enumerate(tab)) {
		uint32_t r = rotLeft((p & Rmask1), Rshift1) |
			     rotLeft((p & Rmask2), Rshift2);
		uint32_t g = rotLeft((p & Gmask1), Gshift1) |
			     rotLeft((p & Gmask2), Gshift2);
		uint32_t b = rotLeft((p & Bmask1), Bshift1) |
			     rotLeft((p & Bmask2), Bshift2);
		t = (((r * factor) >> 8) <<  0) |
		    (((g * factor) >> 8) << 10) |
		    (((b * factor) >> 8) << 20);
	}
}

} // namespace openmsx
