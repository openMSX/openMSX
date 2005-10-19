// $Id$

/*
Original code: Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
openMSX adaptation by Wouter Vermaelen

License: LGPL

Visit the HiEnd3D site for info:
  http://www.hiend3d.com/hq2x.html
*/

#include "HQ3xLiteScaler.hh"
#include "FrameSource.hh"
#include <algorithm>
#include <cassert>

using std::min;

namespace openmsx {

template <class Pixel>
HQ3xLiteScaler<Pixel>::HQ3xLiteScaler(SDL_PixelFormat* format)
	: Scaler<Pixel>(format)
{
}

template <class Pixel>
static inline unsigned readPixel(const Pixel* pIn)
{
	// TODO: Use surface info instead.
	Pixel p = *pIn;
	if (sizeof(Pixel) == 2) {
		return ((p & 0xF800) << 8) |
		       ((p & 0x07C0) << 5) | // drop lowest green bit
		       ((p & 0x001F) << 3);
	} else {
		return p & 0xF8F8F8;
	}
}

template <class Pixel>
static inline void pset(Pixel* pOut, unsigned p)
{
	// TODO: Use surface info instead.
	if (sizeof(Pixel) == 2) {
		*pOut = ((p & 0xF80000) >> 8) |
			((p & 0x00FC00) >> 5) |
			((p & 0x0000F8) >> 3);
	} else {
		*pOut = (p & 0xF8F8F8) | ((p & 0xE0E0E0) >> 5);
	}
}

template <int w1, int w2>
static inline unsigned interpolate(unsigned c1, unsigned c2)
{
	enum { wsum = w1 + w2 };
	return (c1 * w1 + c2 * w2) / wsum;
}

template <int w1, int w2, int w3>
static inline unsigned interpolate(unsigned c1, unsigned c2, unsigned c3)
{
	enum { wsum = w1 + w2 + w3 };
	if (wsum <= 8) {
		// Because the lower 3 bits of each colour component (R,G,B) are
		// zeroed out, we can operate on a single integer as if it is
		// a vector.
		return (c1 * w1 + c2 * w2 + c3 * w3) / wsum;
	} else {
		return ((((c1 & 0x00FF00) * w1 +
		          (c2 & 0x00FF00) * w2 +
		          (c3 & 0x00FF00) * w3) & (0x00FF00 * wsum)) |
		        (((c1 & 0xFF00FF) * w1 +
		          (c2 & 0xFF00FF) * w2 +
		          (c3 & 0xFF00FF) * w3) & (0xFF00FF * wsum))) / wsum;
	}
}

template <class Pixel>
static void scaleLine256(const Pixel* in0, const Pixel* in1, const Pixel* in2,
                         Pixel* out0, Pixel* out1, Pixel*out2)
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
		unsigned pixel6 = c5;
		unsigned pixel7 = c5;
		unsigned pixel8 = c5;
		unsigned pixel9 = c5;
		switch (pattern) {
		case 0x12: case 0x16: case 0x1E: case 0x32:
                case 0x36: case 0x3E: case 0x56: case 0x76:
			if (c2 == c6) {
				pixel2 = interpolate<7, 1>(c5, c2);
				pixel3 = interpolate<1, 7>(c5, c2);
				pixel6 = interpolate<7, 1>(c5, c2);
			}
			break;

		case 0x50: case 0x51: case 0xD0: case 0xD1:
		case 0xD2: case 0xD3: case 0xD8: case 0xD9:
			if (c6 == c8) {
				pixel6 = interpolate<7, 1>(c5, c6);
				pixel8 = interpolate<7, 1>(c5, c6);
				pixel9 = interpolate<1, 7>(c5, c6);
			}
			break;

		case 0x48: case 0x4C: case 0x68: case 0x6A:
		case 0x6C: case 0x6E: case 0x78: case 0x7C:
			if (c8 == c4) {
				pixel4 = interpolate<7, 1>(c5, c4);
				pixel7 = interpolate<1, 7>(c5, c4);
				pixel8 = interpolate<7, 1>(c5, c4);
			}
			break;

		case 0x0A: case 0x0B: case 0x1B: case 0x4B:
		case 0x8A: case 0x8B: case 0x9B: case 0xCB:
			if (c4 == c2) {
				pixel1 = interpolate<1, 7>(c5, c2);
				pixel2 = interpolate<7, 1>(c5, c2);
				pixel4 = interpolate<7, 1>(c5, c2);
			}
			break;

		case 0x13: case 0x17: case 0x33: case 0x37:
		case 0x77:
			if (c2 == c6) {
				pixel1 = interpolate<3, 1>(c5, c2);
				pixel2 = interpolate<1, 3>(c5, c2);
				pixel3 = c2;
				pixel6 = interpolate<3, 1>(c5, c2);
			}
			break;

		case 0x92: case 0x96: case 0xB2: case 0xB6:
		case 0xBE:
			if (c2 == c6) {
				pixel2 = interpolate<3, 1>(c5, c2);
				pixel3 = c2;
				pixel6 = interpolate<1, 3>(c5, c2);
				pixel9 = interpolate<3, 1>(c5, c2);
			}
			break;

		case 0x54: case 0x55: case 0xD4: case 0xD5:
		case 0xDD:
			if (c6 == c8) {
				pixel3 = interpolate<3, 1>(c5, c6);
				pixel6 = interpolate<1, 3>(c5, c6);
				pixel8 = interpolate<3, 1>(c5, c6);
				pixel9 = c6;
			}
			break;

		case 0x70: case 0x71: case 0xF0: case 0xF1:
		case 0xF3:
			if (c6 == c8) {
				pixel6 = interpolate<3, 1>(c5, c6);
				pixel7 = interpolate<3, 1>(c5, c8);
				pixel8 = interpolate<1, 3>(c5, c6);
				pixel9 = c6;
			}
			break;

		case 0xC8: case 0xCC: case 0xE8: case 0xEC:
		case 0xEE:
			if (c8 == c4) {
				pixel4 = interpolate<3, 1>(c5, c4);
				pixel7 = c4;
				pixel8 = interpolate<1, 3>(c5, c4);
				pixel9 = interpolate<3, 1>(c5, c4);
			}
			break;

		case 0x49: case 0x4D: case 0x69: case 0x6D:
		case 0x7D:
			if (c8 == c4) {
				pixel1 = interpolate<3, 1>(c5, c4);
				pixel4 = interpolate<1, 3>(c5, c4);
				pixel7 = c4;
				pixel8 = interpolate<3, 1>(c5, c4);
			}
			break;

		case 0x2A: case 0x2B: case 0xAA: case 0xAB:
		case 0xBB:
			if (c4 == c2) {
				pixel1 = c2;
				pixel2 = interpolate<3, 1>(c5, c2);
				pixel4 = interpolate<1, 3>(c5, c2);
				pixel7 = interpolate<3, 1>(c5, c2);
			}
			break;

		case 0x0E: case 0x0F: case 0x8E: case 0x8F:
		case 0xCF:
			if (c4 == c2) {
				pixel1 = c2;
				pixel2 = interpolate<1, 3>(c5, c2);
				pixel3 = interpolate<3, 1>(c5, c2);
				pixel4 = interpolate<3, 1>(c5, c2);
			}
			break;

		case 0x1A: case 0x1F: case 0x5F:
			if (c4 == c2) {
				pixel1 = interpolate<1, 7>(c5, c2);
				pixel4 = interpolate<7, 1>(c5, c2);
			}
			if (c2 == c6) {
				pixel3 = interpolate<1, 7>(c5, c2);
				pixel6 = interpolate<7, 1>(c5, c2);
			}
			break;

		case 0x52: case 0xD6: case 0xDE:
			if (c2 == c6) {
				pixel2 = interpolate<7, 1>(c5, c6);
				pixel3 = interpolate<1, 7>(c5, c6);
			}
			if (c6 == c8) {
				pixel8 = interpolate<7, 1>(c5, c6);
				pixel9 = interpolate<1, 7>(c5, c6);
			}
			break;

		case 0x58: case 0xF8: case 0xFA:
			if (c8 == c4) {
				pixel4 = interpolate<7, 1>(c5, c8);
				pixel7 = interpolate<1, 7>(c5, c8);
			}
			if (c6 == c8) {
				pixel6 = interpolate<7, 1>(c5, c8);
				pixel9 = interpolate<1, 7>(c5, c8);
			}
			break;

		case 0x4A: case 0x6B: case 0x7B:
			if (c4 == c2) {
				pixel1 = interpolate<1, 7>(c5, c4);
				pixel2 = interpolate<7, 1>(c5, c4);
			}
			if (c8 == c4) {
				pixel7 = interpolate<1, 7>(c5, c4);
				pixel8 = interpolate<7, 1>(c5, c4);
			}
			break;

		case 0x3A: case 0x9A: case 0xBA:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c2);
			break;

