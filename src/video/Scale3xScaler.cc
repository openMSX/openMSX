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

#include "Scale3xScaler.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "HostCPU.hh"
#include "openmsx.hh"
#include <algorithm>

using std::min;

namespace openmsx {

template <class Pixel>
Scale3xScaler<Pixel>::Scale3xScaler(SDL_PixelFormat* format)
	: Scaler3<Pixel>(format)
{
}

template <class Pixel>
void Scale3xScaler<Pixel>::scaleLine256Half(Pixel* dst,
	const Pixel* src0, const Pixel* src1, const Pixel* src2)
{
	// First pixel.
	Pixel mid   = src1[0];
	Pixel right = src1[1];
	Pixel top = src0[0];
	Pixel bot = src2[0];
	dst[0] = mid;
	dst[1] =
		   (top == mid && top != right && bot != mid && mid != src0[1])
		|| (top == right && top != mid && bot != right && mid != top)
		? top : mid;
	dst[2] =
		top == right && top != mid && bot != right ? right : mid;

	// Central pixels.
	for (unsigned x = 1; x < 319; ++x) {
		Pixel left = mid;
		mid   = right;
		right = src1[x + 1];
		top = src0[x];
		bot = src2[x];
		dst[3 * x + 0] =
			top == left && top != right && bot != left ? left : mid;
		dst[3 * x + 1] =
			   (top == left && top != right && bot != left && mid != src0[x+1])
			|| (top == right && top != left && bot != right && mid != src0[x-1])
			? top : mid;
		dst[3 * x + 2] =
			top == right && top != left && bot != right ? right : mid;
/*
E0 = D == B && B != F && D != H ? D : E;
E1 = (D == B && B != F && D != H && E != C) || (B == F && B != D && F != H && E != A) ? B : E;
E2 = B == F && B != D && F != H ? F : E;

A B C
D E F
G H I
*/
	}

	// Last pixel.
	Pixel left = mid;
	mid   = right;
	top = src0[319];
	bot = src2[319];
	dst[957] =
		top == left && top != mid && bot != left ? left : mid;
	dst[958] =
		   (top == left && top != right && bot != left && mid != top)
		|| (top == right && top != left && bot != right && mid != src0[318])
		? top : mid;
	dst[959] = mid;
}

template <class Pixel>
void Scale3xScaler<Pixel>::scaleLine256Mid(Pixel* dst,
	const Pixel* src0, const Pixel* src1, const Pixel* src2)
{
	// First pixel.
	Pixel mid   = src1[0];
	Pixel right = src1[1];
	Pixel top = src0[0];
	Pixel bot = src2[0];
	dst[0] = mid;
	dst[1] = mid;
	dst[2] =
		   (top == right && top != mid && bot != right && mid != src2[1])
		|| (bot == right && bot != mid && top != right && mid != src0[1])
		? right : mid;

	// Central pixels.
	for (unsigned x = 1; x < 319; ++x) {
		Pixel left = mid;
		mid   = right;
		right = src1[x + 1];
		top = src0[x];
		bot = src2[x];
		//dst[3 * x + 0] = (left  == top && right != top && bot != top) ? top : mid;
		dst[3 * x + 0] =
			   (top == left && top != right && bot != left && mid != src2[x-1])
			|| (bot == left && bot != right && top != left && mid != src0[x-1])
			? left : mid;
		dst[3 * x + 1] = mid;
		//dst[3 * x + 2] = (right == top && left  != top && bot != top) ? top : mid;
		dst[3 * x + 2] =
			   (top == right && top != left && bot != right && mid != src2[x+1])
			|| (bot == right && bot != left && top != right && mid != src0[x+1])
			? right : mid;
/*
A B C
D E F
G H I
E3 = (D == B && B != F && D != H && E != G) || (D == H && D != B && H != F && E != A) ? D : E;
E4 = E
E5 = (B == F && B != D && F != H && E != I) || (H == F && D != H && B != F && E != C) ? F : E;
*/
	}

	// Last pixel.
	Pixel left = mid;
	mid   = right;
	top = src0[319];
	bot = src2[319];
	dst[957] = //(mid == src0[319] && src2[319] != src0[319]) ? src0[319] : right;
		   (top == left && top != mid && bot != left && mid != src2[318])
		|| (bot == left && bot != mid && top != left && mid != src0[318])
		? left : mid;
	dst[958] = mid;
	dst[959] = mid;

}

/*
template <class Pixel>
void Scale3xScaler<Pixel>::scaleLine512Half(Pixel* dst,
	const Pixel* src0, const Pixel* src1, const Pixel* src2)
{
}
*/
template <class Pixel>
void Scale3xScaler<Pixel>::scale1x1to3x3(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	unsigned dstY = dstStartY;
	unsigned prevY = srcStartY;
	while (dstY < dstEndY) {
		Pixel* const dummy = 0;
		const Pixel* srcPrev = src.getLinePtr(prevY,  dummy);
		const Pixel* srcCurr = src.getLinePtr(srcStartY, dummy);
		const Pixel* srcNext = src.getLinePtr(min(srcStartY + 1, srcEndY - 1), dummy);
		Pixel* dstUpper = dst.getLinePtr(dstY++, dummy);
		scaleLine256Half(dstUpper, srcPrev, srcCurr, srcNext);
		if (dstY == dstEndY) break;
		Pixel* dstMiddle = dst.getLinePtr(dstY++, dummy);
		scaleLine256Mid(dstMiddle, srcPrev, srcCurr, srcNext);
		if (dstY == dstEndY) break;
		Pixel* dstLower = dst.getLinePtr(dstY++, dummy);
		scaleLine256Half(dstLower, srcNext, srcCurr, srcPrev);
		prevY = srcStartY;
		++srcStartY;
	}
}

/*
template <class Pixel>
void Scale3xScaler<Pixel>::scale2x1to3x3(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
}
*/


// Force template instantiation.
template class Scale3xScaler<word>;
template class Scale3xScaler<unsigned int>;

} // namespace openmsx
