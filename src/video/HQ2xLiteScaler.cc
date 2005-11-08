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
#include <algorithm>
#include <cassert>

using std::min;

namespace openmsx {

template <class Pixel>
HQ2xLiteScaler<Pixel>::HQ2xLiteScaler(SDL_PixelFormat* format)
	: Scaler2<Pixel>(format)
{
}

template <class Pixel>
static void scaleLine256(const Pixel* in0, const Pixel* in1, const Pixel* in2,
                         Pixel* out0, Pixel* out1)
{
	const unsigned WIDTH = 320;

	unsigned c1, c2, c3, c4, c5, c6, c7, c8, c9;
	c2 = c3 = readPixel(in0);
	c5 = c6 = readPixel(in1);
	c8 = c9 = readPixel(in2);

	for (unsigned x = 0; x < WIDTH; ++x) {
		c1 = c2; c4 = c5; c7 = c8;
		c2 = c3; c5 = c6; c8 = c9;
		++in0; ++in1; ++in2;

		if (x != WIDTH - 1) {
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
		unsigned pixel2 = c5;
		unsigned pixel3 = c5;
		unsigned pixel4 = c5;
		switch (pattern) {
		case 0x12: case 0x16: case 0x1e: case 0x32:
                case 0x36: case 0x3e: case 0x56: case 0x76:
			if (c2 == c6) pixel2 = interpolate<1, 1>(c5, c2);
			break;

		case 0x50: case 0x51: case 0xd0: case 0xd1:
		case 0xd2: case 0xd3: case 0xd8: case 0xd9:
			if (c6 == c8) pixel4 = interpolate<1, 1>(c5, c6);
			break;

		case 0x48: case 0x4c: case 0x68: case 0x6a:
                case 0x6c: case 0x6e: case 0x78: case 0x7c:
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c4);
			break;


		case 0x0a: case 0x0b: case 0x1b: case 0x4b:
                case 0x8a: case 0x8b: case 0x9b: case 0xcb:
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c2);
			break;

		case 0x13: case 0x17: case 0x33: case 0x37:
		case 0x77:
			if (c2 == c6) {
				pixel1 = interpolate<5, 2, 1>(c5, c2, c4);
				pixel2 = interpolate<1, 3>(c5, c2);
			}
			break;

		case 0x92: case 0x96: case 0xb2: case 0xb6:
		case 0xbe:
			if (c2 == c6) {
				pixel2 = interpolate<1, 3>(c5, c2);
				pixel4 = interpolate<5, 2, 1>(c5, c2, c8);
			}
			break;

		case 0x54: case 0x55: case 0xd4: case 0xd5:
		case 0xdd:
			if (c6 == c8) {
				pixel2 = interpolate<5, 2, 1>(c5, c6, c2);
				pixel4 = interpolate<1, 3>(c5, c6);
			}
			break;

		case 0x70: case 0x71: case 0xf0: case 0xf1:
		case 0xf3:
			if (c6 == c8) {
				pixel3 = interpolate<5, 2, 1>(c5, c6, c4);
				pixel4 = interpolate<1, 3>(c5, c6);
			}
			break;

		case 0xc8: case 0xcc: case 0xe8: case 0xec:
		case 0xee:
			if (c4 == c8) {
				pixel3 = interpolate<1, 3>(c5, c4);
				pixel4 = interpolate<5, 2, 1>(c5, c4, c6);
			}
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
				pixel1 = interpolate<1, 3>(c5, c2);
				pixel3 = interpolate<5, 2, 1>(c5, c2, c8);
			}
			break;

		case 0x0e: case 0x0f: case 0x8e: case 0x8f:
		case 0xcf:
			if (c2 == c4) {
				pixel1 = interpolate<1, 3>(c5, c2);
				pixel2 = interpolate<5, 2, 1>(c5, c2, c6);
			}
			break;

		case 0x1a: case 0x1f: case 0x5f: {
			unsigned t = interpolate<1, 1>(c5, c2);
			if (c2 == c4) pixel1 = t;
			if (c2 == c6) pixel2 = t;
			break;
		}
		case 0x52: case 0xd6: case 0xde: {
			unsigned t = interpolate<1, 1>(c5, c6);
			if (c2 == c6) pixel2 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0x58: case 0xf8: case 0xfa: {
			unsigned t = interpolate<1, 1>(c5, c8);
			if (c4 == c8) pixel3 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0x4a: case 0x6b: case 0x7b: {
			unsigned t = interpolate<1, 1>(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		case 0x3a: case 0x9a: case 0xba: {
			unsigned t = interpolate<3, 1>(c5, c2);
			if (c2 == c4) pixel1 = t;
			if (c2 == c6) pixel2 = t;
			break;
		}
		case 0x53: case 0x72: case 0x73: {
			unsigned t = interpolate<3, 1>(c5, c6);
			if (c2 == c6) pixel2 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0x59: case 0x5c: case 0x5d: {
			unsigned t = interpolate<3, 1>(c5, c8);
			if (c4 == c8) pixel3 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0x4e: case 0xca: case 0xce: {
			unsigned t = interpolate<3, 1>(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		case 0x5a: {
			unsigned t1 = interpolate<3, 1>(c5, c2);
			if (c2 == c4) pixel1 = t1;
			if (c2 == c6) pixel2 = t1;
			unsigned t2 = interpolate<3, 1>(c5, c8);
			if (c4 == c8) pixel3 = t2;
			if (c6 == c8) pixel4 = t1;
			break;
		}
		case 0xdc:
			if (c4 == c8) pixel3 = interpolate<3, 1>(c5, c8);
			if (c6 == c8) pixel4 = interpolate<1, 1>(c5, c8);
			break;

		case 0x9e:
			if (c2 == c4) pixel1 = interpolate<3, 1>(c5, c2);
			if (c2 == c6) pixel2 = interpolate<1, 1>(c5, c2);
			break;

		case 0xea:
			if (c2 == c4) pixel1 = interpolate<3, 1>(c5, c4);
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c4);
			break;

		case 0xf2:
			if (c2 == c6) pixel2 = interpolate<3, 1>(c5, c6);
			if (c6 == c8) pixel4 = interpolate<1, 1>(c5, c6);
			break;

		case 0x3b:
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) pixel2 = interpolate<3, 1>(c5, c2);
			break;

		case 0x79:
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 0x57:
			if (c2 == c6) pixel2 = interpolate<1, 1>(c5, c6);
			if (c6 == c8) pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 0x4f:
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c4);
			if (c4 == c8) pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 0x7a: {
			unsigned t = interpolate<3, 1>(c5, c2);
			if (c2 == c4) pixel1 = t;
			if (c2 == c6) pixel2 = t;
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) pixel4 = interpolate<3, 1>(c5, c8);
			break;
		}
		case 0x5e: {
			if (c2 == c4) pixel1 = interpolate<3, 1>(c5, c2);
			if (c2 == c6) pixel2 = interpolate<1, 1>(c5, c2);
			unsigned t = interpolate<3, 1>(c5, c8);
			if (c4 == c8) pixel3 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0xda: {
			unsigned t = interpolate<3, 1>(c5, c2);
			if (c2 == c4) pixel1 = t;
			if (c2 == c6) pixel2 = t;
			if (c4 == c8) pixel3 = interpolate<3, 1>(c5, c8);
			if (c6 == c8) pixel4 = interpolate<1, 1>(c5, c8);
			break;
		}
		case 0x5b: {
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) pixel2 = interpolate<3, 1>(c5, c2);
			unsigned t = interpolate<3, 1>(c5, c8);
			if (c4 == c8) pixel3 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0xc9: case 0xcd:
			if (c4 == c8) pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 0x2e: case 0xae:
			if (c2 == c4) pixel1 = interpolate<3, 1>(c5, c2);
			break;

		case 0x93: case 0xb3:
			if (c2 == c6) pixel2 = interpolate<3, 1>(c5, c2);
			break;

		case 0x74: case 0x75:
			if (c6 == c8) pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 0x7e:
			if (c2 == c6) pixel2 = interpolate<1, 1>(c5, c2);
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c4);
			break;

		case 0xdb:
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c2);
			if (c6 == c8) pixel4 = interpolate<1, 1>(c5, c6);
			break;

		case 0xe9: case 0xed:
			if (c4 == c8) pixel3 = interpolate<7, 1>(c5, c4);
			break;

		case 0x2f: case 0xaf:
			if (c2 == c4) pixel1 = interpolate<7, 1>(c5, c2);
			break;

		case 0x97: case 0xb7:
			if (c2 == c6) pixel2 = interpolate<7, 1>(c5, c2);
			break;

		case 0xf4: case 0xf5:
			if (c6 == c8) pixel4 = interpolate<7, 1>(c5, c6);
			break;

		case 0xfc:
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) pixel4 = interpolate<7, 1>(c5, c8);
			break;

		case 0xf9:
			if (c4 == c8) pixel3 = interpolate<7, 1>(c5, c8);
			if (c6 == c8) pixel4 = interpolate<1, 1>(c5, c8);
			break;

		case 0xeb:
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c4);
			if (c4 == c8) pixel3 = interpolate<7, 1>(c5, c4);
			break;

		case 0x6f:
			if (c2 == c4) pixel1 = interpolate<7, 1>(c5, c4);
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c4);
			break;

		case 0x3f:
			if (c2 == c4) pixel1 = interpolate<7, 1>(c5, c2);
			if (c2 == c6) pixel2 = interpolate<1, 1>(c5, c2);
			break;

		case 0x9f:
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) pixel2 = interpolate<7, 1>(c5, c2);
			break;

