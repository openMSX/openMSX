// $Id$

/*
Original code: Copyright (C) 2001-2003 Andrea Mazzoleni
openMSX adaptation by Maarten ter Huurne

This file is based on code from the Scale2x project.
This modified version is licensed under GPL; the original code is dual-licensed
under GPL and under a custom license.

Visit the Scale2x site for info:
  http://scale2x.sourceforge.net/
*/

#include "Scale2xScaler.hh"
#include "openmsx.hh"
#include <cassert>
#include <algorithm>

using std::max;
using std::min;


namespace openmsx {

// Force template instantiation.
template class Scale2xScaler<byte>;
template class Scale2xScaler<word>;
template class Scale2xScaler<unsigned int>;

template <class Pixel>
Scale2xScaler<Pixel>::Scale2xScaler() {
}

template <class Pixel>
void Scale2xScaler<Pixel>::scaleLine256Half(
	Pixel* dst,
	const Pixel* src0, const Pixel* src1, const Pixel* src2,
	int count
) {
	count -= 2;
	assert(count >= 0);

	// First pixel.
	Pixel left = src1[0];
	dst[0] = left;
	dst[1] = (src1[1] == src0[0] && src2[0] != src0[0]) ? src0[0] : left;
	src0++;
	src1++;
	src2++;
	dst += 2;

	// Central pixels.
	for (; count; count--) {
		Pixel top = src0[0];
		Pixel mid = src1[0];
		dst[0] =
			(left == top && src2[0] != top && src1[1] != top) ? top : mid;
		dst[1] =
			(src1[1] == top && src2[0] != top && left != top) ? top : mid;
		src0++;
		src1++;
		src2++;
		dst += 2;
		left = mid;
	}

	// Last pixel.
	dst[0] = (left == src0[0] && src2[0] != src0[0]) ? src0[0] : src1[0];
	dst[1] = src1[0];
}

// TODO: Copy-pasted from SimpleScaler.cc:
template <class Pixel>
inline static Pixel* linePtr(SDL_Surface* surface, int y) {
	assert(0 <= y && y < surface->h);
	return (Pixel*)((byte*)surface->pixels + y * surface->pitch);
}

template <class Pixel>
void Scale2xScaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	int prevY = srcY;
	while (srcY < endSrcY) {
		Pixel* srcPrev = linePtr<Pixel>(src, prevY);
		Pixel* srcCurr = linePtr<Pixel>(src, srcY);
		Pixel* srcNext = linePtr<Pixel>(src, min(srcY + 1, endSrcY - 1));
		Pixel* dstUpper = linePtr<Pixel>(dst, dstY++);
		scaleLine256Half(dstUpper, srcPrev, srcCurr, srcNext, 320);
		if (dstY == dst->h) break;
		Pixel* dstLower = linePtr<Pixel>(dst, dstY++);
		scaleLine256Half(dstLower, srcNext, srcCurr, srcPrev, 320);
		prevY = srcY;
		srcY++;
	}
}

} // namespace openmsx
