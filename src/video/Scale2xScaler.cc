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

// TODO: Copy-pasted from Scalers.cc:
template <class Pixel>
inline static Pixel* linePtr(SDL_Surface* surface, int y) {
	if (y < 0) y = 0;
	if (y >= surface->h) y = surface->h - 1;
	return (Pixel*)((byte*)surface->pixels + y * surface->pitch);
}

template <class Pixel>
void Scale2xScaler<Pixel>::scaleLine256(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY )
{
	Pixel* srcPrev = linePtr<Pixel>(src, srcY - 1);
	Pixel* srcCurr = linePtr<Pixel>(src, srcY);
	Pixel* srcNext = linePtr<Pixel>(src, srcY + 1);
	Pixel* dstUpper = linePtr<Pixel>(dst, dstY);
	Pixel* dstLower = linePtr<Pixel>(dst, dstY + 1);
	scaleLine256Half(dstUpper, srcPrev, srcCurr, srcNext, 320);
	scaleLine256Half(dstLower, srcNext, srcCurr, srcPrev, 320);
}

} // namespace openmsx