		case 0xd7:
			if (c2 == c6) pixel2 = interpolate<7, 1>(c5, c6);
			if (c6 == c8) pixel4 = interpolate<1, 1>(c5, c6);
			break;

		case 0xf6:
			if (c2 == c6) pixel2 = interpolate<1, 1>(c5, c6);
			if (c6 == c8) pixel4 = interpolate<7, 1>(c5, c6);
			break;

		case 0xfe:
			if (c2 == c6) pixel2 = interpolate<1, 1>(c5, c2);
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) pixel4 = interpolate<7, 1>(c5, c8);
			break;

		case 0xfd: {
			unsigned t = interpolate<7, 1>(c5, c8);
			if (c4 == c8) pixel3 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0xfb:
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c2);
			if (c4 == c8) pixel3 = interpolate<7, 1>(c5, c8);
			if (c6 == c8) pixel4 = interpolate<1, 1>(c5, c8);
			break;

		case 0xef: {
			unsigned t = interpolate<7, 1>(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		case 0x7f:
			if (c2 == c4) pixel1 = interpolate<7, 1>(c5, c2);
			if (c2 == c6) pixel2 = interpolate<1, 1>(c5, c2);
			if (c4 == c8) pixel3 = interpolate<1, 1>(c5, c4);
			break;

		case 0xbf: {
			unsigned t = interpolate<7, 1>(c5, c2);
			if (c2 == c4) pixel1 = t;
			if (c2 == c6) pixel2 = t;
			break;
		}
		case 0xdf:
			if (c2 == c4) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) pixel2 = interpolate<7, 1>(c5, c2);
			if (c6 == c8) pixel4 = interpolate<1, 1>(c5, c6);
			break;

		case 0xf7: {
			unsigned t = interpolate<7, 1>(c5, c6);
			if (c2 == c6) pixel2 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0xff: {
			unsigned t1 = interpolate<7, 1>(c5, c2);
			if (c2 == c4) pixel1 = t1;
			if (c2 == c6) pixel2 = t1;
			unsigned t2 = interpolate<7, 1>(c5, c8);
			if (c4 == c8) pixel3 = t2;
			if (c6 == c8) pixel4 = t2;
			break;
		}
		}
		pset(out1 + 0, pixel3);
		pset(out1 + 1, pixel4);
		pset(out0 + 0, pixel1);
		pset(out0 + 1, pixel2);
		out0 += 2; out1 += 2;
	}
}

