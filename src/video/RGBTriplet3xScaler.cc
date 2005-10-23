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

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale256(FrameSource& src, SDL_Surface* dst,
                                 unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 3 * startY + (lower ? 1 : 0);
	unsigned y2 = 3 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 3, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
		for (int p = 0; p < 320; ++p) {
			Pixel pix = srcLine[p];
			dstLine0[3 * p + 0] = (pix & 0x00FF0000) | ((pix >> 1) & 0x007F7F);
			dstLine0[3 * p + 1] = (pix & 0x0000FF00) | ((pix >> 1) & 0x7F007F);
			dstLine0[3 * p + 2] = (pix & 0x000000FF) | ((pix >> 1) & 0x7F7F00);
		}
		//scale_1on3(srcLine, dstLine0, 320);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine0, dstLine1, 960);
		if (y == (720 - 2)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		// emulate a scanline for the 3rd row
		for (int p = 0; p < 320; ++p) {
			Pixel pix = (srcLine[p]>>1 & 0x007F7F7F);
			dstLine2[3 * p + 0] = pix;
			dstLine2[3 * p + 1] = pix;
			dstLine2[3 * p + 2] = pix;
		}
//		copyLine(dstLine0, dstLine2, 960);
	}
}

// Force template instantiation.
template class RGBTriplet3xScaler<word>;
template class RGBTriplet3xScaler<unsigned>;

} // namespace openmsx
