#ifndef SDLPIXELFORMAT_HH
#define SDLPIXELFORMAT_HH

#include "PixelFormat.hh"
#include "build-info.hh"
#include <SDL.h>

namespace openmsx {

class SDLPixelFormat final : public PixelFormat
{
public:
	SDLPixelFormat() = default;
	SDLPixelFormat(unsigned bpp,
	               unsigned Rmask, unsigned Rshift, unsigned Rloss,
	               unsigned Gmask, unsigned Gshift, unsigned Gloss,
	               unsigned Bmask, unsigned Bshift, unsigned Bloss,
	               unsigned Amask, unsigned Ashift, unsigned Aloss)
	{
		format.palette = nullptr;
		format.BitsPerPixel = bpp;
		format.BytesPerPixel = (bpp + 7) / 8;
		format.Rmask = Rmask;
		format.Gmask = Gmask;
		format.Bmask = Bmask;
		format.Amask = Amask;
		format.Rshift = Rshift;
		format.Gshift = Gshift;
		format.Bshift = Bshift;
		format.Ashift = Ashift;
		format.Rloss = Rloss;
		format.Gloss = Gloss;
		format.Bloss = Bloss;
		format.Aloss = Aloss;
	}
	SDLPixelFormat(const SDL_PixelFormat& format_)
	{
		format = format_;
#if HAVE_32BPP
		// SDL sets an alpha channel only for GL modes. We want an alpha channel
		// for all 32bpp output surfaces, so we add one ourselves if necessary.
		if (format.BitsPerPixel == 32 && format.Amask == 0) {
			unsigned rgbMask = format.Rmask | format.Gmask | format.Bmask;
			format.Amask = ~rgbMask;
			format.Ashift = rgbMask & 24;
			format.Aloss = 0;
		}
#endif
	}

	unsigned map(unsigned r, unsigned g, unsigned b) const override {
		return SDL_MapRGB(&format, r, g, b);
	}

	unsigned getBpp() const override {
		return format.BitsPerPixel;
	}

	unsigned getBytesPerPixel() const override {
		return format.BytesPerPixel;
	}

	unsigned getRmask()  const override { return format.Rmask; }
	unsigned getGmask()  const override { return format.Gmask; }
	unsigned getBmask()  const override { return format.Bmask; }
	unsigned getAmask()  const override { return format.Amask; }
	unsigned getRshift() const override { return format.Rshift; }
	unsigned getGshift() const override { return format.Gshift; }
	unsigned getBshift() const override { return format.Bshift; }
	unsigned getAshift() const override { return format.Ashift; }
	unsigned getRloss()  const override { return format.Rloss; }
	unsigned getGloss()  const override { return format.Gloss; }
	unsigned getBloss()  const override { return format.Bloss; }
	unsigned getAloss()  const override { return format.Aloss; }

private:
	SDL_PixelFormat format;
};

} // namespace openmsx

#endif
