// $Id$

#include "SimpleScaler.hh"
#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "openmsx.hh"
#include <cassert>


namespace openmsx {

// Force template instantiation.
template class SimpleScaler<byte>;
template class SimpleScaler<word>;
template class SimpleScaler<unsigned int>;

template <class Pixel>
SimpleScaler<Pixel>::SimpleScaler()
	: scanlineAlphaSetting(RenderSettings::instance().getScanlineAlpha())
{
	update(scanlineAlphaSetting);
	scanlineAlphaSetting->addListener(this);
}

template <class Pixel>
SimpleScaler<Pixel>::~SimpleScaler()
{
	scanlineAlphaSetting->removeListener(this);
}

template <class Pixel>
void SimpleScaler<Pixel>::update(const SettingLeafNode* setting)
{
	assert(setting == scanlineAlphaSetting);
	scanlineAlpha = (scanlineAlphaSetting->getValue() * 255) / 100;
}

// Upper 8 bits do not contain colours; use them as work area.
class HiFreeDarken {
public:
	static inline bool check(Uint32 rMask, Uint32 gMask, Uint32 bMask) {
		return ((rMask | gMask | bMask) & 0xFF000000) == 0;
	}
	static inline Uint32 darken(
		int darkenFactor,
		Uint32 rMask, Uint32 gMask, Uint32 bMask,
		Uint32 colour
		)
	{
		unsigned r = (((colour & rMask) * darkenFactor) >> 8) & rMask;
		unsigned g = (((colour & gMask) * darkenFactor) >> 8) & gMask;
		unsigned b = (((colour & bMask) * darkenFactor) >> 8) & bMask;
		return r | g | b;
	}
};

// Lower 8 bits do not contain colours; use them as work area.
class LoFreeDarken {
public:
	static inline bool check(Uint32 rMask, Uint32 gMask, Uint32 bMask) {
		return ((rMask | gMask | bMask) & 0x000000FF) == 0;
	}
	static inline Uint32 darken(
		int darkenFactor,
		Uint32 rMask, Uint32 gMask, Uint32 bMask,
		Uint32 colour
		)
	{
		unsigned r = (((colour & rMask) >> 8) * darkenFactor) & rMask;
		unsigned g = (((colour & gMask) >> 8) * darkenFactor) & gMask;
		unsigned b = (((colour & bMask) >> 8) * darkenFactor) & bMask;
		return r | g | b;
	}
};

// Uncommon pixel format; fall back to slightly slower routine.
class UniversalDarken {
public:
	static inline bool check(Uint32 rMask, Uint32 gMask, Uint32 bMask) {
		return true;
	}
	static inline Uint32 darken(
		int darkenFactor,
		Uint32 rMask, Uint32 gMask, Uint32 bMask,
		Uint32 colour
	) {
		Uint32 r =
			rMask & 0xFF
			? (((colour & rMask) * darkenFactor) >> 8) & rMask
			: (((colour & rMask) >> 8) * darkenFactor) & rMask;
		Uint32 g =
			gMask & 0xFF
			? (((colour & gMask) * darkenFactor) >> 8) & gMask
			: (((colour & gMask) >> 8) * darkenFactor) & gMask;
		Uint32 b =
			bMask & 0xFF
			? (((colour & bMask) * darkenFactor) >> 8) & bMask
			: (((colour & bMask) >> 8) * darkenFactor) & bMask;
		return r | g | b;
	}
};

void SimpleScaler<byte>::scaleBlank(
	byte colour,
	SDL_Surface* dst, int dstY, int endDstY
) {
	// Note: Scanline algorithm does not work on palette modes (8bpp).
	//       (100% should be possible though).
	Scaler<byte>::scaleBlank(colour, dst, dstY, endDstY);
}

template <class Pixel>
void SimpleScaler<Pixel>::scaleBlank(
	Pixel colour,
	SDL_Surface* dst, int dstY, int endDstY
) {
	if (colour == 0) {
		// No need to draw scanlines if border is black.
		// This is a special case that occurs very often.
		//cerr << "scale black " << dstY << " to " << endDstY << "\n";
		Scaler<Pixel>::scaleBlank(colour, dst, dstY, endDstY);
	} else {
		//cerr << "scale non-black " << dstY << " to " << endDstY << "\n";
		// Note: SDL_FillRect is generally not allowed on locked surfaces.
		//       However, we're using a software surface, which doesn't
		//       have locking.
		assert(!SDL_MUSTLOCK(dst));

		SDL_PixelFormat* format = dst->format;
		Uint32 rMask = format->Rmask;
		Uint32 gMask = format->Gmask;
		Uint32 bMask = format->Bmask;
		int darkenFactor = 256 - scanlineAlpha;
		Pixel scanlineColour = UniversalDarken::darken(
			darkenFactor, rMask, gMask, bMask, colour );

		SDL_Rect rect;
		rect.x = 0;
		rect.w = dst->w;
		rect.h = 1;
		for (int y = dstY; y < endDstY; y += 2) {
			rect.y = y;
			// Note: return code ignored.
			SDL_FillRect(dst, &rect, colour);
			if (y + 1 == endDstY) break;
			rect.y = y + 1;
			// Note: return code ignored.
			SDL_FillRect(dst, &rect, scanlineColour);
		}
	}
}

template <class Pixel>
template <class Darken>
void SimpleScaler<Pixel>::scanLineScale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY,
	int darkenFactor,
	Uint32 rMask, Uint32 gMask, Uint32 bMask
) {
	const int WIDTH = dst->w / 2;
	assert(dst->w == WIDTH * 2);
	while (srcY < endSrcY) {
		const Pixel* srcLine = Scaler<Pixel>::linePtr(src, srcY++);
		Pixel* dstUpper = Scaler<Pixel>::linePtr(dst, dstY++);
		Pixel* dstLower =
			dstY == dst->h ? dstUpper : Scaler<Pixel>::linePtr(dst, dstY++);
		for (int x = 0; x < WIDTH; x++) {
			Pixel p = srcLine[x];
			dstUpper[x * 2] = dstUpper[x * 2 + 1] = p;
			dstLower[x * 2] = dstLower[x * 2 + 1] =
				Darken::darken(darkenFactor, rMask, gMask, bMask, p);
		}
	}
}

