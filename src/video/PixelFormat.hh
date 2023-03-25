#ifndef PIXELFORMAT_HH
#define PIXELFORMAT_HH

#include <cassert>
#include <cstdint>

namespace openmsx {

class PixelFormat
{
public:
	PixelFormat() = default;
	PixelFormat(uint32_t Rmask_, uint8_t Rshift_, uint8_t Rloss,
	            uint32_t Gmask_, uint8_t Gshift_, uint8_t Gloss,
	            uint32_t Bmask_, uint8_t Bshift_, uint8_t Bloss,
	            uint32_t Amask_, uint8_t Ashift_, uint8_t Aloss)
		: Rmask (Rmask_),  Gmask (Gmask_),  Bmask (Bmask_),  Amask (Amask_)
		, Rshift(Rshift_), Gshift(Gshift_), Bshift(Bshift_), Ashift(Ashift_)
	{
		assert(Rloss == 0);
		assert(Gloss == 0);
		assert(Bloss == 0);
		assert(Aloss == 0);
	}

	[[nodiscard]] unsigned getRmask() const  { return Rmask; }
	[[nodiscard]] unsigned getGmask() const  { return Gmask; }
	[[nodiscard]] unsigned getBmask() const  { return Bmask; }
	[[nodiscard]] unsigned getAmask() const  { return Amask; }

	[[nodiscard]] unsigned getRshift() const { return Rshift; }
	[[nodiscard]] unsigned getGshift() const { return Gshift; }
	[[nodiscard]] unsigned getBshift() const { return Bshift; }
	[[nodiscard]] unsigned getAshift() const { return Ashift; }

	[[nodiscard]] unsigned map(unsigned r, unsigned g, unsigned b) const
	{
		return (r << Rshift) |
		       (g << Gshift) |
		       (b << Bshift) |
		       Amask;
	}

private:
	uint32_t Rmask,  Gmask,  Bmask,  Amask;
	uint8_t  Rshift, Gshift, Bshift, Ashift;
};

} // namespace openmsx

#endif
