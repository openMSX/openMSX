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
#include "FrameSource.hh"
#include "ScalerOutput.hh"
#include "openmsx.hh"

namespace openmsx {

template <class Pixel>
Scale2xScaler<Pixel>::Scale2xScaler(const PixelOperations<Pixel>& pixelOps)
	: Scaler2<Pixel>(pixelOps)
{
}

template <class Pixel>
inline void Scale2xScaler<Pixel>::scaleLine_1on2(
	Pixel* __restrict dst0, Pixel* __restrict dst1,
	const Pixel* __restrict src0, const Pixel* __restrict src1,
	const Pixel* __restrict src2, unsigned long srcWidth) __restrict
{
	// For some reason processing the two output lines separately is faster
	// than merging the two in a single loop (even though a single loop
	// only has to fetch the inputs once and can eliminate some common
	// sub-expressions).
	scaleLineHalf_1on2(dst0, src0, src1, src2, srcWidth);
	scaleLineHalf_1on2(dst1, src2, src1, src0, srcWidth);
}

template <class Pixel>
void Scale2xScaler<Pixel>::scaleLineHalf_1on2(
	Pixel* __restrict dst, const Pixel* __restrict src0,
	const Pixel* __restrict src1, const Pixel* __restrict src2,
	unsigned long srcWidth) __restrict
{
	//   n      m is expaned to a b
	// w m e                    c d
	//   s         a = (w == n) && (s != n) && (e != n) ? n : m
	//             b =   .. swap w/e
	//             c =   .. swap n/s
	//             d =   .. swap w/e  n/s

	// First pixel.
	Pixel mid   = src1[0];
	Pixel right = src1[1];
	dst[0] = mid;
	dst[1] = (right == src0[0] && src2[0] != src0[0]) ? src0[0] : mid;

	// Central pixels.
	for (unsigned x = 1; x < srcWidth - 1; ++x) {
		Pixel left = mid;
		mid   = right;
		right = src1[x + 1];
		Pixel top = src0[x];
		Pixel bot = src2[x];
		dst[2 * x + 0] = (left  == top && right != top && bot != top) ? top : mid;
		dst[2 * x + 1] = (right == top && left  != top && bot != top) ? top : mid;
	}

	// Last pixel.
	dst[2 * srcWidth - 2] =
		(mid == src0[srcWidth - 1] && src2[srcWidth - 1] != src0[srcWidth - 1])
		? src0[srcWidth - 1] : right;
	dst[2 * srcWidth - 1] =
		src1[srcWidth - 1];
}

template <class Pixel>
inline void Scale2xScaler<Pixel>::scaleLine_1on1(
	Pixel* __restrict dst0, Pixel* __restrict dst1,
	const Pixel* __restrict src0, const Pixel* __restrict src1,
	const Pixel* __restrict src2, unsigned long srcWidth) __restrict
{
	scaleLineHalf_1on1(dst0, src0, src1, src2, srcWidth);
	scaleLineHalf_1on1(dst1, src2, src1, src0, srcWidth);
}

template <class Pixel>
void Scale2xScaler<Pixel>::scaleLineHalf_1on1(
	Pixel* __restrict dst, const Pixel* __restrict src0,
	const Pixel* __restrict src1, const Pixel* __restrict src2,
	unsigned long srcWidth) __restrict
{
	//    ab ef
	// x0 12 34 5x
	//    cd gh

	// First pixel.
	Pixel mid =   src1[0];
	Pixel right = src1[1];
	dst[0] = mid;

	// Central pixels.
	for (unsigned x = 1; x < srcWidth - 1; ++x) {
		Pixel left = mid;
		mid   = right;
		right = src1[x + 1];
		Pixel top = src0[x];
		Pixel bot = src2[x];
		dst[x] = (left == top && right != top && bot != top) ? top : mid;
	}

	// Last pixel.
	dst[srcWidth - 1] =
		(mid == src0[srcWidth - 1] && src2[srcWidth - 1] != src0[srcWidth - 1])
		? src0[srcWidth - 1] : right;
}

template <class Pixel>
void Scale2xScaler<Pixel>::scale1x1to2x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	int srcY = srcStartY;
	const Pixel* srcPrev = src.getLinePtr<Pixel>(srcY - 1, srcWidth);
	const Pixel* srcCurr = src.getLinePtr<Pixel>(srcY + 0, srcWidth);
	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 2) {
		const Pixel* srcNext = src.getLinePtr<Pixel>(srcY + 1, srcWidth);
		Pixel* dstUpper = dst.acquireLine(dstY + 0);
		Pixel* dstLower = dst.acquireLine(dstY + 1);
		scaleLine_1on2(dstUpper, dstLower,
		               srcPrev, srcCurr, srcNext,
		               srcWidth);
		dst.releaseLine(dstY + 0, dstUpper);
		dst.releaseLine(dstY + 1, dstLower);
		srcPrev = srcCurr;
		srcCurr = srcNext;
	}
}

template <class Pixel>
void Scale2xScaler<Pixel>::scale1x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	int srcY = srcStartY;
	const Pixel* srcPrev = src.getLinePtr<Pixel>(srcY - 1, srcWidth);
	const Pixel* srcCurr = src.getLinePtr<Pixel>(srcY + 0, srcWidth);
	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 2) {
		const Pixel* srcNext = src.getLinePtr<Pixel>(srcY + 1, srcWidth);
		Pixel* dstUpper = dst.acquireLine(dstY + 0);
		Pixel* dstLower = dst.acquireLine(dstY + 1);
		scaleLine_1on1(dstUpper, dstLower,
		               srcPrev, srcCurr, srcNext,
		               srcWidth);
		dst.releaseLine(dstY + 0, dstUpper);
		dst.releaseLine(dstY + 1, dstLower);
		srcPrev = srcCurr;
		srcCurr = srcNext;
	}
}

// Force template instantiation.
#if HAVE_16BPP
template class Scale2xScaler<word>;
#endif
#if HAVE_32BPP
template class Scale2xScaler<unsigned>;
#endif

} // namespace openmsx