template <class Pixel>
static void scaleLine512(const Pixel* in0, const Pixel* in1, const Pixel* in2,
                         Pixel* out0, Pixel* out1)
{
	const unsigned WIDTH = 640; // TODO: Specify this in a clean way.

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

	for (unsigned i = 0; i < WIDTH; i++) {
		c1 = c2; c4 = c5; c7 = c8;
		c2 = c3; c5 = c6; c8 = c9;
		++in0; ++in1; ++in2;

		if (i < WIDTH - 1) {
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
void HQ2xLiteScaler<Pixel>::scale1x1to2x2(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface* dst, unsigned dstStartY, unsigned dstEndY)
{
	unsigned prevY = srcStartY;
	for (unsigned dstY = dstStartY; dstY < dstEndY; dstY += 2) {
		Pixel* const dummy = 0;
		const Pixel* srcPrev = src.getLinePtr(prevY, dummy);
		const Pixel* srcCurr = src.getLinePtr(srcStartY, dummy);
		const Pixel* srcNext = src.getLinePtr(
			min(srcStartY + 1, srcEndY - 1), dummy);
		Pixel* dstUpper = dst->getLinePtr(dstY, dummy);
		Pixel* dstLower = dst->getLinePtr(
			min(dstY + 1, dstEndY - 1), dummy);
		scaleLine256(srcPrev, srcCurr, srcNext, dstUpper, dstLower);
		prevY = srcStartY;
		++srcStartY;
	}
}

template <class Pixel>
void HQ2xLiteScaler<Pixel>::scale1x1to1x2(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface* dst, unsigned dstStartY, unsigned dstEndY)
{
	unsigned prevY = srcStartY;
	for (unsigned dstY = dstStartY; dstY < dstEndY; dstY += 2) {
		Pixel* const dummy = 0;
		const Pixel* srcPrev = src.getLinePtr(prevY, dummy);
		const Pixel* srcCurr = src.getLinePtr(srcStartY, dummy);
		const Pixel* srcNext = src.getLinePtr(
			min(srcStartY + 1, srcEndY - 1), dummy);
		Pixel* dstUpper = dst->getLinePtr(dstY, dummy);
		Pixel* dstLower = dst->getLinePtr(
			min(dstY + 1, dstEndY - 1), dummy);
		scaleLine512(srcPrev, srcCurr, srcNext, dstUpper, dstLower);
		prevY = srcStartY;
		++srcStartY;
	}
}


// Force template instantiation.
template class HQ2xLiteScaler<word>;
template class HQ2xLiteScaler<unsigned>;

} // namespace openmsx
