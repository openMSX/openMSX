// $Id$

/*
Original code: Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
openMSX adaptation by Maarten ter Huurne

License: LGPL

Visit the HiEnd3D site for info:
  http://www.hiend3d.com/hq2x.html
*/

#include "HQ2xScaler.hh"
#include <cassert>


namespace openmsx {

// Force template instantiation.
template class HQ2xScaler<word>;
template class HQ2xScaler<unsigned int>;

template <class Pixel>
inline Pix32 HQ2xScaler<Pixel>::readPixel(Pixel *pIn) {
	// TODO: Use surface info instead.
	Pixel p = *pIn;
	if (typeid(Pixel) == typeid(word)) {
		return((p << 8) & 0xF80000)
			| ((p << 5) & 0x00F800) // 0x00FC00
			| ((p << 3) & 0x0000F8);
	} else {
		//return p;
		//return p & 0xFCFCFC;
		//return p & 0xF8FCF8;
		return p & 0xF8F8F8;
	}
}

template <class Pixel>
inline void HQ2xScaler<Pixel>::pset(Pixel* pOut, Pix32 colour)
{
	// TODO: Use surface info instead.
	if (typeid(Pixel) == typeid(word)) {
		*pOut = (
			  ((colour >> 8) & 0xF800)
			| ((colour >> 5) & 0x07E0)
			| ((colour >> 3) & 0x001F)
			);
	} else {
		*pOut = (colour & 0xF8F8F8) | ((colour & 0xE0E0E0) >> 5);
	}
}

template <class Pixel>
inline bool HQ2xScaler<Pixel>::edge(Pix32 c1, Pix32 c2)
{
	if (c1 == c2) return false;

	int r1 = c1 >> 16;
	int g1 = (c1 >> 8) & 0xFF;
	int b1 = c1 & 0xFF;
	int Y1 = r1 + g1 + b1;

	int r2 = c2 >> 16;
	int g2 = (c2 >> 8) & 0xFF;
	int b2 = c2 & 0xFF;
	int Y2 = r2 + g2 + b2;

	int dY = Y1 - Y2;
	if (dY < -0xC0 || dY > 0xC0) return true;

	//int u1 = r1 - b1;
	//int u2 = r2 - b2;
	//if (abs(u1 - u2) > 0x1C) return true;
	int du = r1 - r2 + b2 - b1;
	if (du < -0x1C || du > 0x1C) return true;

	//int v1 = 3 * g1 - Y1;
	//int v2 = 3 * g2 - Y2;
	//if (abs(v1 - v2) > 0x30) return true;
	int dv = 3 * (g1 - g2) - dY;
	if (dv < -0x30 || dv > 0x30) return true;

	return false;
}

template <int w1, int w2>
inline Pix32 interpolate(Pix32 c1, Pix32 c2) {
	return (c1 * w1 + c2 * w2) >> 2;
}

template <int w1, int w2, int w3>
inline Pix32 interpolate(Pix32 c1, Pix32 c2, Pix32 c3) {
	enum { wsum = w1 + w2 + w3 };
	if (wsum <= 8) {
		// Because the lower 3 bits of each colour component (R,G,B) are
		// zeroed out, we can operate on a single integer as if it is
		// a vector.
		return (c1 * w1 + c2 * w2 + c3 * w3) / wsum;
	} else {
		return
			(
				(
					(
						(c1 & 0x00FF00) * w1 +
						(c2 & 0x00FF00) * w2 +
						(c3 & 0x00FF00) * w3
					) & (0x00FF00 * wsum)
				) | (
					(
						(c1 & 0xFF00FF) * w1 +
						(c2 & 0xFF00FF) * w2 +
						(c3 & 0xFF00FF) * w3
					) & (0xFF00FF * wsum)
				)
			) / wsum;
	}
}

inline Pix32 Interp1(Pix32 c1, Pix32 c2)
{
	return interpolate<3, 1>(c1, c2);
}

inline Pix32 Interp2(Pix32 c1, Pix32 c2, Pix32 c3)
{
	return interpolate<2, 1, 1>(c1, c2, c3);
}

inline Pix32 Interp6(Pix32 c1, Pix32 c2, Pix32 c3)
{
	return interpolate<5, 2, 1>(c1, c2, c3);
}

inline Pix32 Interp7(Pix32 c1, Pix32 c2, Pix32 c3)
{
	return interpolate<6, 1, 1>(c1, c2, c3);
}

inline Pix32 Interp9(Pix32 c1, Pix32 c2, Pix32 c3)
{
	return interpolate<2, 3, 3>(c1, c2, c3);
}

inline Pix32 Interp10(Pix32 c1, Pix32 c2, Pix32 c3)
{
	return interpolate<14, 1, 1>(c1, c2, c3);
}

template <class Pixel>
HQ2xScaler<Pixel>::HQ2xScaler() {
}

template <class Pixel>
void HQ2xScaler<Pixel>::scaleLine256(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY,
	const int prevLine, const int nextLine )
{
	const int width = 320; // TODO: Specify this in a clean way.
	const int srcPitch = src->pitch / sizeof(Pixel);
	const int dstPitch = dst->pitch / sizeof(Pixel);

	Pixel* pIn = (Pixel*)src->pixels + srcY * srcPitch;
	Pixel* pOut = (Pixel*)dst->pixels + dstY * dstPitch;

	Pix32 c1, c2, c3, c4, c5, c6, c7, c8, c9;

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

	c1 = c2 = readPixel(pIn + prevLine);
	c4 = c5 = readPixel(pIn);
	c7 = c8 = readPixel(pIn + nextLine);
	c3 = c6 = c9 = 0; // avoid warning
	pIn += 1;

	for (int i = 0; i < width; i++) {

		if (i < width - 1) {
			c3 = readPixel(pIn + prevLine);
			c6 = readPixel(pIn);
			c9 = readPixel(pIn + nextLine);
		}

		byte pattern = 0;
		if (edge(c5, c1)) pattern |= 0x01;
		if (edge(c5, c2)) pattern |= 0x02;
		if (edge(c5, c3)) pattern |= 0x04;
		if (edge(c5, c4)) pattern |= 0x08;
		if (edge(c5, c6)) pattern |= 0x10;
		if (edge(c5, c7)) pattern |= 0x20;
		if (edge(c5, c8)) pattern |= 0x40;
		if (edge(c5, c9)) pattern |= 0x80;

		Pix32 pixel = 0; // avoid warning

		switch (pattern) {
		case 0xe:
		case 0x2a:
		case 0x8e:
		case 0xaa:
		{
			if (edge(c4, c2)) {
				pixel = Interp1(c5, c1);
			} else {
				pixel = Interp9(c5, c4, c2);
			}
			break;
		}
		case 0x8:
		case 0xc:
		case 0x18:
		case 0x1c:
		case 0x28:
		case 0x2c:
		case 0x38:
		case 0x3c:
		case 0x48:
		case 0x4c:
		case 0x58:
		case 0x5c:
		case 0x68:
		case 0x6c:
		case 0x78:
		case 0x7c:
		case 0x88:
		case 0x8c:
		case 0x98:
		case 0x9c:
		case 0xa8:
		case 0xac:
		case 0xb8:
		case 0xbc:
		case 0xc8:
		case 0xcc:
		case 0xd8:
		case 0xdc:
		case 0xe8:
		case 0xec:
		case 0xf8:
		case 0xfc:
		{
			pixel = Interp2(c5, c1, c2);
			break;
		}
		case 0xb:
		case 0x1a:
		case 0x1b:
		case 0x1f:
		case 0x3b:
		case 0x4a:
		case 0x4b:
		case 0x4f:
		case 0x5b:
		case 0x5f:
		case 0x6b:
		case 0x7b:
		case 0x8b:
		case 0x9b:
		case 0x9f:
		case 0xcb:
		case 0xdb:
		case 0xdf:
		case 0xeb:
		case 0xfb:
		{
			if (edge(c4, c2)) {
				pixel = c5;
			} else {
				pixel = Interp2(c5, c4, c2);
			}
			break;
		}
		case 0x2e:
		case 0x3a:
		case 0x4e:
		case 0x5a:
		case 0x5e:
		case 0x7a:
		case 0x9a:
		case 0x9e:
		case 0xae:
		case 0xba:
		case 0xca:
		case 0xce:
		case 0xda:
		case 0xea:
		{
			if (edge(c4, c2)) {
				pixel = Interp1(c5, c1);
			} else {
				pixel = Interp7(c5, c4, c2);
			}
			break;
		}
		case 0xf:
		case 0x2b:
		case 0x8f:
		case 0xab:
		case 0xbb:
		case 0xcf:
		{
			if (edge(c4, c2)) {
				pixel = c5;
			} else {
				pixel = Interp9(c5, c4, c2);
			}
			break;
		}
		case 0x3:
		case 0x7:
		case 0x23:
		case 0x27:
		case 0x43:
		case 0x47:
		case 0x53:
		case 0x57:
		case 0x63:
		case 0x67:
		case 0x73:
		case 0x83:
		case 0x87:
		case 0x93:
		case 0x97:
		case 0xa3:
		case 0xa7:
		case 0xb3:
		case 0xb7:
		case 0xc3:
		case 0xc7:
		case 0xd3:
		case 0xd7:
		case 0xe3:
		case 0xe7:
		case 0xf3:
		case 0xf7:
		{
			pixel = Interp1(c5, c4);
			break;
		}
		case 0x1e:
		case 0x3e:
		case 0x6a:
		case 0x6e:
		case 0x7e:
		case 0xbe:
		case 0xde:
		case 0xee:
		case 0xfa:
		case 0xfe:
		{
			pixel = Interp1(c5, c1);
			break;
		}
		case 0x49:
		case 0x4d:
		case 0x69:
		case 0x6d:
		case 0x7d:
		{
			if (edge(c8, c4)) {
				pixel = Interp1(c5, c2);
			} else {
				pixel = Interp6(c5, c4, c2);
			}
			break;
		}
		case 0x0:
		case 0x1:
		case 0x4:
		case 0x5:
		case 0x10:
		case 0x11:
		case 0x14:
		case 0x15:
		case 0x20:
		case 0x21:
		case 0x24:
		case 0x25:
		case 0x30:
		case 0x31:
		case 0x34:
		case 0x35:
		case 0x40:
		case 0x41:
		case 0x44:
		case 0x45:
		case 0x50:
		case 0x51:
		case 0x54:
		case 0x55:
		case 0x60:
		case 0x61:
		case 0x64:
		case 0x65:
		case 0x70:
		case 0x71:
		case 0x74:
		case 0x75:
		case 0x80:
		case 0x81:
		case 0x84:
		case 0x85:
		case 0x90:
		case 0x91:
		case 0x94:
		case 0x95:
		case 0xa0:
		case 0xa1:
		case 0xa4:
		case 0xa5:
		case 0xb0:
		case 0xb1:
		case 0xb4:
		case 0xb5:
		case 0xc0:
		case 0xc1:
		case 0xc4:
		case 0xc5:
		case 0xd0:
		case 0xd1:
		case 0xd4:
		case 0xd5:
		case 0xe0:
		case 0xe1:
		case 0xe4:
		case 0xe5:
		case 0xf0:
		case 0xf1:
		case 0xf4:
		case 0xf5:
		{
			pixel = Interp2(c5, c4, c2);
			break;
		}
		case 0x9:
		case 0xd:
		case 0x19:
		case 0x1d:
		case 0x29:
		case 0x2d:
		case 0x39:
		case 0x3d:
		case 0x59:
		case 0x5d:
		case 0x79:
		case 0x89:
		case 0x8d:
		case 0x99:
		case 0x9d:
		case 0xa9:
		case 0xad:
		case 0xb9:
		case 0xbd:
		case 0xc9:
		case 0xcd:
		case 0xd9:
		case 0xdd:
		case 0xe9:
		case 0xed:
		case 0xf9:
		case 0xfd:
		{
			pixel = Interp1(c5, c2);
			break;
		}
		case 0x2f:
		case 0x3f:
		case 0x6f:
		case 0x7f:
		case 0xaf:
		case 0xbf:
		case 0xef:
		case 0xff:
		{
			if (edge(c4, c2)) {
				pixel = c5;
			} else {
				pixel = Interp10(c5, c4, c2);
			}
			break;
		}
		case 0x2:
		case 0x6:
		case 0x12:
		case 0x16:
		case 0x22:
		case 0x26:
		case 0x32:
		case 0x36:
		case 0x42:
		case 0x46:
		case 0x52:
		case 0x56:
		case 0x62:
		case 0x66:
		case 0x72:
		case 0x76:
		case 0x82:
		case 0x86:
		case 0x92:
		case 0x96:
		case 0xa2:
		case 0xa6:
		case 0xb2:
		case 0xb6:
		case 0xc2:
		case 0xc6:
		case 0xd2:
		case 0xd6:
		case 0xe2:
		case 0xe6:
		case 0xf2:
		case 0xf6:
		{
			pixel = Interp2(c5, c1, c4);
			break;
		}
		case 0xa:
		case 0x8a:
		{
			if (edge(c4, c2)) {
				pixel = Interp1(c5, c1);
			} else {
				pixel = Interp2(c5, c4, c2);
			}
			break;
		}
		case 0x13:
		case 0x17:
		case 0x33:
		case 0x37:
		case 0x77:
		{
			if (edge(c2, c6)) {
				pixel = Interp1(c5, c4);
			} else {
				pixel = Interp6(c5, c2, c4);
			}
			break;
		}
		}
		pset(pOut, pixel);

		switch (pattern) {
		case 0x17:
		case 0x37:
		case 0x77:
		case 0x96:
		case 0xb6:
		case 0xbe:
		{
			if (edge(c2, c6)) {
				pixel = c5;
			} else {
				pixel = Interp9(c5, c2, c6);
			}
			break;
		}
		case 0x2:
		case 0x3:
		case 0xa:
		case 0xb:
		case 0x22:
		case 0x23:
		case 0x2a:
		case 0x2b:
		case 0x42:
		case 0x43:
		case 0x4a:
		case 0x4b:
		case 0x62:
		case 0x63:
		case 0x6a:
		case 0x6b:
		case 0x82:
		case 0x83:
		case 0x8a:
		case 0x8b:
		case 0xa2:
		case 0xa3:
		case 0xaa:
		case 0xab:
		case 0xc2:
		case 0xc3:
		case 0xca:
		case 0xcb:
		case 0xe2:
		case 0xe3:
		case 0xea:
		case 0xeb:
		{
			pixel = Interp2(c5, c3, c6);
			break;
		}
		case 0x13:
		case 0x33:
		case 0x92:
		case 0xb2:
		{
			if (edge(c2, c6)) {
				pixel = Interp1(c5, c3);
			} else {
				pixel = Interp9(c5, c2, c6);
			}
			break;
		}
		case 0x3a:
		case 0x3b:
		case 0x53:
		case 0x5a:
		case 0x5b:
		case 0x72:
		case 0x73:
		case 0x7a:
		case 0x93:
		case 0x9a:
		case 0xb3:
		case 0xba:
		case 0xda:
		case 0xf2:
		{
			if (edge(c2, c6)) {
				pixel = Interp1(c5, c3);
			} else {
				pixel = Interp7(c5, c2, c6);
			}
			break;
		}
		case 0x16:
		case 0x1a:
		case 0x1e:
		case 0x1f:
		case 0x36:
		case 0x3e:
		case 0x3f:
		case 0x52:
		case 0x56:
		case 0x57:
		case 0x5e:
		case 0x5f:
		case 0x76:
		case 0x7e:
		case 0x7f:
		case 0x9e:
		case 0xd6:
		case 0xde:
		case 0xf6:
		case 0xfe:
		{
			if (edge(c2, c6)) {
				pixel = c5;
			} else {
				pixel = Interp2(c5, c2, c6);
			}
			break;
		}
		case 0x14:
		case 0x15:
		case 0x1c:
		case 0x1d:
		case 0x34:
		case 0x35:
		case 0x3c:
		case 0x3d:
		case 0x5c:
		case 0x5d:
		case 0x74:
		case 0x75:
		case 0x7c:
		case 0x7d:
		case 0x94:
		case 0x95:
		case 0x9c:
		case 0x9d:
		case 0xb4:
		case 0xb5:
		case 0xbc:
		case 0xbd:
		case 0xdc:
		case 0xf4:
		case 0xf5:
		case 0xfc:
		case 0xfd:
		{
			pixel = Interp1(c5, c2);
			break;
		}
		case 0x54:
		case 0x55:
		case 0xd4:
		case 0xd5:
		case 0xdd:
		{
			if (edge(c6, c8)) {
				pixel = Interp1(c5, c2);
			} else {
				pixel = Interp6(c5, c6, c2);
			}
			break;
		}
		case 0x1b:
		case 0x7b:
		case 0x9b:
		case 0xbb:
		case 0xd2:
		case 0xd3:
		case 0xdb:
		case 0xf3:
		case 0xfa:
		case 0xfb:
		{
			pixel = Interp1(c5, c3);
			break;
		}
		case 0xe:
		case 0xf:
		case 0x8e:
		case 0x8f:
		case 0xcf:
		{
			if (edge(c4, c2)) {
				pixel = Interp1(c5, c6);
			} else {
				pixel = Interp6(c5, c2, c6);
			}
			break;
		}
		case 0x0:
		case 0x1:
		case 0x4:
		case 0x5:
		case 0x8:
		case 0x9:
		case 0xc:
		case 0xd:
		case 0x20:
		case 0x21:
		case 0x24:
		case 0x25:
		case 0x28:
		case 0x29:
		case 0x2c:
		case 0x2d:
		case 0x40:
		case 0x41:
		case 0x44:
		case 0x45:
		case 0x48:
		case 0x49:
		case 0x4c:
		case 0x4d:
		case 0x60:
		case 0x61:
		case 0x64:
		case 0x65:
		case 0x68:
		case 0x69:
		case 0x6c:
		case 0x6d:
		case 0x80:
		case 0x81:
		case 0x84:
		case 0x85:
		case 0x88:
		case 0x89:
		case 0x8c:
		case 0x8d:
		case 0xa0:
		case 0xa1:
		case 0xa4:
		case 0xa5:
		case 0xa8:
		case 0xa9:
		case 0xac:
		case 0xad:
		case 0xc0:
		case 0xc1:
		case 0xc4:
		case 0xc5:
		case 0xc8:
		case 0xc9:
		case 0xcc:
		case 0xcd:
		case 0xe0:
		case 0xe1:
		case 0xe4:
		case 0xe5:
		case 0xe8:
		case 0xe9:
		case 0xec:
		case 0xed:
		{
			pixel = Interp2(c5, c2, c6);
			break;
		}
		case 0x97:
		case 0x9f:
		case 0xb7:
		case 0xbf:
		case 0xd7:
		case 0xdf:
		case 0xf7:
		case 0xff:
		{
			if (edge(c2, c6)) {
				pixel = c5;
			} else {
				pixel = Interp10(c5, c2, c6);
			}
			break;
		}
		case 0x6:
		case 0x7:
		case 0x26:
		case 0x27:
		case 0x2e:
		case 0x2f:
		case 0x46:
		case 0x47:
		case 0x4e:
		case 0x4f:
		case 0x66:
		case 0x67:
		case 0x6e:
		case 0x6f:
		case 0x86:
		case 0x87:
		case 0xa6:
		case 0xa7:
		case 0xae:
		case 0xaf:
		case 0xc6:
		case 0xc7:
		case 0xce:
		case 0xe6:
		case 0xe7:
		case 0xee:
		case 0xef:
		{
			pixel = Interp1(c5, c6);
			break;
		}
		case 0x10:
		case 0x11:
		case 0x18:
		case 0x19:
		case 0x30:
		case 0x31:
		case 0x38:
		case 0x39:
		case 0x50:
		case 0x51:
		case 0x58:
		case 0x59:
		case 0x70:
		case 0x71:
		case 0x78:
		case 0x79:
		case 0x90:
		case 0x91:
		case 0x98:
		case 0x99:
		case 0xb0:
		case 0xb1:
		case 0xb8:
		case 0xb9:
		case 0xd0:
		case 0xd1:
		case 0xd8:
		case 0xd9:
		case 0xf0:
		case 0xf1:
		case 0xf8:
		case 0xf9:
		{
			pixel = Interp2(c5, c3, c2);
			break;
		}
		case 0x12:
		case 0x32:
		{
			if (edge(c2, c6)) {
				pixel = Interp1(c5, c3);
			} else {
				pixel = Interp2(c5, c2, c6);
			}
			break;
		}
		}
		pset(pOut + 1, pixel);

		switch (pattern) {
		case 0x2a:
		case 0x2b:
		case 0xaa:
		case 0xab:
		case 0xbb:
		{
			if (edge(c4, c2)) {
				pixel = Interp1(c5, c8);
			} else {
				pixel = Interp6(c5, c4, c8);
			}
			break;
		}
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
		case 0xc0:
		case 0xc1:
		case 0xc2:
		case 0xc3:
		case 0xc4:
		case 0xc5:
		case 0xc6:
		case 0xc7:
		case 0xd0:
		case 0xd1:
		case 0xd2:
		case 0xd3:
		case 0xd4:
		case 0xd5:
		case 0xd6:
		case 0xd7:
		{
			pixel = Interp2(c5, c7, c4);
			break;
		}
		case 0x48:
		case 0x4c:
		{
			if (edge(c8, c4)) {
				pixel = Interp1(c5, c7);
			} else {
				pixel = Interp2(c5, c8, c4);
			}
			break;
		}
		case 0xe9:
		case 0xeb:
		case 0xed:
		case 0xef:
		case 0xf9:
		case 0xfb:
		case 0xfd:
		case 0xff:
		{
			if (edge(c8, c4)) {
				pixel = c5;
			} else {
				pixel = Interp10(c5, c8, c4);
			}
			break;
		}
		case 0x69:
		case 0x6d:
		case 0x7d:
		case 0xe8:
		case 0xec:
		case 0xee:
		{
			if (edge(c8, c4)) {
				pixel = c5;
			} else {
				pixel = Interp9(c5, c8, c4);
			}
			break;
		}
		case 0x28:
		case 0x29:
		case 0x2c:
		case 0x2d:
		case 0x2e:
		case 0x2f:
		case 0x38:
		case 0x39:
		case 0x3a:
		case 0x3b:
		case 0x3c:
		case 0x3d:
		case 0x3e:
		case 0x3f:
		case 0xa8:
		case 0xa9:
		case 0xac:
		case 0xad:
		case 0xae:
		case 0xaf:
		case 0xb8:
		case 0xb9:
		case 0xba:
		case 0xbc:
		case 0xbd:
		case 0xbe:
		case 0xbf:
		{
			pixel = Interp1(c5, c8);
			break;
		}
		case 0x4b:
		case 0x5f:
		case 0xcb:
		case 0xcf:
		case 0xd8:
		case 0xd9:
		case 0xdb:
		case 0xdd:
		case 0xde:
		case 0xdf:
		{
			pixel = Interp1(c5, c7);
			break;
		}
		case 0x0:
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
		case 0xa0:
		case 0xa1:
		case 0xa2:
		case 0xa3:
		case 0xa4:
		case 0xa5:
		case 0xa6:
		case 0xa7:
		case 0xb0:
		case 0xb1:
		case 0xb2:
		case 0xb3:
		case 0xb4:
		case 0xb5:
		case 0xb6:
		case 0xb7:
		{
			pixel = Interp2(c5, c8, c4);
			break;
		}
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x76:
		case 0x77:
		case 0xe0:
		case 0xe1:
		case 0xe2:
		case 0xe3:
		case 0xe4:
		case 0xe5:
		case 0xe6:
		case 0xe7:
		case 0xf2:
		case 0xf4:
		case 0xf5:
		case 0xf6:
		case 0xf7:
		{
			pixel = Interp1(c5, c4);
			break;
		}
		case 0x4a:
		case 0x58:
		case 0x68:
		case 0x6a:
		case 0x6b:
		case 0x6c:
		case 0x6e:
		case 0x6f:
		case 0x78:
		case 0x79:
		case 0x7a:
		case 0x7b:
		case 0x7c:
		case 0x7e:
		case 0x7f:
		case 0xea:
		case 0xf8:
		case 0xfa:
		case 0xfc:
		case 0xfe:
		{
			if (edge(c8, c4)) {
				pixel = c5;
			} else {
				pixel = Interp2(c5, c8, c4);
			}
			break;
		}
		case 0x8:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f:
		case 0x88:
		case 0x89:
		case 0x8a:
		case 0x8b:
		case 0x8c:
		case 0x8d:
		case 0x8e:
		case 0x8f:
		case 0x98:
		case 0x99:
		case 0x9a:
		case 0x9b:
		case 0x9c:
		case 0x9d:
		case 0x9e:
		case 0x9f:
		{
			pixel = Interp2(c5, c7, c8);
			break;
		}
		case 0x4e:
		case 0x4f:
		case 0x59:
		case 0x5a:
		case 0x5b:
		case 0x5c:
		case 0x5d:
		case 0x5e:
		case 0xc9:
		case 0xca:
		case 0xcd:
		case 0xce:
		case 0xda:
		case 0xdc:
		{
			if (edge(c8, c4)) {
				pixel = Interp1(c5, c7);
			} else {
				pixel = Interp7(c5, c8, c4);
			}
			break;
		}
		case 0x70:
		case 0x71:
		case 0xf0:
		case 0xf1:
		case 0xf3:
		{
			if (edge(c6, c8)) {
				pixel = Interp1(c5, c4);
			} else {
				pixel = Interp6(c5, c8, c4);
			}
			break;
		}
		case 0x49:
		case 0x4d:
		case 0xc8:
		case 0xcc:
		{
			if (edge(c8, c4)) {
				pixel = Interp1(c5, c7);
			} else {
				pixel = Interp9(c5, c8, c4);
			}
			break;
		}
		}
		pset(pOut + dstPitch, pixel);

		switch (pattern) {
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f:
		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
		case 0x38:
		case 0x39:
		case 0x3a:
		case 0x3b:
		case 0x3c:
		case 0x3d:
		case 0x3e:
		case 0x3f:
		{
			pixel = Interp2(c5, c9, c8);
			break;
		}
		case 0x53:
		case 0x57:
		case 0x59:
		case 0x5a:
		case 0x5b:
		case 0x5c:
		case 0x5d:
		case 0x5e:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x79:
		case 0x7a:
		{
			if (edge(c6, c8)) {
				pixel = Interp1(c5, c9);
			} else {
				pixel = Interp7(c5, c6, c8);
			}
			break;
		}
		case 0x54:
		case 0x55:
		case 0x70:
		case 0x71:
		{
			if (edge(c6, c8)) {
				pixel = Interp1(c5, c9);
			} else {
				pixel = Interp9(c5, c6, c8);
			}
			break;
		}
		case 0xc0:
		case 0xc1:
		case 0xc2:
		case 0xc3:
		case 0xc4:
		case 0xc5:
		case 0xc6:
		case 0xc7:
		case 0xc9:
		case 0xca:
		case 0xcb:
		case 0xcd:
		case 0xce:
		case 0xcf:
		case 0xe0:
		case 0xe1:
		case 0xe2:
		case 0xe3:
		case 0xe4:
		case 0xe5:
		case 0xe6:
		case 0xe7:
		case 0xe9:
		case 0xea:
		case 0xeb:
		case 0xed:
		case 0xef:
		{
			pixel = Interp1(c5, c6);
			break;
		}
		case 0x56:
		case 0x5f:
		case 0x76:
		case 0x77:
		case 0x78:
		case 0x7b:
		case 0x7c:
		case 0x7d:
		case 0x7e:
		case 0x7f:
		{
			pixel = Interp1(c5, c9);
			break;
		}
		case 0xc8:
		case 0xcc:
		case 0xe8:
		case 0xec:
		case 0xee:
		{
			if (edge(c8, c4)) {
				pixel = Interp1(c5, c6);
			} else {
				pixel = Interp6(c5, c8, c6);
			}
			break;
		}
		case 0xd4:
		case 0xd5:
		case 0xdd:
		case 0xf0:
		case 0xf1:
		case 0xf3:
		{
			if (edge(c6, c8)) {
				pixel = c5;
			} else {
				pixel = Interp9(c5, c6, c8);
			}
			break;
		}
		case 0x0:
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x8:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
		case 0x28:
		case 0x29:
		case 0x2a:
		case 0x2b:
		case 0x2c:
		case 0x2d:
		case 0x2e:
		case 0x2f:
		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
		case 0x88:
		case 0x89:
		case 0x8a:
		case 0x8b:
		case 0x8c:
		case 0x8d:
		case 0x8e:
		case 0x8f:
		case 0xa0:
		case 0xa1:
		case 0xa2:
		case 0xa3:
		case 0xa4:
		case 0xa5:
		case 0xa6:
		case 0xa7:
		case 0xa8:
		case 0xa9:
		case 0xaa:
		case 0xab:
		case 0xac:
		case 0xad:
		case 0xae:
		case 0xaf:
		{
			pixel = Interp2(c5, c6, c8);
			break;
		}
		case 0x50:
		case 0x51:
		{
			if (edge(c6, c8)) {
				pixel = Interp1(c5, c9);
			} else {
				pixel = Interp2(c5, c6, c8);
			}
			break;
		}
		case 0x90:
		case 0x91:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x97:
		case 0x98:
		case 0x99:
		case 0x9a:
		case 0x9b:
		case 0x9c:
		case 0x9d:
		case 0x9e:
		case 0x9f:
		case 0xb0:
		case 0xb1:
		case 0xb3:
		case 0xb4:
		case 0xb5:
		case 0xb7:
		case 0xb8:
		case 0xb9:
		case 0xba:
		case 0xbb:
		case 0xbc:
		case 0xbd:
		case 0xbf:
		{
			pixel = Interp1(c5, c8);
			break;
		}
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
		case 0x48:
		case 0x49:
		case 0x4a:
		case 0x4b:
		case 0x4c:
		case 0x4d:
		case 0x4e:
		case 0x4f:
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
		case 0x68:
		case 0x69:
		case 0x6a:
		case 0x6b:
		case 0x6c:
		case 0x6d:
		case 0x6e:
		case 0x6f:
		{
			pixel = Interp2(c5, c9, c6);
			break;
		}
		case 0x92:
		case 0x96:
		case 0xb2:
		case 0xb6:
		case 0xbe:
		{
			if (edge(c2, c6)) {
				pixel = Interp1(c5, c8);
			} else {
				pixel = Interp6(c5, c6, c8);
			}
			break;
		}
		case 0xf4:
		case 0xf5:
		case 0xf6:
		case 0xf7:
		case 0xfc:
		case 0xfd:
		case 0xfe:
		case 0xff:
		{
			if (edge(c6, c8)) {
				pixel = c5;
			} else {
				pixel = Interp10(c5, c6, c8);
			}
			break;
		}
		case 0x52:
		case 0x58:
		case 0xd0:
		case 0xd1:
		case 0xd2:
		case 0xd3:
		case 0xd6:
		case 0xd7:
		case 0xd8:
		case 0xd9:
		case 0xda:
		case 0xdb:
		case 0xdc:
		case 0xde:
		case 0xdf:
		case 0xf2:
		case 0xf8:
		case 0xf9:
		case 0xfa:
		case 0xfb:
		{
			if (edge(c6, c8)) {
				pixel = c5;
			} else {
				pixel = Interp2(c5, c6, c8);
			}
			break;
		}
		}
		pset(pOut + dstPitch + 1, pixel);

		c1 = c2; c4 = c5; c7 = c8;
		c2 = c3; c5 = c6; c8 = c9;
		pIn += 1;
		pOut += 2;
	}
}

template <class Pixel>
void HQ2xScaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	const int srcPitch = src->pitch / sizeof(Pixel);
	assert(srcY < endSrcY);
	scaleLine256(src, srcY, dst, dstY,
		0, srcY < endSrcY - 1 ? srcPitch : 0
		);
	srcY += 1;
	dstY += 2;
	while (srcY < endSrcY - 1) {
		scaleLine256(src, srcY, dst, dstY, -srcPitch, srcPitch);
		srcY += 1;
		dstY += 2;
	}
	if (srcY < endSrcY) {
		scaleLine256(src, srcY, dst, dstY, -srcPitch, 0);
	}
}

} // namespace openmsx

