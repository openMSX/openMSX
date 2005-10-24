// $Id$

#include "RGBTriplet3xScaler.hh"
#include "FrameSource.hh"
#include <algorithm>
#include <cassert>

using std::min;

namespace openmsx {

template <class Pixel>
RGBTriplet3xScaler<Pixel>::RGBTriplet3xScaler(SDL_PixelFormat* format)
	: Scaler3<Pixel>(format)
{
}

static void calcSpil(unsigned x, unsigned& r, unsigned& s)
{
	r = 3 * x;
	s = 0;
	if (r > 255) {
		s = (r - 255) / 2;
		r = 255;
	}
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale256(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned y = dstStartY; y < dstEndY; y += 3, ++srcStartY) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);

		unsigned r, g, b, rs, gs, bs;
		calcSpil((srcLine[0] >> 16) & 0xFF, r, rs);
		calcSpil((srcLine[0] >>  8) & 0xFF, g, gs);
		calcSpil((srcLine[0] >>  0) & 0xFF, b, bs);
		dstLine0[0] = (r  << 16) + (gs << 8);
		dstLine0[1] = (rs << 16) + (g  << 8) + (bs << 0);
		calcSpil((srcLine[1] >> 16) & 0xFF, r, rs);
		dstLine0[2] = (rs << 16) + (gs << 8) + (b  << 0);
		for (int i = 1; i < 319; ++i) {
			calcSpil((srcLine[i + 0] >>  8) & 0xFF, g, gs);
			dstLine0[3 * i + 0] = (r  << 16) + (gs << 8) + (bs << 0);
			calcSpil((srcLine[i + 0] >>  0) & 0xFF, b, bs);
			dstLine0[3 * i + 1] = (rs << 16) + (g  << 8) + (bs << 0);
			calcSpil((srcLine[i + 1] >> 16) & 0xFF, r, rs);
			dstLine0[3 * i + 2] = (rs << 16) + (gs << 8) + (b  << 0);
		}
		calcSpil((srcLine[319] >>  8) & 0xFF, g, gs);
		dstLine0[957] = (r  << 16) + (gs << 8) + (bs << 0);
		calcSpil((srcLine[319] >>  0) & 0xFF, b, bs);
		dstLine0[958] = (rs << 16) + (g  << 8) + (bs << 0);
		dstLine0[959] =              (gs << 8) + (b  << 0);
		
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine0, dstLine1, 960);
		if (y + 2 == dstEndY) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		// emulate a scanline for the 3rd row
		for (int i = 0; i < 960; ++i) {
			dstLine2[i] = (dstLine0[i] >> 1) & 0x007F7F7F;
		}
	}
}

// Force template instantiation.
template class RGBTriplet3xScaler<word>;
template class RGBTriplet3xScaler<unsigned>;

} // namespace openmsx
