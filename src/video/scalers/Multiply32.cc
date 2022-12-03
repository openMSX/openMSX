#include "Multiply32.hh"
#include "PixelOperations.hh"
#include "enumerate.hh"
#include "narrow.hh"

namespace openmsx {

Multiply32<uint16_t>::Multiply32(const PixelOperations<uint16_t>& pixelOps)
	: Rshift1(narrow<int>(((2 + pixelOps.getRloss()) - pixelOps.getRshift()) & 31))
	, Gshift1(narrow<int>(((2 + pixelOps.getGloss()) - pixelOps.getGshift()) & 31))
	, Bshift1(narrow<int>(((2 + pixelOps.getBloss()) - pixelOps.getBshift()) & 31))

	, Rshift2(narrow<int>((2 * (2 + pixelOps.getRloss()) - pixelOps.getRshift() - 10) & 31))
	, Gshift2(narrow<int>((2 * (2 + pixelOps.getGloss()) - pixelOps.getGshift() - 10) & 31))
	, Bshift2(narrow<int>((2 * (2 + pixelOps.getBloss()) - pixelOps.getBshift() - 10) & 31))

	, Rshift3((Rshift1 +  0) & 31)
	, Gshift3((Gshift1 + 10) & 31)
	, Bshift3((Bshift1 + 20) & 31)

	, Rmask1(narrow_cast<uint16_t>(pixelOps.getRmask()))
	, Gmask1(narrow_cast<uint16_t>(pixelOps.getGmask()))
	, Bmask1(narrow_cast<uint16_t>(pixelOps.getBmask()))

	, Rmask2(narrow_cast<uint16_t>(
		((1 << (2 + pixelOps.getRloss())) - 1) <<
	                (10 + pixelOps.getRshift() - 2 * (2 + pixelOps.getRloss()))))
	, Gmask2(narrow_cast<uint16_t>(
		((1 << (2 + pixelOps.getGloss())) - 1) <<
	                (10 + pixelOps.getGshift() - 2 * (2 + pixelOps.getGloss()))))
	, Bmask2(narrow_cast<uint16_t>(
		((1 << (2 + pixelOps.getBloss())) - 1) <<
	                (10 + pixelOps.getBshift() - 2 * (2 + pixelOps.getBloss()))))
{
}

void Multiply32<uint16_t>::setFactor32(unsigned f)
{
	if (factor == f) return;
	factor = f;

	for (auto [p, t] : enumerate(tab)) {
		uint32_t r = std::rotl(uint16_t(p & Rmask1), Rshift1) |
			     std::rotl(uint16_t(p & Rmask2), Rshift2);
		uint32_t g = std::rotl(uint16_t(p & Gmask1), Gshift1) |
			     std::rotl(uint16_t(p & Gmask2), Gshift2);
		uint32_t b = std::rotl(uint16_t(p & Bmask1), Bshift1) |
			     std::rotl(uint16_t(p & Bmask2), Bshift2);
		t = (((r * factor) >> 8) <<  0) |
		    (((g * factor) >> 8) << 10) |
		    (((b * factor) >> 8) << 20);
	}
}

} // namespace openmsx
