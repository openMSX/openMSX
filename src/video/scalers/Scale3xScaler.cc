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
#include "ScalerOutput.hh"
#include "vla.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel>
Scale3xScaler<Pixel>::Scale3xScaler(const PixelOperations<Pixel>& pixelOps_)
	: Scaler3<Pixel>(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale3xScaler<Pixel>::scaleLine1on3Half(
	std::span<Pixel> dst,
	std::span<const Pixel> src0, std::span<const Pixel> src1, std::span<const Pixel> src2)
{
	auto srcWidth = src0.size();
	assert(src0.size() == srcWidth);
	assert(src1.size() == srcWidth);
	assert(src2.size() == srcWidth);
	assert(dst.size() == 3 * srcWidth);
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
	         (((top == mid) && (mid != src0[1])) ||
	          ((top == right) && (mid != top)))
	       ? top : mid;
	dst[2] = (mid != right) && (top != bot) && (top == right)
	       ? top : mid;

	// Central pixels.
	for (auto x : xrange(1u, srcWidth - 1)) {
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
	top = src0[srcWidth - 1];
	bot = src2[srcWidth - 1];
	dst[3 * srcWidth - 3] = (left != mid) && (top != bot) && (top ==left)
	         ? top : mid;
	dst[3 * srcWidth - 2] = (left != mid) && (top != bot) &&
	           (((top == left) && (mid != top)) ||
	            ((top == mid ) && (mid != src0[srcWidth - 2])))
	         ? top : mid;
	dst[3 * srcWidth - 1] = mid;
}

template<std::unsigned_integral Pixel>
void Scale3xScaler<Pixel>::scaleLine1on3Mid(
	std::span<Pixel> dst,
	std::span<const Pixel> src0, std::span<const Pixel> src1, std::span<const Pixel> src2)
{
	auto srcWidth = src0.size();
	assert(src0.size() == srcWidth);
	assert(src1.size() == srcWidth);
	assert(src2.size() == srcWidth);
	assert(dst.size() == 3 * srcWidth);
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
	for (auto x : xrange(1u, srcWidth - 1)) {
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
	top = src0[srcWidth - 1];
	bot = src2[srcWidth - 1];
	dst[3 * srcWidth - 3] = (left != mid) && (top != bot) &&
	           (((left == top) && (mid != src2[srcWidth - 2])) ||
	            ((left == bot) && (mid != src0[srcWidth - 2])))
	         ? left : mid;
	dst[3 * srcWidth - 2] = mid;
	dst[3 * srcWidth - 1] = mid;
}

template<std::unsigned_integral Pixel>
void Scale3xScaler<Pixel>::scale1x1to3x3(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	VLA_SSE_ALIGNED(Pixel, buf0, srcWidth);
	VLA_SSE_ALIGNED(Pixel, buf1, srcWidth);
	VLA_SSE_ALIGNED(Pixel, buf2, srcWidth);

	int srcY = srcStartY;
	auto srcPrev = src.getLine(srcY - 1, buf0);
	auto srcCurr = src.getLine(srcY + 0, buf1);

	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 3) {
		auto srcNext = src.getLine(srcY + 1, buf2);

		auto dstUpper  = dst.acquireLine(dstY + 0);
		scaleLine1on3Half(dstUpper, srcPrev, srcCurr, srcNext);
		dst.releaseLine(dstY + 0, dstUpper);

		auto dstMiddle = dst.acquireLine(dstY + 1);
		scaleLine1on3Mid(dstMiddle, srcPrev, srcCurr, srcNext);
		dst.releaseLine(dstY + 1, dstMiddle);

		auto dstLower  = dst.acquireLine(dstY + 2);
		scaleLine1on3Half(dstLower, srcNext, srcCurr, srcPrev);
		dst.releaseLine(dstY + 2, dstLower);

		srcPrev = srcCurr;
		srcCurr = srcNext;
		std::swap(buf0, buf1);
		std::swap(buf1, buf2);
	}
}

// Force template instantiation.
#if HAVE_16BPP
template class Scale3xScaler<uint16_t>;
#endif
#if HAVE_32BPP
template class Scale3xScaler<uint32_t>;
#endif

} // namespace openmsx
