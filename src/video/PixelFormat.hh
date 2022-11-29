#ifndef PIXELFORMAT_HH
#define PIXELFORMAT_HH

#include "narrow.hh"
#include <cstdint>

namespace openmsx {

class PixelFormat
{
public:
	PixelFormat() = default;
	PixelFormat(unsigned bpp_,
	            uint32_t Rmask_, uint8_t Rshift_, uint8_t Rloss_,
	            uint32_t Gmask_, uint8_t Gshift_, uint8_t Gloss_,
	            uint32_t Bmask_, uint8_t Bshift_, uint8_t Bloss_,
	            uint32_t Amask_, uint8_t Ashift_, uint8_t Aloss_)
		: Rmask (Rmask_),  Gmask (Gmask_),  Bmask (Bmask_),  Amask (Amask_)
		, Rshift(Rshift_), Gshift(Gshift_), Bshift(Bshift_), Ashift(Ashift_)
		, Rloss (Rloss_),  Gloss (Gloss_),  Bloss (Bloss_),  Aloss (Aloss_)
		, bpp(narrow<uint8_t>(bpp_))
		, bytesPerPixel(narrow<uint8_t>((bpp + 7) / 8)) {}

	[[nodiscard]] unsigned getBpp()           const { return bpp; }
	[[nodiscard]] unsigned getBytesPerPixel() const { return bytesPerPixel; }

	[[nodiscard]] unsigned getRmask() const  { return Rmask; }
	[[nodiscard]] unsigned getGmask() const  { return Gmask; }
	[[nodiscard]] unsigned getBmask() const  { return Bmask; }
	[[nodiscard]] unsigned getAmask() const  { return Amask; }

	[[nodiscard]] unsigned getRshift() const { return Rshift; }
	[[nodiscard]] unsigned getGshift() const { return Gshift; }
	[[nodiscard]] unsigned getBshift() const { return Bshift; }
	[[nodiscard]] unsigned getAshift() const { return Ashift; }

	[[nodiscard]] unsigned getRloss() const  { return Rloss; }
	[[nodiscard]] unsigned getGloss() const  { return Gloss; }
	[[nodiscard]] unsigned getBloss() const  { return Bloss; }
	[[nodiscard]] unsigned getAloss() const  { return Aloss; }

	[[nodiscard]] unsigned map(unsigned r, unsigned g, unsigned b) const
	{
		return ((r >> Rloss) << Rshift) |
		       ((g >> Gloss) << Gshift) |
		       ((b >> Bloss) << Bshift) |
		       Amask;
	}

private:
	uint32_t Rmask,  Gmask,  Bmask,  Amask;
	uint8_t  Rshift, Gshift, Bshift, Ashift;
	uint8_t  Rloss,  Gloss,  Bloss,  Aloss;
	uint8_t bpp, bytesPerPixel;
};

} // namespace openmsx

#endif
