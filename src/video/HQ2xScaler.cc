// $Id$

/*
Original code: Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
openMSX adaptation by Maarten ter Huurne

License: LGPL

Visit the HiEnd3D site for info:
  http://www.hiend3d.com/hq2x.html
*/

#include "HQ2xScaler.hh"
#include "HQCommon.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

template <class Pixel>
HQ2xScaler<Pixel>::HQ2xScaler(const PixelOperations<Pixel>& pixelOps)
	: Scaler2<Pixel>(pixelOps)
{
}

template <class Pixel>
static void scale1on2(const Pixel* in0, const Pixel* in1, const Pixel* in2,
                      Pixel* out0, Pixel* out1, unsigned srcWidth,
                      unsigned* edgeBuf)
{
	unsigned c1, c2, c3, c4, c5, c6, c7, c8, c9;
	c2 = c3 = readPixel(in0);
	c5 = c6 = readPixel(in1);
	c8 = c9 = readPixel(in2);

	unsigned pattern = 0;
	if (edge(c5, c8)) pattern |= 3 <<  6;
	if (edge(c5, c2)) pattern |= 3 <<  9;

	for (unsigned x = 0; x < srcWidth; ++x) {
		c1 = c2; c4 = c5; c7 = c8;
		c2 = c3; c5 = c6; c8 = c9;
		++in0; ++in1; ++in2;

		if (x != srcWidth - 1) {
			c3 = readPixel(in0);
			c6 = readPixel(in1);
			c9 = readPixel(in2);
		}

		pattern = (pattern >> 6) & 0x001F; // left overlap
		// overlaps with left
		//if (edge(c8, c4)) pattern |= 1 <<  0; // B - l: c5-c9 6
		//if (edge(c5, c7)) pattern |= 1 <<  1; // B - l: c6-c8 7
		//if (edge(c5, c4)) pattern |= 1 <<  2; //     l: c5-c6 8
		// overlaps with top and left
		//if (edge(c5, c1)) pattern |= 1 <<  3; //     l: c2-c6 9,  t: c4-c8 0
		//if (edge(c4, c2)) pattern |= 1 <<  4; //     l: c5-c3 10, t: c5-c7 1
		// non-overlapping pixels
		if (edge(c5, c8)) pattern |= 1 <<  5; // B
		if (edge(c5, c9)) pattern |= 1 <<  6; // BR
		if (edge(c6, c8)) pattern |= 1 <<  7; // BR
		if (edge(c5, c6)) pattern |= 1 <<  8; // R
		// overlaps with top
		//if (edge(c2, c6)) pattern |= 1 <<  9; // R - t: c5-c9 6
		//if (edge(c5, c3)) pattern |= 1 << 10; // R - t: c6-c8 7
		//if (edge(c5, c2)) pattern |= 1 << 11; //     t: c5-c8 5
		pattern |= ((edgeBuf[x] &  (1 << 5)            ) << 6) |
		           ((edgeBuf[x] & ((1 << 6) | (1 << 7))) << 3);
		edgeBuf[x] = pattern;

		unsigned pixel1, pixel2, pixel3, pixel4;

#include "HQ2xScaler-1x1to2x2.nn"

		pset(out1 + 0, pixel3);
		pset(out1 + 1, pixel4);
		pset(out0 + 0, pixel1);
		pset(out0 + 1, pixel2);
		out0 += 2; out1 += 2;
	}
}


template <class Pixel>
static void scale1on1(const Pixel* in0, const Pixel* in1, const Pixel* in2,
                      Pixel* out0, Pixel* out1, unsigned srcWidth,
                      unsigned* edgeBuf)
{
	//   +----+----+----+
	//   |    |    |    |
	//   | c1 | c2 | c3 |
	//   +----+----+----+
	//   |    |    |    |
	//   | c4 | c5 | c6 |
	//   +----+----+----+
	//   |    |    |    |
	//   | c7 | c8 | c9 |
	//   +----+----+----+
	unsigned c1, c2, c3, c4, c5, c6, c7, c8, c9;
	c2 = c3 = readPixel(in0);
	c5 = c6 = readPixel(in1);
	c8 = c9 = readPixel(in2);

	unsigned pattern = 0;
	if (edge(c5, c8)) pattern |= 3 <<  6;
	if (edge(c5, c2)) pattern |= 3 <<  9;

	for (unsigned x = 0; x < srcWidth; ++x) {
		c1 = c2; c4 = c5; c7 = c8;
		c2 = c3; c5 = c6; c8 = c9;
		++in0; ++in1; ++in2;

		if (x != srcWidth - 1) {
			c3 = readPixel(in0);
			c6 = readPixel(in1);
			c9 = readPixel(in2);
		}

		pattern = (pattern >> 6) & 0x001F; // left overlap
		// overlaps with left
		//if (edge(c8, c4)) pattern |= 1 <<  0; // B - l: c5-c9 6
		//if (edge(c5, c7)) pattern |= 1 <<  1; // B - l: c6-c8 7
		//if (edge(c5, c4)) pattern |= 1 <<  2; //     l: c5-c6 8
		// overlaps with top and left
		//if (edge(c5, c1)) pattern |= 1 <<  3; //     l: c2-c6 9,  t: c4-c8 0
		//if (edge(c4, c2)) pattern |= 1 <<  4; //     l: c5-c3 10, t: c5-c7 1
		// non-overlapping pixels
		if (edge(c5, c8)) pattern |= 1 <<  5; // B
		if (edge(c5, c9)) pattern |= 1 <<  6; // BR
		if (edge(c6, c8)) pattern |= 1 <<  7; // BR
		if (edge(c5, c6)) pattern |= 1 <<  8; // R
		// overlaps with top
		//if (edge(c2, c6)) pattern |= 1 <<  9; // R - t: c5-c9 6
		//if (edge(c5, c3)) pattern |= 1 << 10; // R - t: c6-c8 7
		//if (edge(c5, c2)) pattern |= 1 << 11; //     t: c5-c8 5
		pattern |= ((edgeBuf[x] &  (1 << 5)            ) << 6) |
		           ((edgeBuf[x] & ((1 << 6) | (1 << 7))) << 3);
		edgeBuf[x] = pattern;

		unsigned pixel1, pixel2;

#include "HQ2xScaler-1x1to1x2.nn"

		pset(out1++, pixel2);
		pset(out0++, pixel1);
	}
}

