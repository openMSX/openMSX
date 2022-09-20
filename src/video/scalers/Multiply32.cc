#include "Multiply32.hh"
#include "PixelOperations.hh"
#include "enumerate.hh"
#include "ranges.hh"

namespace openmsx {

// class Multiply32<uint32_t>

Multiply32<uint32_t>::Multiply32(const PixelOperations<uint32_t>& /*pixelOps*/)
{
	// nothing
}


// class Multiply32<uint16_t>

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
	ranges::fill(tab, 0);
}

void Multiply32<uint16_t>::setFactor32(unsigned f)
{
	if (factor == f) return;
	factor = f;

	for (auto [p, t] : enumerate(tab)) {
		uint32_t r = std::rotl((p & Rmask1), Rshift1) |
			     std::rotl((p & Rmask2), Rshift2);
		uint32_t g = std::rotl((p & Gmask1), Gshift1) |
			     std::rotl((p & Gmask2), Gshift2);
		uint32_t b = std::rotl((p & Bmask1), Bshift1) |
			     std::rotl((p & Bmask2), Bshift2);
		t = (((r * factor) >> 8) <<  0) |
		    (((g * factor) >> 8) << 10) |
		    (((b * factor) >> 8) << 20);
	}
}

} // namespace openmsx
