// $Id$

/*
 * Lightweight version of the hq2x scaler (http://www.hiend3d.com/hq2x.html)
 *
 * The difference between this version and the full version of hq2x is the
 * calculation of the distance between two colors. Here it's simplified to
 * just  color1 == color2
 * Because of this simplification a lot of the interpolation code can also
 * be simplified.
 * For low color images the result is very close to the full hq2x image but
 * it is calculated _a_lot_ faster.
 */

#include "HQ2xLiteScaler.hh"
#include "HQCommon.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

template <class Pixel>
HQ2xLiteScaler<Pixel>::HQ2xLiteScaler(const PixelOperations<Pixel>& pixelOps)
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
	if (c5 != c8) pattern |= 3 <<  6;
	if (c5 != c2) pattern |= 3 <<  9;

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
		//if (c8 != c4) pattern |= 1 <<  0; // B - l: c5-c9 6
		//if (c5 != c7) pattern |= 1 <<  1; // B - l: c6-c8 7
		//if (c5 != c4) pattern |= 1 <<  2; //     l: c5-c6 8
		// overlaps with top and left
		//if (c5 != c1) pattern |= 1 <<  3; //     l: c2-c6 9,  t: c4-c8 0
		//if (c4 != c2) pattern |= 1 <<  4; //     l: c5-c3 10, t: c5-c7 1
		// non-overlapping pixels
		if (c5 != c8) pattern |= 1 <<  5; // B
		if (c5 != c9) pattern |= 1 <<  6; // BR
		if (c6 != c8) pattern |= 1 <<  7; // BR
		if (c5 != c6) pattern |= 1 <<  8; // R
		// overlaps with top
		//if (c2 != c6) pattern |= 1 <<  9; // R - t: c5-c9 6
		//if (c5 != c3) pattern |= 1 << 10; // R - t: c6-c8 7
		//if (c5 != c2) pattern |= 1 << 11; //     t: c5-c8 5
		pattern |= ((edgeBuf[x] &  (1 << 5)            ) << 6) |
		           ((edgeBuf[x] & ((1 << 6) | (1 << 7))) << 3);
		edgeBuf[x] = pattern;

		unsigned pixel1, pixel2, pixel3, pixel4;

#include "HQ2xLiteScaler-1x1to2x2.nn"

		pset(out1 + 0, pixel3);
		pset(out1 + 1, pixel4);
		pset(out0 + 0, pixel1);
		pset(out0 + 1, pixel2);
		out0 += 2; out1 += 2;
	}
}

