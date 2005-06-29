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
#include <cassert>

namespace openmsx {

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
        return (c1 * w1 + c2 * w2 + c3 * w3) / wsum;
}

static inline unsigned interp11(unsigned c1, unsigned c2)
{
	return interpolate<1, 1>(c1, c2);
}

static inline unsigned interp13(unsigned c1, unsigned c2)
{
	return interpolate<1, 3>(c1, c2);
}

static inline unsigned interp31(unsigned c1, unsigned c2)
{
	return interpolate<3, 1>(c1, c2);
}

static inline unsigned interp71(unsigned c1, unsigned c2)
{
	return interpolate<7, 1>(c1, c2);
}

static inline unsigned interp521(unsigned c1, unsigned c2, unsigned c3)
{
	return interpolate<5, 2, 1>(c1, c2, c3);
}

template <class Pixel>
static void scaleLine256(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY,
	const int prev, const int next)
{
	const unsigned WIDTH = 320;

	const int srcPitch = src->pitch / sizeof(Pixel);
	const int dstPitch = dst->pitch / sizeof(Pixel);

	Pixel* in = (Pixel*)src->pixels + srcY * srcPitch;
	Pixel* out = (Pixel*)dst->pixels + dstY * dstPitch;

	unsigned c1, c2, c3, c4, c5, c6, c7, c8, c9;
	c1 = c2 = readPixel(in + prev);
	c4 = c5 = readPixel(in);
	c7 = c8 = readPixel(in + next);
	c3 = c6 = c9 = 0; // avoid warning
	in += 1;

	for (unsigned x = 0; x < WIDTH; ++x) {
		if (x != WIDTH - 1) {
			c3 = readPixel(in + prev);
			c6 = readPixel(in);
			c9 = readPixel(in + next);
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
			if (c2 == c6) pixel2 = interp11(c5, c2);
			break;

		case 0x50: case 0x51: case 0xd0: case 0xd1:
		case 0xd2: case 0xd3: case 0xd8: case 0xd9:
			if (c6 == c8) pixel4 = interp11(c5, c6);
			break;

		case 0x48: case 0x4c: case 0x68: case 0x6a:
                case 0x6c: case 0x6e: case 0x78: case 0x7c:
			if (c4 == c8) pixel3 = interp11(c5, c4);
			break;


		case 0x0a: case 0x0b: case 0x1b: case 0x4b:
                case 0x8a: case 0x8b: case 0x9b: case 0xcb:
			if (c2 == c4) pixel1 = interp11(c5, c2);
			break;

		case 0x13: case 0x17: case 0x33: case 0x37:
		case 0x77:
			if (c2 == c6) {
				pixel1 = interp521(c5, c2, c4);
				pixel2 = interp13(c5, c2);
			}
			break;

		case 0x92: case 0x96: case 0xb2: case 0xb6:
		case 0xbe:
			if (c2 == c6) {
				pixel2 = interp13(c5, c2);
				pixel4 = interp521(c5, c2, c8);
			}
			break;

		case 0x54: case 0x55: case 0xd4: case 0xd5:
		case 0xdd:
			if (c6 == c8) {
				pixel2 = interp521(c5, c6, c2);
				pixel4 = interp13(c5, c6);
			}
			break;

		case 0x70: case 0x71: case 0xf0: case 0xf1:
		case 0xf3:
			if (c6 == c8) {
				pixel3 = interp521(c5, c6, c4);
				pixel4 = interp13(c5, c6);
			}
			break;

		case 0xc8: case 0xcc: case 0xe8: case 0xec:
		case 0xee:
			if (c4 == c8) {
				pixel3 = interp13(c5, c4);
				pixel4 = interp521(c5, c4, c6);
			}
			break;

		case 0x49: case 0x4d: case 0x69: case 0x6d:
		case 0x7d:
			if (c4 == c8) {
				pixel1 = interp521(c5, c4, c2);
				pixel3 = interp13(c5, c4);
			}
			break;

		case 0x2a: case 0x2b: case 0xaa: case 0xab:
		case 0xbb:
			if (c2 == c4) {
				pixel1 = interp13(c5, c2);
				pixel3 = interp521(c5, c2, c8);
			}
			break;

		case 0x0e: case 0x0f: case 0x8e: case 0x8f:
		case 0xcf:
			if (c2 == c4) {
				pixel1 = interp13(c5, c2);
				pixel2 = interp521(c5, c2, c6);
			}
			break;

		case 0x1a: case 0x1f: case 0x5f: {
			unsigned t = interp11(c5, c2);
			if (c2 == c4) pixel1 = t;
			if (c2 == c6) pixel2 = t;
			break;
		}
		case 0x52: case 0xd6: case 0xde: {
			unsigned t = interp11(c5, c6);
			if (c2 == c6) pixel2 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0x58: case 0xf8: case 0xfa: {
			unsigned t = interp11(c5, c8);
			if (c4 == c8) pixel3 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0x4a: case 0x6b: case 0x7b: {
			unsigned t = interp11(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		case 0x3a: case 0x9a: case 0xba: {
			unsigned t = interp31(c5, c2);
			if (c2 == c4) pixel1 = t;
			if (c2 == c6) pixel2 = t;
			break;
		} 
		case 0x53: case 0x72: case 0x73: {
			unsigned t = interp31(c5, c6);
			if (c2 == c6) pixel2 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0x59: case 0x5c: case 0x5d: {
			unsigned t = interp31(c5, c8);
			if (c4 == c8) pixel3 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0x4e: case 0xca: case 0xce: {
			unsigned t = interp31(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		case 0x5a: {
			unsigned t1 = interp31(c5, c2);
			if (c2 == c4) pixel1 = t1;
			if (c2 == c6) pixel2 = t1;
			unsigned t2 = interp31(c5, c8);
			if (c4 == c8) pixel3 = t2;
			if (c6 == c8) pixel4 = t1;
			break;
		}
		case 0xdc:
			if (c4 == c8) pixel3 = interp31(c5, c8);
			if (c6 == c8) pixel4 = interp11(c5, c8);
			break;

		case 0x9e:
			if (c2 == c4) pixel1 = interp31(c5, c2);
			if (c2 == c6) pixel2 = interp11(c5, c2);
			break;

		case 0xea:
			if (c2 == c4) pixel1 = interp31(c5, c4);
			if (c4 == c8) pixel3 = interp11(c5, c4);
			break;

		case 0xf2:
			if (c2 == c6) pixel2 = interp31(c5, c6);
			if (c6 == c8) pixel4 = interp11(c5, c6);
			break;

		case 0x3b:
			if (c2 == c4) pixel1 = interp11(c5, c2);
			if (c2 == c6) pixel2 = interp31(c5, c2);
			break;

		case 0x79:
			if (c4 == c8) pixel3 = interp11(c5, c8);
			if (c6 == c8) pixel4 = interp31(c5, c8);
			break;

		case 0x57:
			if (c2 == c6) pixel2 = interp11(c5, c6);
			if (c6 == c8) pixel4 = interp31(c5, c6);
			break;

		case 0x4f:
			if (c2 == c4) pixel1 = interp11(c5, c4);
			if (c4 == c8) pixel3 = interp31(c5, c4);
			break;

		case 0x7a: {
			unsigned t = interp31(c5, c2);
			if (c2 == c4) pixel1 = t;
			if (c2 == c6) pixel2 = t;
			if (c4 == c8) pixel3 = interp11(c5, c8);
			if (c6 == c8) pixel4 = interp31(c5, c8);
			break;
		}
		case 0x5e: {
			if (c2 == c4) pixel1 = interp31(c5, c2);
			if (c2 == c6) pixel2 = interp11(c5, c2);
			unsigned t = interp31(c5, c8);
			if (c4 == c8) pixel3 = t;
			if (c6 == c8) pixel4 = t;
			break;
		} 
		case 0xda: {
			unsigned t = interp31(c5, c2);
			if (c2 == c4) pixel1 = t;
			if (c2 == c6) pixel2 = t;
			if (c4 == c8) pixel3 = interp31(c5, c8);
			if (c6 == c8) pixel4 = interp11(c5, c8);
			break;
		}
		case 0x5b: {
			if (c2 == c4) pixel1 = interp11(c5, c2);
			if (c2 == c6) pixel2 = interp31(c5, c2);
			unsigned t = interp31(c5, c8);
			if (c4 == c8) pixel3 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0xc9: case 0xcd:
			if (c4 == c8) pixel3 = interp31(c5, c4);
			break;

		case 0x2e: case 0xae:
			if (c2 == c4) pixel1 = interp31(c5, c2);
			break;

		case 0x93: case 0xb3:
			if (c2 == c6) pixel2 = interp31(c5, c2);
			break;

		case 0x74: case 0x75:
			if (c6 == c8) pixel4 = interp31(c5, c6);
			break;

		case 0x7e:
			if (c2 == c6) pixel2 = interp11(c5, c2);
			if (c4 == c8) pixel3 = interp11(c5, c4);
			break;

		case 0xdb:
			if (c2 == c4) pixel1 = interp11(c5, c2);
			if (c6 == c8) pixel4 = interp11(c5, c6);
			break;

		case 0xe9: case 0xed:
			if (c4 == c8) pixel3 = interp71(c5, c4);
			break;

		case 0x2f: case 0xaf:
			if (c2 == c4) pixel1 = interp71(c5, c2);
			break;

		case 0x97: case 0xb7:
			if (c2 == c6) pixel2 = interp71(c5, c2);
			break;

		case 0xf4: case 0xf5:
			if (c6 == c8) pixel4 = interp71(c5, c6);
			break;

		case 0xfc:
			if (c4 == c8) pixel3 = interp11(c5, c8);
			if (c6 == c8) pixel4 = interp71(c5, c8);
			break;

		case 0xf9:
			if (c4 == c8) pixel3 = interp71(c5, c8);
			if (c6 == c8) pixel4 = interp11(c5, c8);
			break;

		case 0xeb:
			if (c2 == c4) pixel1 = interp11(c5, c4);
			if (c4 == c8) pixel3 = interp71(c5, c4);
			break;

		case 0x6f:
			if (c2 == c4) pixel1 = interp71(c5, c4);
			if (c4 == c8) pixel3 = interp11(c5, c4);
			break;

		case 0x3f:
			if (c2 == c4) pixel1 = interp71(c5, c2);
			if (c2 == c6) pixel2 = interp11(c5, c2);
			break;

		case 0x9f:
			if (c2 == c4) pixel1 = interp11(c5, c2);
			if (c2 == c6) pixel2 = interp71(c5, c2);
			break;

		case 0xd7:
			if (c2 == c6) pixel2 = interp71(c5, c6);
			if (c6 == c8) pixel4 = interp11(c5, c6);
			break;

		case 0xf6:
			if (c2 == c6) pixel2 = interp11(c5, c6);
			if (c6 == c8) pixel4 = interp71(c5, c6);
			break;

		case 0xfe:
			if (c2 == c6) pixel2 = interp11(c5, c2);
			if (c4 == c8) pixel3 = interp11(c5, c8);
			if (c6 == c8) pixel4 = interp71(c5, c8);
			break;

		case 0xfd: {
			unsigned t = interp71(c5, c8);
			if (c4 == c8) pixel3 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0xfb:
			if (c2 == c4) pixel1 = interp11(c5, c2);
			if (c4 == c8) pixel3 = interp71(c5, c8);
			if (c6 == c8) pixel4 = interp11(c5, c8);
			break;

		case 0xef: {
			unsigned t = interp71(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		case 0x7f:
			if (c2 == c4) pixel1 = interp71(c5, c2);
			if (c2 == c6) pixel2 = interp11(c5, c2);
			if (c4 == c8) pixel3 = interp11(c5, c4);
			break;

		case 0xbf: {
			unsigned t = interp71(c5, c2);
			if (c2 == c4) pixel1 = t;
			if (c2 == c6) pixel2 = t;
			break;
		}
		case 0xdf:
			if (c2 == c4) pixel1 = interp11(c5, c2);
			if (c2 == c6) pixel2 = interp71(c5, c2);
			if (c6 == c8) pixel4 = interp11(c5, c6);
			break;

		case 0xf7: {
			unsigned t = interp71(c5, c6);
			if (c2 == c6) pixel2 = t;
			if (c6 == c8) pixel4 = t;
			break;
		}
		case 0xff: {
			unsigned t1 = interp71(c5, c2);
			if (c2 == c4) pixel1 = t1;
			if (c2 == c6) pixel2 = t1;
			unsigned t2 = interp71(c5, c8);
			if (c4 == c8) pixel3 = t2;
			if (c6 == c8) pixel4 = t2;
			break;
		}
		}
		pset(out,                pixel1);
		pset(out + 1,            pixel2);
		pset(out + dstPitch,     pixel3);
		pset(out + dstPitch + 1, pixel4);

		c1 = c2; c2 = c3; c4 = c5; c5 = c6; c7 = c8; c8 = c9;

		in  += 1;
		out += 2;
	}
}

template <class Pixel>
static void scaleLine512(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY,
	const int prevLine, const int nextLine )
{
	const int width = 640; // TODO: Specify this in a clean way.
	const int srcPitch = src->pitch / sizeof(Pixel);
	const int dstPitch = dst->pitch / sizeof(Pixel);

	Pixel* pIn = (Pixel*)src->pixels + srcY * srcPitch;
	Pixel* pOut = (Pixel*)dst->pixels + dstY * dstPitch;

	unsigned c1, c2, c3, c4, c5, c6, c7, c8, c9;

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

	c2 = c3 = readPixel(pIn + prevLine);
	c5 = c6 = readPixel(pIn);
	c8 = c9 = readPixel(pIn + nextLine);

	for (int i = 0; i < width; i++) {
		c1 = c2; c4 = c5; c7 = c8;
		c2 = c3; c5 = c6; c8 = c9;

		pIn += 1;

		if (i < width - 1) {
			c3 = readPixel(pIn + prevLine);
			c6 = readPixel(pIn);
			c9 = readPixel(pIn + nextLine);
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
			if (c2 == c4) pixel1 = interp11(c5, c4);
			break;

		case 0x48: case 0x4c: case 0x58: case 0x68:
		case 0x6a: case 0x6c: case 0x6e: case 0x78:
		case 0x79: case 0x7c: case 0x7e: case 0xf8:
		case 0xfa: case 0xfc: case 0xfe:
			if (c4 == c8) pixel3 = interp11(c5, c4);
			break;

		case 0x13: case 0x17: case 0x33: case 0x37:
		case 0x77:
			if (c2 == c6) pixel1 = interp521(c5, c2, c4);
			break;

		case 0x70: case 0x71: case 0xf0: case 0xf1:
		case 0xf3:
			if (c6 == c8) pixel3 = interp521(c5, c8, c4);
			break;

		case 0xc8: case 0xcc: case 0xe8: case 0xec:
		case 0xee:
			if (c4 == c8) pixel3 = interp13(c5, c4);
			break;

		case 0x49: case 0x4d: case 0x69: case 0x6d:
		case 0x7d:
			if (c4 == c8) {
				pixel1 = interp521(c5, c4, c2);
				pixel3 = interp13(c5, c4);
			}
			break;

		case 0x2a: case 0x2b: case 0xaa: case 0xab:
		case 0xbb:
			if (c2 == c4) {
				pixel1 = interp13(c5, c4);
				pixel3 = interp521(c5, c4, c8);
			}
			break;

		case 0x0e: case 0x0f: case 0x8e: case 0x8f:
		case 0xcf:
			if (c2 == c4) pixel1 = interp13(c5, c4);
			break;

		case 0x4a: case 0x6b: case 0x7b: {
			unsigned t = interp11(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		case 0x2e: case 0x3a: case 0x9a: case 0x9e:
		case 0xae: case 0xba:
			if (c2 == c4) pixel1 = interp31(c5, c4);
			break;

		case 0x59: case 0x5c: case 0x5d: case 0xc9:
		case 0xcd: case 0xdc:
			if (c4 == c8) pixel3 = interp31(c5, c4);
			break;

		case 0x4e: case 0x5a: case 0x5e: case 0xca: 
		case 0xce: case 0xda: {
			unsigned t = interp31(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		case 0x7a: case 0xea:
			if (c2 == c4) pixel1 = interp31(c5, c4);
			if (c4 == c8) pixel3 = interp11(c5, c4);
			break;

		case 0x4f: case 0x5b:
			if (c2 == c4) pixel1 = interp11(c5, c4);
			if (c4 == c8) pixel3 = interp31(c5, c4);
			break;

		case 0x2f: case 0x3f: case 0xaf: case 0xbf:
			if (c2 == c4) pixel1 = interp71(c5, c4);
			break;

		case 0xe9: case 0xed: case 0xf9: case 0xfd:
			if (c4 == c8) pixel3 = interp71(c5, c4);
			break;

		case 0xeb: case 0xfb:
			if (c2 == c4) pixel1 = interp11(c5, c4);
			if (c4 == c8) pixel3 = interp71(c5, c4);
			break;

		case 0x6f: case 0x7f:
			if (c2 == c4) pixel1 = interp71(c5, c4);
			if (c4 == c8) pixel3 = interp11(c5, c4);
			break;

		case 0xef: case 0xff: {
			unsigned t = interp71(c5, c4);
			if (c2 == c4) pixel1 = t;
			if (c4 == c8) pixel3 = t;
			break;
		}
		}
		pset(pOut,            pixel1);
		pset(pOut + dstPitch, pixel3);

		++pOut;
	}
}

template <class Pixel>
void HQ2xLiteScaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	const int srcPitch = src->pitch / sizeof(Pixel);
	assert(srcY < endSrcY);
	scaleLine256<Pixel>(src, srcY, dst, dstY,
		0, srcY < endSrcY - 1 ? srcPitch : 0
		);
	srcY += 1;
	dstY += 2;
	while (srcY < endSrcY - 1) {
		scaleLine256<Pixel>(src, srcY, dst, dstY, -srcPitch, srcPitch);
		srcY += 1;
		dstY += 2;
	}
	if (srcY < endSrcY) {
		scaleLine256<Pixel>(src, srcY, dst, dstY, -srcPitch, 0);
	}
}

template <class Pixel>
void HQ2xLiteScaler<Pixel>::scale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	const int srcPitch = src->pitch / sizeof(Pixel);
	assert(srcY < endSrcY);
	scaleLine512<Pixel>(src, srcY, dst, dstY,
	             0, srcY < endSrcY - 1 ? srcPitch : 0);
	srcY += 1;
	dstY += 2;
	while (srcY < endSrcY - 1) {
		scaleLine512<Pixel>(src, srcY, dst, dstY, -srcPitch, srcPitch);
		srcY += 1;
		dstY += 2;
	}
	if (srcY < endSrcY) {
		scaleLine512<Pixel>(src, srcY, dst, dstY, -srcPitch, 0);
	}
}


// Force template instantiation.
template class HQ2xLiteScaler<word>;
template class HQ2xLiteScaler<unsigned>;

} // namespace openmsx