		case 0x53: case 0x72: case 0x73:
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c6);
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c6);
			break;

		case 0x59: case 0x5C: case 0x5D:
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c8);
			break;

		case 0x4E: case 0xCA: case 0xCE:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c4);
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c4);
			break;

		case 0x5A:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c2);
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c8);
			break;

		case 0xDC:
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) {
				pixel6 = interpolate<7, 1>(c5, c8);
				pixel8 = interpolate<7, 1>(c5, c8);
				pixel9 = interpolate<1, 7>(c5, c8);
			}
			break;

		case 0x9E:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) {
				pixel2 = interpolate<7, 1>(c5, c2);
				pixel3 = interpolate<1, 7>(c5, c2);
				pixel6 = interpolate<7, 1>(c5, c2);
			}
			break;

		case 0xEA:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c4);
			if (c8 == c4) {
				pixel4 = interpolate<7, 1>(c5, c4);
				pixel7 = interpolate<1, 7>(c5, c4);
				pixel8 = interpolate<7, 1>(c5, c4);
			}
			break;

		case 0xF2:
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c6);
			if (c6 == c8) {
				pixel6 = interpolate<7, 1>(c5, c6);
				pixel8 = interpolate<7, 1>(c5, c6);
				pixel9 = interpolate<1, 7>(c5, c6);
			}
			break;

		case 0x3B:
			if (c4 == c2) {
				pixel1 = interpolate<1, 7>(c5, c2);
				pixel2 = interpolate<7, 1>(c5, c2);
				pixel4 = interpolate<7, 1>(c5, c2);
			}
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c2);
			break;

		case 0x79:
			if (c8 == c4) {
				pixel4 = interpolate<7, 1>(c5, c8);
				pixel7 = interpolate<1, 7>(c5, c8);
				pixel8 = interpolate<7, 1>(c5, c8);
			}
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c8);
			break;

		case 0x57:
			if (c2 == c6) {
				pixel2 = interpolate<7, 1>(c5, c6);
				pixel3 = interpolate<1, 7>(c5, c6);
				pixel6 = interpolate<7, 1>(c5, c6);
			}
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c6);
			break;

		case 0x4F:
			if (c4 == c2) {
				pixel1 = interpolate<1, 7>(c5, c4);
				pixel2 = interpolate<7, 1>(c5, c4);
				pixel4 = interpolate<7, 1>(c5, c4);
			}
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c4);
			break;

		case 0x7A:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c2);
			if (c8 == c4) {
				pixel4 = interpolate<7, 1>(c5, c8);
				pixel7 = interpolate<1, 7>(c5, c8);
				pixel8 = interpolate<7, 1>(c5, c8);
			}
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c8);
			break;

		case 0x5E:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) {
				pixel2 = interpolate<7, 1>(c5, c2);
				pixel3 = interpolate<1, 7>(c5, c2);
				pixel6 = interpolate<7, 1>(c5, c2);
			}
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c8);
			break;

		case 0xDA:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c2);
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) {
				pixel6 = interpolate<7, 1>(c5, c8);
				pixel8 = interpolate<7, 1>(c5, c8);
				pixel9 = interpolate<1, 7>(c5, c8);
			}
			break;

		case 0x5B:
			if (c4 == c2) {
				pixel1 = interpolate<1, 7>(c5, c2);
				pixel2 = interpolate<7, 1>(c5, c2);
				pixel4 = interpolate<7, 1>(c5, c2);
			}
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c2);
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c8);
			break;

		case 0xC9: case 0xCD:
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c4);
			break;

		case 0x2E: case 0xAE:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c2);
			break;

		case 0x93: case 0xB3:
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c2);
			break;

		case 0x74: case 0x75:
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c6);
			break;

		case 0x7E:
			if (c2 == c6) {
				pixel2 = interpolate<7, 1>(c5, c2);
				pixel3 = interpolate<1, 7>(c5, c2);
				pixel6 = interpolate<7, 1>(c5, c2);
			}
			if (c8 == c4) {
				pixel4 = interpolate<7, 1>(c5, c4);
				pixel7 = interpolate<1, 7>(c5, c4);
				pixel8 = interpolate<7, 1>(c5, c4);
			}
			break;

		case 0xDB:
			if (c4 == c2) {
				pixel1 = interpolate<1, 7>(c5, c2);
				pixel2 = interpolate<7, 1>(c5, c2);
				pixel4 = interpolate<7, 1>(c5, c2);
			}
			if (c6 == c8) {
				pixel6 = interpolate<7, 1>(c5, c6);
				pixel8 = interpolate<7, 1>(c5, c6);
				pixel9 = interpolate<1, 7>(c5, c6);
			}
			break;

		case 0xE9: case 0xED:
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c4);
			break;

		case 0x2F: case 0xAF:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c2);
			break;

		case 0x97: case 0xB7:
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c2);
			break;

		case 0xF4: case 0xF5:
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c6);
			break;

		case 0xFC:
			if (c8 == c4) {
				pixel4 = interpolate<7, 1>(c5, c8);
				pixel7 = interpolate<1, 7>(c5, c8);
			}
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c8);
			break;

		case 0xF9:
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) {
				pixel6 = interpolate<7, 1>(c5, c8);
				pixel9 = interpolate<1, 7>(c5, c8);
			}
			break;

		case 0xEB:
			if (c4 == c2) {
				pixel1 = interpolate<1, 7>(c5, c4);
				pixel2 = interpolate<7, 1>(c5, c4);
			}
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c4);
			break;

		case 0x6F:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c4);
			if (c8 == c4) {
				pixel7 = interpolate<1, 7>(c5, c4);
				pixel8 = interpolate<7, 1>(c5, c4);
			}
			break;

		case 0x3F:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) {
				pixel3 = interpolate<1, 7>(c5, c2);
				pixel6 = interpolate<7, 1>(c5, c2);
			}
			break;

		case 0x9F:
			if (c4 == c2) {
				pixel1 = interpolate<1, 7>(c5, c2);
				pixel4 = interpolate<7, 1>(c5, c2);
			}
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c2);
			break;

		case 0xD7:
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c6);
			if (c6 == c8) {
				pixel8 = interpolate<7, 1>(c5, c6);
				pixel9 = interpolate<1, 7>(c5, c6);
			}
			break;

		case 0xF6:
			if (c2 == c6) {
				pixel2 = interpolate<7, 1>(c5, c6);
				pixel3 = interpolate<1, 7>(c5, c6);
			}
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c6);
			break;

		case 0xFE:
			if (c2 == c6) {
				pixel2 = interpolate<7, 1>(c5, c2);
				pixel3 = interpolate<1, 7>(c5, c2);
			}
			if (c8 == c4) {
				pixel4 = interpolate<7, 1>(c5, c8);
				pixel7 = interpolate<1, 7>(c5, c8);
			}
			if (c6 == c8) {
				pixel6 = interpolate<7, 1>(c5, c8);
				pixel8 = interpolate<7, 1>(c5, c8);
				pixel9 = interpolate<1, 1>(c5, c8);
			}
			break;

		case 0xFD:
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c8);
			break;

		case 0xFB:
			if (c4 == c2) {
				pixel1 = interpolate<1, 7>(c5, c2);
				pixel2 = interpolate<7, 1>(c5, c2);
			}
			if (c8 == c4) {
				pixel4 = interpolate<7, 1>(c5, c8);
				pixel7 = interpolate<1, 1>(c5, c8);
				pixel8 = interpolate<7, 1>(c5, c8);
			}
			if (c6 == c8) {
				pixel6 = interpolate<7, 1>(c5, c8);
				pixel9 = interpolate<1, 7>(c5, c8);
			}
			break;

		case 0xEF:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c4);
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c4);
			break;

		case 0x7F:
			if (c4 == c2) {
				pixel1 = interpolate<1, 1>(c5, c2);
				pixel2 = interpolate<7, 1>(c5, c2);
				pixel4 = interpolate<7, 1>(c5, c2);
			}
			if (c2 == c6) {
				pixel3 = interpolate<1, 7>(c5, c2);
				pixel6 = interpolate<7, 1>(c5, c2);
			}
			if (c8 == c4) {
				pixel7 = interpolate<1, 7>(c5, c4);
				pixel8 = interpolate<7, 1>(c5, c4);
			}
			break;

		case 0xBF:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c2);
			break;

		case 0xDF:
			if (c4 == c2) {
				pixel1 = interpolate<1, 7>(c5, c2);
				pixel4 = interpolate<7, 1>(c5, c2);
			}
			if (c2 == c6) {
				pixel2 = interpolate<7, 1>(c5, c2);
				pixel3 = interpolate<1, 1>(c5, c2);
				pixel6 = interpolate<7, 1>(c5, c2);
			}
			if (c6 == c8) {
				pixel8 = interpolate<7, 1>(c5, c6);
				pixel9 = interpolate<1, 7>(c5, c6);
			}
			break;

		case 0xF7:
			if (c2 == c6)
				pixel3 = interpolate<1, 1>(c5, c6);
			if (c6 == c8)
				pixel9 = interpolate<1, 1>(c5, c6);
			break;

		case 0xFF:
			if (c4 == c2) pixel1 = interpolate<1, 1>(c5, c2);
			if (c2 == c6) pixel3 = interpolate<1, 1>(c5, c2);
			if (c8 == c4) pixel7 = interpolate<1, 1>(c5, c8);
			if (c6 == c8) pixel9 = interpolate<1, 1>(c5, c8);
			break;
		}

		pset(out2 + 0, pixel7);
		pset(out2 + 1, pixel8);
		pset(out2 + 2, pixel9);
		pset(out1 + 0, pixel4);
		pset(out1 + 1, c5    );
		pset(out1 + 2, pixel6);
		pset(out0 + 0, pixel1);
		pset(out0 + 1, pixel2);
		pset(out0 + 2, pixel3);
		out0 += 3; out1 += 3; out2 += 3;
	}
}

