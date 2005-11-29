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
	/* A B C
	 * D E F
	 * G H I
	 *
	 * E0 =  D == B && B != F && D != H ? D : E;
	 * E1 = (D == B && B != F && D != H && E != C) ||
	 *      (B == F && B != D && F != H && E != A) ? B : E;
	 * E2 = B == F && B != D && F != H ? F : E;
	 */

	// First pixel.
	Pixel top   = src0[0];
	Pixel mid   = src1[0];
	Pixel right = src1[1];
	Pixel bot   = src2[0];

	dst[0] = mid;
	dst[1] = (mid != right) && (top != bot) &&
	         (((top == mid ) && (mid != src0[1])) ||
	          ((top == right) && (mid != top)))
	       ? top : mid;
	dst[2] = (mid != right) && (top != bot) && (top == right)
	       ? top : mid;

	// Central pixels.
	for (unsigned x = 1; x < 319; ++x) {
		Pixel left = mid;
		mid   = right;
		right = src1[x + 1];
		top = src0[x];
		bot = src2[x];
		dst[3 * x + 0] = (left != right) && (top != bot) &&
		                 (top == left)
		               ? top : mid;
		dst[3 * x + 1] = (left != right) && (top != bot) &&
		                 (((top == left ) && (mid != src0[x + 1])) ||
		                  ((top == right) && (mid != src0[x - 1])))
		               ? top : mid;
		dst[3 * x + 2] = (left != right) && (top != bot) &&
		                 (top == right)
		               ? top : mid;
	}

	// Last pixel.
	Pixel left = mid;
	mid = right;
	top = src0[319];
	bot = src2[319];
	dst[957] = (left != mid) && (top != bot) && (top ==left)
	         ? top : mid;
	dst[958] = (left != mid) && (top != bot) &&
	           (((top == left) && (mid != top)) ||
	            ((top == mid ) && (mid != src0[318])))
	         ? top : mid;
	dst[959] = mid;
}

template <class Pixel>
void Scale3xScaler<Pixel>::scaleLine256Mid(Pixel* dst,
	const Pixel* src0, const Pixel* src1, const Pixel* src2)
{
	/*
	 * A B C
	 * D E F
	 * G H I
	 *
	 * E3 = (D == B && B != F && D != H && E != G) ||
	 *      (D == H && D != B && H != F && E != A) ? D : E;
	 * E4 = E
	 * E5 = (B == F && B != D && F != H && E != I) ||
	 *      (H == F && D != H && B != F && E != C) ? F : E;
	 */

	// First pixel.
	Pixel mid   = src1[0];
	Pixel right = src1[1];
	Pixel top = src0[0];
	Pixel bot = src2[0];
	dst[0] = mid;
	dst[1] = mid;
	dst[2] = (mid != right) && (top != bot) &&
	         (((right == top) && (mid != src2[1])) ||
	          ((right == bot) && (mid != src0[1])))
	       ? right : mid;

	// Central pixels.
	for (unsigned x = 1; x < 319; ++x) {
		Pixel left = mid;
		mid   = right;
		right = src1[x + 1];
		top = src0[x];
		bot = src2[x];
		dst[3 * x + 0] = (left != right) && (top != bot) &&
		                 (((left == top) && (mid != src2[x - 1])) ||
		                  ((left == bot) && (mid != src0[x - 1])))
		               ? left : mid;
		dst[3 * x + 1] = mid;
		dst[3 * x + 2] = (left != right) && (top != bot) &&
		                 (((right == top) && (mid != src2[x + 1])) ||
		                  ((right == bot) && (mid != src0[x + 1])))
		               ? right : mid;
	}

	// Last pixel.
	Pixel left = mid;
	mid   = right;
	top = src0[319];
	bot = src2[319];
	dst[957] = (left != mid) && (top != bot) &&
	           (((left == top) && (mid != src2[318])) ||
	            ((left == bot) && (mid != src0[318])))
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
void Scale3xScaler<Pixel>::scale1x1to3x3(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	Pixel* const dummy = 0;
	int srcY = srcStartY;
	const Pixel* srcPrev = src.getLinePtr(srcY - 1, srcWidth, dummy);
	const Pixel* srcCurr = src.getLinePtr(srcY + 0, srcWidth, dummy);
	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 3) {
		const Pixel* srcNext = src.getLinePtr(srcY + 1, srcWidth, dummy);
		Pixel* dstUpper = dst.getLinePtr(dstY + 0, dummy);
		scaleLine256Half(dstUpper, srcPrev, srcCurr, srcNext);
		if ((dstY + 1) == dstEndY) break;
		Pixel* dstMiddle = dst.getLinePtr(dstY + 1, dummy);
		scaleLine256Mid(dstMiddle, srcPrev, srcCurr, srcNext);
		if ((dstY + 2) == dstEndY) break;
		Pixel* dstLower = dst.getLinePtr(dstY + 2, dummy);
		scaleLine256Half(dstLower, srcNext, srcCurr, srcPrev);
		srcPrev = srcCurr;
		srcCurr = srcNext;
	}
}


// Force template instantiation.
template class Scale3xScaler<word>;
template class Scale3xScaler<unsigned int>;

} // namespace openmsx
