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
                  
                unsigned pixel1, pixel2, pixel3, pixel4;
		switch (pattern) {
		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0c: case 0x0d:
		case 0x10: case 0x11: case 0x14: case 0x15:
		case 0x18: case 0x19: case 0x1c: case 0x1d:
		case 0x20: case 0x21: case 0x22: case 0x23:
		case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2c: case 0x2d:
		case 0x30: case 0x31: case 0x34: case 0x35:
		case 0x38: case 0x39: case 0x3c: case 0x3d:
		case 0x40: case 0x41: case 0x42: case 0x43:
		case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x60: case 0x61: case 0x62: case 0x63:
		case 0x64: case 0x65: case 0x66: case 0x67:
		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8c: case 0x8d:
		case 0x90: case 0x91: case 0x94: case 0x95:
		case 0x98: case 0x99: case 0x9c: case 0x9d:
		case 0xa0: case 0xa1: case 0xa2: case 0xa3:
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xac: case 0xad:
		case 0xb0: case 0xb1: case 0xb4: case 0xb5:
		case 0xb8: case 0xb9: case 0xbc: case 0xbd:
		case 0xc0: case 0xc1: case 0xc2: case 0xc3:
		case 0xc4: case 0xc5: case 0xc6: case 0xc7:
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
		case 0xe4: case 0xe5: case 0xe6: case 0xe7:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x12: case 0x16: case 0x1e: case 0x32:
                case 0x36: case 0x3e: case 0x56: case 0x76:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp11(c5, c2);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x50: case 0x51: case 0xd0: case 0xd1:
		case 0xd2: case 0xd3: case 0xd8: case 0xd9:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp11(c5, c6);
			break;

		case 0x48: case 0x4c: case 0x68: case 0x6a:
                case 0x6c: case 0x6e: case 0x78: case 0x7c:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			pixel4 = c5;
			break;


		case 0x0a: case 0x0b: case 0x1b: case 0x4b:
                case 0x8a: case 0x8b: case 0x9b: case 0xcb:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel2 = c5;
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x13: case 0x17: case 0x33: case 0x37:
		case 0x77:
			pixel1 = (c2 != c6) ? c5 : interp521(c5, c2, c4);
			pixel2 = (c2 != c6) ? c5 : interp13(c5, c2);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x92: case 0x96: case 0xb2: case 0xb6:
		case 0xbe:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp13(c5, c2);
			pixel3 = c5;
			pixel4 = (c2 != c6) ? c5 : interp521(c5, c6, c8);
			break;

		case 0x54: case 0x55: case 0xd4: case 0xd5:
		case 0xdd:
			pixel1 = c5;
			pixel2 = (c6 != c8) ? c5 : interp521(c5, c6, c2);
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp13(c5, c6);
			break;

		case 0x70: case 0x71: case 0xf0: case 0xf1:
		case 0xf3:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c6 != c8) ? c5 : interp521(c5, c8, c4);
			pixel4 = (c6 != c8) ? c5 : interp13(c5, c6);
			break;

		case 0xc8: case 0xcc: case 0xe8: case 0xec:
		case 0xee:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp13(c5, c4);
			pixel4 = (c8 != c4) ? c5 : interp521(c5, c8, c6);
			break;

		case 0x49: case 0x4d: case 0x69: case 0x6d:
		case 0x7d:
			pixel1 = (c8 != c4) ? c5 : interp521(c5, c4, c2);
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp13(c5, c4);
			pixel4 = c5;
			break;

		case 0x2a: case 0x2b: case 0xaa: case 0xab:
		case 0xbb:
			pixel1 = (c4 != c2) ? c5 : interp13(c5, c2);
			pixel2 = c5;
			pixel3 = (c4 != c2) ? c5 : interp521(c5, c4, c8);
			pixel4 = c5;
			break;

		case 0x0e: case 0x0f: case 0x8e: case 0x8f:
		case 0xcf:
			pixel1 = (c4 != c2) ? c5 : interp13(c5, c2);
			pixel2 = (c4 != c2) ? c5 : interp521(c5, c2, c6);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x1a: case 0x1f: case 0x5f:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp11(c5, c2);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x52: case 0xd6: case 0xde:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp11(c5, c6);
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp11(c5, c6);
			break;

		case 0x58: case 0xf8: case 0xfa:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp11(c5, c8);
			break;

		case 0x4a: case 0x6b: case 0x7b:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c4);
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			pixel4 = c5;
			break;

		case 0x3a: case 0x9a: case 0xba:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp31(c5, c2);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x53: case 0x72: case 0x73:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp31(c5, c6);
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp31(c5, c6);
			break;

		case 0x59: case 0x5c: case 0x5d:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp31(c5, c8);
			break;

		case 0x4e: case 0xca: case 0xce:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c4);
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c4);
			pixel4 = c5;
			break;

		case 0x5a:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp31(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp31(c5, c8);
			break;

		case 0xdc:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp11(c5, c8);
			break;

		case 0x9e:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp11(c5, c2);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0xea:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c4);
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			pixel4 = c5;
			break;

		case 0xf2:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp31(c5, c6);
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp11(c5, c6);
			break;

		case 0x3b:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp31(c5, c2);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x79:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp31(c5, c8);
			break;

		case 0x57:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp11(c5, c6);
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c9 : interp31(c5, c6);
			break;

		case 0x4f:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c4);
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c4);
			pixel4 = c5;
			break;

		case 0x7a:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp31(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp31(c5, c8);
			break;

		case 0x5e:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp11(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp31(c5, c8);
			break;

		case 0xda:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp31(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp11(c5, c8);
			break;

		case 0x5b:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp31(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp31(c5, c8);
			break;

		case 0xc9: case 0xcd:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c4);
			pixel4 = c5;
			break;

		case 0x2e: case 0xae:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel2 = c5;
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x93: case 0xb3:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp31(c5, c2);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x74: case 0x75:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp31(c5, c6);
			break;

		case 0x7e:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp11(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			pixel4 = c5;
			break;

		case 0xdb:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel2 = c5;
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp11(c5, c6);
			break;

		case 0xe9: case 0xed:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c4);
			pixel4 = c5;
			break;

		case 0x2f: case 0xaf:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c2);
			pixel2 = c5;
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x97: case 0xb7:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp71(c5, c2);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0xf4: case 0xf5:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp71(c5, c6);
			break;

		case 0xfc:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp71(c5, c8);
			break;

		case 0xf9:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp11(c5, c8);
			break;

		case 0xeb:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c4);
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c4);
			pixel4 = c5;
			break;

		case 0x6f:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c4);
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			pixel4 = c5;
			break;

		case 0x3f:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp11(c5, c2);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0x9f:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp71(c5, c2);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0xd7:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp71(c5, c6);
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp11(c5, c6);
			break;

		case 0xf6:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp11(c5, c6);
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp71(c5, c6);
			break;

		case 0xfe:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp11(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp71(c5, c8);
			break;

		case 0xfd:
			pixel1 = c5;
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp71(c5, c8);
			break;

		case 0xfb:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp11(c5, c8);
			break;

		case 0xef:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c4);
			pixel2 = c5;
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c4);
			pixel4 = c5;
			break;

		case 0x7f:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp11(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			pixel4 = c5;
			break;

		case 0xbf:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp71(c5, c2);
			pixel3 = c5;
			pixel4 = c5;
			break;

		case 0xdf:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp71(c5, c2);
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp11(c5, c6);
			break;

		case 0xf7:
			pixel1 = c5;
			pixel2 = (c2 != c6) ? c5 : interp71(c5, c6);
			pixel3 = c5;
			pixel4 = (c6 != c8) ? c5 : interp71(c5, c6);
			break;

		case 0xff:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c2);
			pixel2 = (c2 != c6) ? c5 : interp71(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c8);
			pixel4 = (c6 != c8) ? c5 : interp71(c5, c8);
			break;

		default:
			assert(false);
			pixel1 = pixel2 = pixel3 = pixel4 = 0; // avoid warning
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
                  
                unsigned pixel1, pixel3;
		switch (pattern) {
		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0c: case 0x0d:
		case 0x10: case 0x11: case 0x14: case 0x15:
		case 0x18: case 0x19: case 0x1c: case 0x1d:
		case 0x20: case 0x21: case 0x22: case 0x23:
		case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2c: case 0x2d:
		case 0x30: case 0x31: case 0x34: case 0x35:
		case 0x38: case 0x39: case 0x3c: case 0x3d:
		case 0x40: case 0x41: case 0x42: case 0x43:
		case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x60: case 0x61: case 0x62: case 0x63:
		case 0x64: case 0x65: case 0x66: case 0x67:
		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8c: case 0x8d:
		case 0x90: case 0x91: case 0x94: case 0x95:
		case 0x98: case 0x99: case 0x9c: case 0x9d:
		case 0xa0: case 0xa1: case 0xa2: case 0xa3:
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xa8: case 0xa9: case 0xac: case 0xad:
		case 0xb0: case 0xb1: case 0xb4: case 0xb5:
		case 0xb8: case 0xb9: case 0xbc: case 0xbd:
		case 0xc0: case 0xc1: case 0xc2: case 0xc3:
		case 0xc4: case 0xc5: case 0xc6: case 0xc7:
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
		case 0xe4: case 0xe5: case 0xe6: case 0xe7:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0x12: case 0x16: case 0x1e: case 0x32:
                case 0x36: case 0x3e: case 0x56: case 0x76:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0x50: case 0x51: case 0xd0: case 0xd1:
		case 0xd2: case 0xd3: case 0xd8: case 0xd9:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0x48: case 0x4c: case 0x68: case 0x6a:
                case 0x6c: case 0x6e: case 0x78: case 0x7c:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			break;


		case 0x0a: case 0x0b: case 0x1b: case 0x4b:
                case 0x8a: case 0x8b: case 0x9b: case 0xcb:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel3 = c5;
			break;

		case 0x13: case 0x17: case 0x33: case 0x37:
		case 0x77:
			pixel1 = (c2 != c6) ? c5 : interp521(c5, c2, c4);
			pixel3 = c5;
			break;

		case 0x92: case 0x96: case 0xb2: case 0xb6:
		case 0xbe:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0x54: case 0x55: case 0xd4: case 0xd5:
		case 0xdd:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0x70: case 0x71: case 0xf0: case 0xf1:
		case 0xf3:
			pixel1 = c5;
			pixel3 = (c6 != c8) ? c5 : interp521(c5, c8, c4);
			break;

		case 0xc8: case 0xcc: case 0xe8: case 0xec:
		case 0xee:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp13(c5, c4);
			break;

		case 0x49: case 0x4d: case 0x69: case 0x6d:
		case 0x7d:
			pixel1 = (c8 != c4) ? c5 : interp521(c5, c4, c2);
			pixel3 = (c8 != c4) ? c5 : interp13(c5, c4);
			break;

		case 0x2a: case 0x2b: case 0xaa: case 0xab:
		case 0xbb:
			pixel1 = (c4 != c2) ? c5 : interp13(c5, c2);
			pixel3 = (c4 != c2) ? c5 : interp521(c5, c4, c8);
			break;

		case 0x0e: case 0x0f: case 0x8e: case 0x8f:
		case 0xcf:
			pixel1 = (c4 != c2) ? c5 : interp13(c5, c2);
			pixel3 = c5;
			break;

		case 0x1a: case 0x1f: case 0x5f:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel3 = c5;
			break;

		case 0x52: case 0xd6: case 0xde:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0x58: case 0xf8: case 0xfa:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c8);
			break;

		case 0x4a: case 0x6b: case 0x7b:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c4);
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			break;

		case 0x3a: case 0x9a: case 0xba:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel3 = c5;
			break;

		case 0x53: case 0x72: case 0x73:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0x59: case 0x5c: case 0x5d:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			break;

		case 0x4e: case 0xca: case 0xce:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c4);
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c4);
			break;

		case 0x5a:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			break;

		case 0xdc:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			break;

		case 0x9e:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel3 = c5;
			break;

		case 0xea:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c4);
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			break;

		case 0xf2:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0x3b:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel3 = c5;
			break;

		case 0x79:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c8);
			break;

		case 0x57:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0x4f:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c4);
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c4);
			break;

		case 0x7a:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c8);
			break;

		case 0x5e:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			break;

		case 0xda:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			break;

		case 0x5b:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c8);
			break;

		case 0xc9: case 0xcd:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp31(c5, c4);
			break;

		case 0x2e: case 0xae:
			pixel1 = (c4 != c2) ? c5 : interp31(c5, c2);
			pixel3 = c5;
			break;

		case 0x93: case 0xb3:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0x74: case 0x75:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0x7e:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			break;

		case 0xdb:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel3 = c5;
			break;

		case 0xe9: case 0xed:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c4);
			break;

		case 0x2f: case 0xaf:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c2);
			pixel3 = c5;
			break;

		case 0x97: case 0xb7:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0xf4: case 0xf5:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0xfc:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c8);
			break;

		case 0xf9:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c8);
			break;

		case 0xeb:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c4);
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c4);
			break;

		case 0x6f:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c4);
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			break;

		case 0x3f:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c2);
			pixel3 = c5;
			break;

		case 0x9f:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel3 = c5;
			break;

		case 0xd7:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0xf6:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0xfe:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c8);
			break;

		case 0xfd:
			pixel1 = c5;
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c8);
			break;

		case 0xfb:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c8);
			break;

		case 0xef:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c4);
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c4);
			break;

		case 0x7f:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp11(c5, c4);
			break;

		case 0xbf:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c2);
			pixel3 = c5;
			break;

		case 0xdf:
			pixel1 = (c4 != c2) ? c5 : interp11(c5, c2);
			pixel3 = c5;
			break;

		case 0xf7:
			pixel1 = c5;
			pixel3 = c5;
			break;

		case 0xff:
			pixel1 = (c4 != c2) ? c5 : interp71(c5, c2);
			pixel3 = (c8 != c4) ? c5 : interp71(c5, c8);
			break;

		default:
			assert(false);
			pixel1 = pixel3 = 0; // avoid warning
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