template <class Pixel>
void HQ3xLiteScaler<Pixel>::scaleBlank(Pixel color, SDL_Surface* dst,
                                   unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 3 * startY + (lower ? 1 : 0);
	unsigned y2 = 3 * endY   + (lower ? 1 : 0);
	y2 = std::min(720u, y2);
	for (unsigned y = y1; y < y2; ++y) {
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		fillLine(dstLine, color, dst->w);
	}
}

template <class Pixel>
void HQ3xLiteScaler<Pixel>::scale256(FrameSource& src, SDL_Surface* dst,
                                 unsigned startY, unsigned endY, bool lower)
{
	unsigned dstY = 3 * startY + (lower ? 1 : 0);
	unsigned prevY = startY;
	while (startY < endY) {
		Pixel* dummy = 0;
		const Pixel* srcPrev = src.getLinePtr(prevY,  dummy);
		const Pixel* srcCurr = src.getLinePtr(startY, dummy);
		const Pixel* srcNext = src.getLinePtr(min(startY + 1, endY - 1), dummy);
		Pixel* dst0 = Scaler<Pixel>::linePtr(dst, dstY + 0);
		Pixel* dst1 = Scaler<Pixel>::linePtr(dst, dstY + 1);
		Pixel* dst2 = (dstY != (720 - 2))
		            ? Scaler<Pixel>::linePtr(dst, dstY + 2)
		            : dst1;
		scaleLine256(srcPrev, srcCurr, srcNext, dst0, dst1, dst2);
		prevY = startY;
		++startY;
		dstY += 3;
	}
}

template <class Pixel>
void HQ3xLiteScaler<Pixel>::scale512(FrameSource& src, SDL_Surface* dst,
                                 unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 3 * startY + (lower ? 1 : 0);
	unsigned y2 = 3 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 3, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_2on3(srcLine, dstLine0, 640); // TODO
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine0, dstLine1, 960);
		if (y == (720 - 2)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		copyLine(dstLine0, dstLine2, 960);
	}
}


// Force template instantiation.
template class HQ3xLiteScaler<word>;
template class HQ3xLiteScaler<unsigned>;

} // namespace openmsx