template <class Pixel>
void HQ2xScaler<Pixel>::scale1x1to2x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	Pixel* const dummy = 0;
	int srcY = srcStartY;
	const Pixel* srcPrev = src.getLinePtr(srcY - 1, srcWidth, dummy);
	const Pixel* srcCurr = src.getLinePtr(srcY + 0, srcWidth, dummy);

	assert(srcWidth <= 1024);
	unsigned edgeBuf[1024];

	unsigned x = 0;
	unsigned c1 = readPixel(&srcPrev[x]);
	unsigned c2 = readPixel(&srcCurr[x]);
	unsigned pattern = edge(c1, c2) ? ((1 << 6) | (1 << 7)) : 0;
	for (/* */; x < (srcWidth - 1); ++x) {
		pattern >>= 6;
		unsigned n1 = readPixel(&srcPrev[x + 1]);
		unsigned n2 = readPixel(&srcCurr[x + 1]);
		if (edge(c1, c2)) pattern |= (1 << 5);
		if (edge(c1, n2)) pattern |= (1 << 6);
		if (edge(c2, n1)) pattern |= (1 << 7);
		edgeBuf[x] = pattern;
		c1 = n1; c2 = n2;
	}
	pattern >>= 6;
	if (edge(c1, c2)) pattern |= (1 << 5) | (1 << 6) | (1 << 7);
	edgeBuf[x] = pattern;

	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 2) {
		const Pixel* srcNext = src.getLinePtr(srcY + 1, srcWidth, dummy);
		Pixel* dstUpper = dst.getLinePtr(dstY + 0, dummy);
		Pixel* dstLower = dst.getLinePtr(dstY + 1, dummy);
		scale1on2(srcPrev, srcCurr, srcNext, dstUpper, dstLower,
		          srcWidth, edgeBuf);
		srcPrev = srcCurr;
		srcCurr = srcNext;
	}
}

template <class Pixel>
void HQ2xScaler<Pixel>::scale1x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	Pixel* dummy = 0;
	int srcY = srcStartY;
	const Pixel* srcPrev = src.getLinePtr(srcY - 1, srcWidth, dummy);
	const Pixel* srcCurr = src.getLinePtr(srcY + 0, srcWidth, dummy);

	assert(srcWidth <= 1024);
	unsigned edgeBuf[1024];

	unsigned x = 0;
	unsigned c1 = readPixel(&srcPrev[x]);
	unsigned c2 = readPixel(&srcCurr[x]);
	unsigned pattern = edge(c1, c2) ? ((1 << 6) | (1 << 7)) : 0;
	for (/* */; x < (srcWidth - 1); ++x) {
		pattern >>= 6;
		unsigned n1 = readPixel(&srcPrev[x + 1]);
		unsigned n2 = readPixel(&srcCurr[x + 1]);
		if (edge(c1, c2)) pattern |= (1 << 5);
		if (edge(c1, n2)) pattern |= (1 << 6);
		if (edge(c2, n1)) pattern |= (1 << 7);
		edgeBuf[x] = pattern;
		c1 = n1; c2 = n2;
	}
	pattern >>= 6;
	if (edge(c1, c2)) pattern |= (1 << 5) | (1 << 6) | (1 << 7);
	edgeBuf[x] = pattern;

	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 2) {
		const Pixel* srcNext = src.getLinePtr(srcY + 1, srcWidth, dummy);
		Pixel* dstUpper = dst.getLinePtr(dstY + 0, dummy);
		Pixel* dstLower = dst.getLinePtr(dstY + 1, dummy);
		scale1on1(srcPrev, srcCurr, srcNext, dstUpper, dstLower,
		          srcWidth, edgeBuf);
		srcPrev = srcCurr;
		srcCurr = srcNext;
	}
}


// Force template instantiation.
template class HQ2xScaler<word>;
template class HQ2xScaler<unsigned>;

} // namespace openmsx