void SimpleScaler<byte>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	// Note: Scanline algorithm does not work on palette modes (8bpp).
	//       (100% should be possible though).
	Scaler<byte>::scale256(src, srcY, endSrcY, dst, dstY);
}

template <class Pixel>
void SimpleScaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	if (scanlineAlpha == 0) {
		Scaler<Pixel>::scale256(src, srcY, endSrcY, dst, dstY);
		return;
	}

	SDL_PixelFormat* format = dst->format;
	Uint32 rMask = format->Rmask;
	Uint32 gMask = format->Gmask;
	Uint32 bMask = format->Bmask;
	int darkenFactor = 256 - scanlineAlpha;
	// Note: For 16bpp, HiFreeDarken is always OK.
	//       To avoid unnecessary template expansions,
	//       test pixel size explicitly.
	if (sizeof(Pixel) == 2 || HiFreeDarken::check(rMask, gMask, bMask)) {
		scanLineScale256<HiFreeDarken>(
			src, srcY, endSrcY, dst, dstY,
			darkenFactor, rMask, gMask, bMask
			);
	} else if (LoFreeDarken::check(rMask, gMask, bMask)) {
		scanLineScale256<LoFreeDarken>(
			src, srcY, endSrcY, dst, dstY,
			darkenFactor, rMask, gMask, bMask
			);
	} else {
		scanLineScale256<UniversalDarken>(
			src, srcY, endSrcY, dst, dstY,
			darkenFactor, rMask, gMask, bMask
			);
	}
}

template <class Pixel>
template <class Darken>
void SimpleScaler<Pixel>::scanLineScale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY,
	int darkenFactor,
	Uint32 rMask, Uint32 gMask, Uint32 bMask
) {
	const unsigned WIDTH = dst->w;
	while (srcY < endSrcY) {
		Scaler<Pixel>::copyLine(src, srcY, dst, dstY++);
		if (dstY == dst->h) break;
		const Pixel* srcLine = Scaler<Pixel>::linePtr(src, srcY);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, dstY++);
		for (unsigned x = 0; x < WIDTH; x++) {
			dstLine[x] = Darken::darken(
				darkenFactor, rMask, gMask, bMask, srcLine[x] );
		}
		srcY++;
	}
}

void SimpleScaler<byte>::scale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	// Note: Scanline algorithm does not work on palette modes (8bpp).
	//       (100% should be possible though).
	Scaler<byte>::scale512(src, srcY, endSrcY, dst, dstY);
}

template <class Pixel>
void SimpleScaler<Pixel>::scale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	if (scanlineAlpha == 0) {
		Scaler<Pixel>::scale512(src, srcY, endSrcY, dst, dstY);
		return;
	}

	SDL_PixelFormat* format = dst->format;
	Uint32 rMask = format->Rmask;
	Uint32 gMask = format->Gmask;
	Uint32 bMask = format->Bmask;
	int darkenFactor = 256 - scanlineAlpha;
	// Note: For 16bpp, HiFreeDarken is always OK.
	//       To avoid unnecessary template expansions,
	//       test pixel size explicitly.
	if (sizeof(Pixel) == 2 || HiFreeDarken::check(rMask, gMask, bMask)) {
		scanLineScale512<HiFreeDarken>(
			src, srcY, endSrcY, dst, dstY,
			darkenFactor, rMask, gMask, bMask
			);
	} else if (LoFreeDarken::check(rMask, gMask, bMask)) {
		scanLineScale512<LoFreeDarken>(
			src, srcY, endSrcY, dst, dstY,
			darkenFactor, rMask, gMask, bMask
			);
	} else {
		scanLineScale512<UniversalDarken>(
			src, srcY, endSrcY, dst, dstY,
			darkenFactor, rMask, gMask, bMask
			);
	}
}

} // namespace openmsx