template <class Pixel>
static void scale1on1(const Pixel* in0, const Pixel* in1, const Pixel* in2,
                      Pixel* out0, Pixel* out1, unsigned srcWidth)
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

	for (unsigned i = 0; i < srcWidth; i++) {
		c1 = c2; c4 = c5; c7 = c8;
		c2 = c3; c5 = c6; c8 = c9;
		++in0; ++in1; ++in2;

		if (i < srcWidth - 1) {
			c3 = readPixel(in0);
			c6 = readPixel(in1);
			c9 = readPixel(in2);
		}

		unsigned pattern = 0;
		if (c5 != c1) pattern |= 0x01;
		if (c5 != c2) pattern |= 0x02;
		if (c5 != c3) pattern |= 0x04;
		if (c5 != c4) pattern |= 0x08;
		if (c5 != c6) pattern |= 0x10;
		if (c5 != c7) pattern |= 0x20;
		if (c5 != c8) pattern |= 0x40;
		if (c5 != c9) pattern |= 0x80;

                unsigned pixel1 = c5;
		unsigned pixel3 = c5;
		switch (pattern) {
		case 0x0a: case 0x0b: case 0x1a: case 0x1b:
		case 0x1f: case 0x3b: case 0x4b: case 0x5f:
                case 0x8a: case 0x8b: case 0x9b: case 0x9f:
		case 0xcb: case 0xdb: case 0xdf:
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c4);
			break;

		case 0x48: case 0x4c: case 0x58: case 0x68:
		case 0x6a: case 0x6c: case 0x6e: case 0x78:
		case 0x79: case 0x7c: case 0x7e: case 0xf8:
		case 0xfa: case 0xfc: case 0xfe:
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c4);
			break;

		case 0x13: case 0x17: case 0x33: case 0x37:
		case 0x77:
			if (c2 == c6) pixel1 = interpolate<5, 2, 1>(c5, c2, c4);
			break;

		case 0x70: case 0x71: case 0xf0: case 0xf1:
		case 0xf3:
			if (c6 == c8) pixel3 = interpolate<5, 2, 1>(c5, c8, c4);
			break;

		case 0xc8: case 0xcc: case 0xe8: case 0xec:
		case 0xee:
			if (c4 == c8) pixel3 = interpolate<1, 3>(c5, c4);
			break;

		case 0x49: case 0x4d: case 0x69: case 0x6d:
		case 0x7d:
			if (c4 == c8) {
				pixel1 = interpolate<5, 2, 1>(c5, c4, c2);
				pixel3 = interpolate<1, 3>(c5, c4);
			}
			break;

		case 0x2a: case 0x2b: case 0xaa: case 0xab:
		case 0xbb:
			if (c2 == c4) {
				pixel1 = interpolate<1, 3>(c5, c4);
				pixel3 = interpolate<5, 2, 1>(c5, c4, c8);
			}
			break;

		case 0x0e: case 0x0f: case 0x8e: case 0x8f:
		case 0xcf:
			if (c2 == c4) pixel1 = interpolate<1, 3>(c5, c4);
			break;

		case 0x4a: case 0x6b: case 0x7b: {
			unsigned t = interpolate<1, 1>(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		case 0x2e: case 0x3a: case 0x9a: case 0x9e:
		case 0xae: case 0xba:
			if (c2 == c4) pixel1 = interpolate<3, 1>(c5, c4);
			break;

		case 0x59: case 0x5c: case 0x5d: case 0xc9:
		case 0xcd: case 0xdc:
			if (c4 == c8) pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 0x4e: case 0x5a: case 0x5e: case 0xca:
		case 0xce: case 0xda: {
			unsigned t = interpolate<3, 1>(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		case 0x7a: case 0xea:
			if (c2 == c4) pixel1 = interpolate<3, 1>(c5, c4);
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c4);
			break;

		case 0x4f: case 0x5b:
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c4);
			if (c4 == c8) pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 0x2f: case 0x3f: case 0xaf: case 0xbf:
			if (c2 == c4) pixel1 = interpolate<7, 1>(c5, c4);
			break;

		case 0xe9: case 0xed: case 0xf9: case 0xfd:
			if (c4 == c8) pixel3 = interpolate<7, 1>(c5, c4);
			break;

		case 0xeb: case 0xfb:
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c4);
			if (c4 == c8) pixel3 = interpolate<7, 1>(c5, c4);
			break;

		case 0x6f: case 0x7f:
			if (c2 == c4) pixel1 = interpolate<7, 1>(c5, c4);
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c4);
			break;

		case 0xef: case 0xff: {
			unsigned t = interpolate<7, 1>(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		}
		pset(out1++, pixel3);
		pset(out0++, pixel1);
	}
}

template <class Pixel>
void HQ2xLiteScaler<Pixel>::scale1x1to2x2(FrameSource& src,
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
	unsigned pattern = (c1 != c2) ? ((1 << 6) | (1 << 7)) : 0;
	for (/* */; x < (srcWidth - 1); ++x) {
		pattern >>= 6;
		unsigned n1 = readPixel(&srcPrev[x + 1]);
		unsigned n2 = readPixel(&srcCurr[x + 1]);
		if (c1 != c2) pattern |= (1 << 5);
		if (c1 != n2) pattern |= (1 << 6);
		if (c2 != n1) pattern |= (1 << 7);
		edgeBuf[x] = pattern;
		c1 = n1; c2 = n2;
	}
	pattern >>= 6;
	if (c1 != c2) pattern |= (1 << 5) | (1 << 6) | (1 << 7);
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
void HQ2xLiteScaler<Pixel>::scale1x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	Pixel* const dummy = 0;
	int srcY = srcStartY;
	const Pixel* srcPrev = src.getLinePtr(srcY - 1, srcWidth, dummy);
	const Pixel* srcCurr = src.getLinePtr(srcY + 0, srcWidth, dummy);
	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 2) {
		const Pixel* srcNext = src.getLinePtr(srcY + 1, srcWidth, dummy);
		Pixel* dstUpper = dst.getLinePtr(dstY + 0, dummy);
		Pixel* dstLower = dst.getLinePtr(dstY + 1, dummy);
		scale1on1(srcPrev, srcCurr, srcNext, dstUpper, dstLower,
		          srcWidth);
		srcPrev = srcCurr;
		srcCurr = srcNext;
	}
}


// Force template instantiation.
template class HQ2xLiteScaler<word>;
template class HQ2xLiteScaler<unsigned>;

} // namespace openmsx
