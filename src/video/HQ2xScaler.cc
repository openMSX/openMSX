// $Id$

/*
Original code: Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
openMSX adaptation by Maarten ter Huurne

License: LGPL

Visit the HiEnd3D site for info:
  http://www.hiend3d.com/hq2x.html
*/

#include "HQ2xScaler.hh"
#include <cmath>

using std::abs;


namespace openmsx {

// Force template instantiation.
template class HQ2xScaler<word>;
template class HQ2xScaler<unsigned int>;

template <class Pixel>
inline Pix16 HQ2xScaler<Pixel>::readPixel(Pixel *pIn) {
	// TODO: Use surface info instead.
	if (typeid(Pixel) == typeid(word)) {
		word pixel = *pIn;
		return pixel;
	} else {
		unsigned int pixel = *pIn;
		int r = (pixel & 0xFF0000) >> (16 + 8 - 5);
		int g = (pixel & 0x00FF00) >> (8 + 8 - 6);
		int b = (pixel & 0x0000FF) >> (8 - 5);
		return (Pix16)((r << 11) | (g << 5) | b);
	}
}

template <class Pixel>
inline void HQ2xScaler<Pixel>::pset(Pixel* pOut, Pix32 colour)
{
	// TODO: Use surface info instead.
	if (typeid(Pixel) == typeid(word)) {
		int r = (colour & 0xFF0000) >> (16 + 8 - 5);
		int g = (colour & 0x00FF00) >> (8 + 8 - 6);
		int b = (colour & 0x0000FF) >> (8 - 5);
		*pOut = (word)((r << 11) | (g << 5) | b);
	} else {
		*pOut = colour;
	}
}

template <class Pixel>
inline bool HQ2xScaler<Pixel>::edge(Pix16 w1, Pix16 w2)
{
	enum {
		Ymask = 0x00FF0000,
		Umask = 0x0000FF00,
		Vmask = 0x000000FF,
		trY   = 0x00300000,
		trU   = 0x00000700,
		trV   = 0x00000006,
	};

	int YUV1 = RGBtoYUV[w1];
	int YUV2 = RGBtoYUV[w2];
	return ( abs((YUV1 & Ymask) - (YUV2 & Ymask)) > trY )
		|| ( abs((YUV1 & Umask) - (YUV2 & Umask)) > trU )
		|| ( abs((YUV1 & Vmask) - (YUV2 & Vmask)) > trV );
}

template <int w1, int w2>
inline Pix32 interpolate(Pix32 c1, Pix32 c2) {
	enum { shift = w1 + w2 == 2 ? 1 : 2 };
	// TODO: Why no masking here, unlike the routine below?
	return (c1 * w1 + c2 * w2) >> shift;
}

template <int w1, int w2, int w3>
inline Pix32 interpolate(Pix32 c1, Pix32 c2, Pix32 c3) {
	enum { shift = w1 + w2 + w3 == 8 ? 3 : 4 };
	return
		(
			(
				(
					(c1 & 0x00FF00) * w1 +
					(c2 & 0x00FF00) * w2 +
					(c3 & 0x00FF00) * w3
				) & (0x00FF00 << shift)
			) | (
				(
					(c1 & 0xFF00FF) * w1 +
					(c2 & 0xFF00FF) * w2 +
					(c3 & 0xFF00FF) * w3
				) & (0xFF00FF << shift)
			)
		) >> shift;
}

inline Pix32 Interp1(Pix32 c1, Pix32 c2)
{
	return interpolate<3, 1>(c1, c2);
}

inline Pix32 Interp2(Pix32 c1, Pix32 c2, Pix32 c3)
{
	// TODO: Why no masking here?
	return (c1 * 2 + c2 + c3) >> 2;
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


#define PIXEL00_0     pset(pOut, c[5]);
#define PIXEL00_10    pset(pOut, Interp1(c[5], c[1]));
#define PIXEL00_11    pset(pOut, Interp1(c[5], c[4]));
#define PIXEL00_12    pset(pOut, Interp1(c[5], c[2]));
#define PIXEL00_20    pset(pOut, Interp2(c[5], c[4], c[2]));
#define PIXEL00_21    pset(pOut, Interp2(c[5], c[1], c[2]));
#define PIXEL00_22    pset(pOut, Interp2(c[5], c[1], c[4]));
#define PIXEL00_60    pset(pOut, Interp6(c[5], c[2], c[4]));
#define PIXEL00_61    pset(pOut, Interp6(c[5], c[4], c[2]));
#define PIXEL00_70    pset(pOut, Interp7(c[5], c[4], c[2]));
#define PIXEL00_90    pset(pOut, Interp9(c[5], c[4], c[2]));
#define PIXEL00_100   pset(pOut, Interp10(c[5], c[4], c[2]));
#define PIXEL01_0     pset(pOut+1, c[5]);
#define PIXEL01_10    pset(pOut+1, Interp1(c[5], c[3]));
#define PIXEL01_11    pset(pOut+1, Interp1(c[5], c[2]));
#define PIXEL01_12    pset(pOut+1, Interp1(c[5], c[6]));
#define PIXEL01_20    pset(pOut+1, Interp2(c[5], c[2], c[6]));
#define PIXEL01_21    pset(pOut+1, Interp2(c[5], c[3], c[6]));
#define PIXEL01_22    pset(pOut+1, Interp2(c[5], c[3], c[2]));
#define PIXEL01_60    pset(pOut+1, Interp6(c[5], c[6], c[2]));
#define PIXEL01_61    pset(pOut+1, Interp6(c[5], c[2], c[6]));
#define PIXEL01_70    pset(pOut+1, Interp7(c[5], c[2], c[6]));
#define PIXEL01_90    pset(pOut+1, Interp9(c[5], c[2], c[6]));
#define PIXEL01_100   pset(pOut+1, Interp10(c[5], c[2], c[6]));
#define PIXEL10_0     pset(pOut+dstPitch, c[5]);
#define PIXEL10_10    pset(pOut+dstPitch, Interp1(c[5], c[7]));
#define PIXEL10_11    pset(pOut+dstPitch, Interp1(c[5], c[8]));
#define PIXEL10_12    pset(pOut+dstPitch, Interp1(c[5], c[4]));
#define PIXEL10_20    pset(pOut+dstPitch, Interp2(c[5], c[8], c[4]));
#define PIXEL10_21    pset(pOut+dstPitch, Interp2(c[5], c[7], c[4]));
#define PIXEL10_22    pset(pOut+dstPitch, Interp2(c[5], c[7], c[8]));
#define PIXEL10_60    pset(pOut+dstPitch, Interp6(c[5], c[4], c[8]));
#define PIXEL10_61    pset(pOut+dstPitch, Interp6(c[5], c[8], c[4]));
#define PIXEL10_70    pset(pOut+dstPitch, Interp7(c[5], c[8], c[4]));
#define PIXEL10_90    pset(pOut+dstPitch, Interp9(c[5], c[8], c[4]));
#define PIXEL10_100   pset(pOut+dstPitch, Interp10(c[5], c[8], c[4]));
#define PIXEL11_0     pset(pOut+dstPitch+1, c[5]);
#define PIXEL11_10    pset(pOut+dstPitch+1, Interp1(c[5], c[9]));
#define PIXEL11_11    pset(pOut+dstPitch+1, Interp1(c[5], c[6]));
#define PIXEL11_12    pset(pOut+dstPitch+1, Interp1(c[5], c[8]));
#define PIXEL11_20    pset(pOut+dstPitch+1, Interp2(c[5], c[6], c[8]));
#define PIXEL11_21    pset(pOut+dstPitch+1, Interp2(c[5], c[9], c[8]));
#define PIXEL11_22    pset(pOut+dstPitch+1, Interp2(c[5], c[9], c[6]));
#define PIXEL11_60    pset(pOut+dstPitch+1, Interp6(c[5], c[8], c[6]));
#define PIXEL11_61    pset(pOut+dstPitch+1, Interp6(c[5], c[6], c[8]));
#define PIXEL11_70    pset(pOut+dstPitch+1, Interp7(c[5], c[6], c[8]));
#define PIXEL11_90    pset(pOut+dstPitch+1, Interp9(c[5], c[6], c[8]));
#define PIXEL11_100   pset(pOut+dstPitch+1, Interp10(c[5], c[6], c[8]));



template <class Pixel>
HQ2xScaler<Pixel>::HQ2xScaler() {
	for (int i = 0; i < 65536; i++) {
		LUT16to32[i] =
			((i & 0xF800) << 8) | ((i & 0x07E0) << 5) | ((i & 0x001F) << 3);
	}

	for (int i = 0; i < 32; i++)
	for (int j = 0; j < 64; j++)
	for (int k = 0; k < 32; k++) {
		int r = i << 3;
		int g = j << 2;
		int b = k << 3;
		int Y = (r + g + b) >> 2;
		int u = 128 + ((r - b) >> 2);
		int v = 128 + ((-r + 2 * g - b) >> 3);
		RGBtoYUV[ (i << 11) | (j << 5) | k ] = (Y << 16) | (u << 8) | v;
	}
}

template <class Pixel>
void HQ2xScaler<Pixel>::scaleLine256(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY )
{
	const int width = 320; // TODO: Specify this in a clean way.
	const int srcPitch = src->pitch / sizeof(Pixel);
	const int dstPitch = dst->pitch / sizeof(Pixel);

	Pixel* pIn = (Pixel*)src->pixels + srcY * srcPitch;
	Pixel* pOut = (Pixel*)dst->pixels + dstY * dstPitch;
	const int prevLine = srcY > 0 ? -srcPitch : 0;
	const int nextLine = srcY < src->h - 1 ? srcPitch : 0;

	for (int i = 0; i < width; i++) {
		Pix16 w[10];
		Pix32 c[10];

		//   +----+----+----+
		//   |    |    |    |
		//   | w1 | w2 | w3 |
		//   +----+----+----+
		//   |    |    |    |
		//   | w4 | w5 | w6 |
		//   +----+----+----+
		//   |    |    |    |
		//   | w7 | w8 | w9 |
		//   +----+----+----+

		w[2] = readPixel(pIn + prevLine);
		w[5] = readPixel(pIn);
		w[8] = readPixel(pIn + nextLine);

		if (i > 0) {
			w[1] = readPixel(pIn + prevLine - 1);
			w[4] = readPixel(pIn - 1);
			w[7] = readPixel(pIn + nextLine - 1);
		} else {
			w[1] = w[2];
			w[4] = w[5];
			w[7] = w[8];
		}

		if (i < width - 1) {
			w[3] = readPixel(pIn + prevLine + 1);
			w[6] = readPixel(pIn + 1);
			w[9] = readPixel(pIn + nextLine + 1);
		} else {
			w[3] = w[2];
			w[6] = w[5];
			w[9] = w[8];
		}

		int pattern = 0;
		for (int k = 1; k <= 9; k++) {
			if (k != 5) {
				// TODO: Original code had edge() body substituted here to
				//       avoid repeated lookup of RGBtoYUV[w[5]].
				//       Try to find another optimisation that is more readable.
				pattern >>= 1;
				if (w[k] != w[5] && edge(w[5], w[k])) pattern |= 0x80;
			}
			c[k] = LUT16to32[w[k]];
		}

		switch (pattern) {
		case 0x2a:
		case 0xaa:
		case 0xe:
		case 0x8e:
		{
			if (edge(w[4], w[2])) {
				PIXEL00_10
			} else {
				PIXEL00_90
			}
			break;
		}
		case 0x8:
		case 0xc:
		case 0x88:
		case 0x8c:
		case 0x28:
		case 0x2c:
		case 0xa8:
		case 0xac:
		case 0x48:
		case 0x4c:
		case 0x18:
		case 0x68:
		case 0x6c:
		case 0xc8:
		case 0xcc:
		case 0x1c:
		case 0x98:
		case 0x38:
		case 0x58:
		case 0xf8:
		case 0xd8:
		case 0x78:
		case 0xb8:
		case 0x9c:
		case 0x3c:
		case 0x5c:
		case 0xec:
		case 0xe8:
		case 0x7c:
		case 0xbc:
		case 0xdc:
		case 0xfc:
		{
			PIXEL00_21
			break;
		}
		case 0xb:
		case 0x8b:
		case 0x1a:
		case 0x1f:
		case 0x4a:
		case 0x6b:
		case 0x1b:
		case 0x4b:
		case 0xcb:
		case 0x9b:
		case 0x3b:
		case 0x4f:
		case 0x5b:
		case 0xdb:
		case 0x7b:
		case 0x5f:
		case 0xeb:
		case 0x9f:
		case 0xfb:
		case 0xdf:
		{
			if (edge(w[4], w[2])) {
				PIXEL00_0
			} else {
				PIXEL00_20
			}
			break;
		}
		case 0x3a:
		case 0xca:
		case 0x4e:
		case 0x9a:
		case 0x5a:
		case 0x9e:
		case 0xea:
		case 0x7a:
		case 0x5e:
		case 0xda:
		case 0xba:
		case 0xce:
		case 0xae:
		case 0x2e:
		{
			if (edge(w[4], w[2])) {
				PIXEL00_10
			} else {
				PIXEL00_70
			}
			break;
		}
		case 0xab:
		case 0x2b:
		case 0x8f:
		case 0xf:
		case 0xcf:
		case 0xbb:
		{
			if (edge(w[4], w[2])) {
				PIXEL00_0
			} else {
				PIXEL00_90
			}
			break;
		}
		case 0x3:
		case 0x23:
		case 0x83:
		case 0xa3:
		case 0x7:
		case 0x27:
		case 0x87:
		case 0x43:
		case 0x63:
		case 0x47:
		case 0xc3:
		case 0x53:
		case 0xd3:
		case 0x67:
		case 0xe3:
		case 0xc7:
		case 0x57:
		case 0xa7:
		case 0x73:
		case 0xb3:
		case 0x93:
		case 0xe7:
		case 0xf3:
		case 0xb7:
		case 0x97:
		case 0xd7:
		case 0xf7:
		{
			PIXEL00_11
			break;
		}
		case 0x6a:
		case 0x1e:
		case 0x3e:
		case 0x6e:
		case 0x7e:
		case 0xee:
		case 0xbe:
		case 0xfa:
		case 0xde:
		case 0xfe:
		{
			PIXEL00_10
			break;
		}
		case 0x49:
		case 0x4d:
		case 0x6d:
		case 0x69:
		case 0x7d:
		{
			if (edge(w[8], w[4])) {
				PIXEL00_12
			} else {
				PIXEL00_61
			}
			break;
		}
		case 0x0:
		case 0x1:
		case 0x4:
		case 0x20:
		case 0x80:
		case 0x5:
		case 0x84:
		case 0xa0:
		case 0x21:
		case 0x81:
		case 0x24:
		case 0x85:
		case 0xa4:
		case 0xa1:
		case 0x25:
		case 0xa5:
		case 0x10:
		case 0x11:
		case 0x30:
		case 0x31:
		case 0x40:
		case 0x41:
		case 0x44:
		case 0x45:
		case 0x14:
		case 0x15:
		case 0x34:
		case 0x35:
		case 0x90:
		case 0x91:
		case 0xb0:
		case 0xb1:
		case 0xc0:
		case 0xc1:
		case 0xc4:
		case 0xc5:
		case 0x60:
		case 0x61:
		case 0x64:
		case 0x65:
		case 0x50:
		case 0x51:
		case 0x94:
		case 0x95:
		case 0xb4:
		case 0xe0:
		case 0xe4:
		case 0xe1:
		case 0xd0:
		case 0xd1:
		case 0x54:
		case 0x55:
		case 0x70:
		case 0x71:
		case 0xd5:
		case 0xd4:
		case 0xf1:
		case 0xf0:
		case 0xe5:
		case 0xb5:
		case 0x75:
		case 0x74:
		case 0xf5:
		case 0xf4:
		{
			PIXEL00_20
			break;
		}
		case 0x9:
		case 0xd:
		case 0x89:
		case 0x8d:
		case 0x29:
		case 0xa9:
		case 0x2d:
		case 0x19:
		case 0x1d:
		case 0x39:
		case 0x99:
		case 0x59:
		case 0xd9:
		case 0xb9:
		case 0x3d:
		case 0x9d:
		case 0x79:
		case 0xad:
		case 0x5d:
		case 0xcd:
		case 0xc9:
		case 0xbd:
		case 0xdd:
		case 0xed:
		case 0xe9:
		case 0xf9:
		case 0xfd:
		{
			PIXEL00_12
			break;
		}
		case 0xaf:
		case 0x2f:
		case 0x6f:
		case 0x3f:
		case 0xef:
		case 0x7f:
		case 0xbf:
		case 0xff:
		{
			if (edge(w[4], w[2])) {
				PIXEL00_0
			} else {
				PIXEL00_100
			}
			break;
		}
		case 0x2:
		case 0x22:
		case 0x82:
		case 0xa2:
		case 0x6:
		case 0x26:
		case 0x86:
		case 0xa6:
		case 0x12:
		case 0x32:
		case 0x42:
		case 0x16:
		case 0x36:
		case 0x92:
		case 0xb2:
		case 0x46:
		case 0xc2:
		case 0x62:
		case 0x52:
		case 0xd6:
		case 0x56:
		case 0xd2:
		case 0xc6:
		case 0xe2:
		case 0x66:
		case 0x72:
		case 0xb6:
		case 0x96:
		case 0x76:
		case 0xe6:
		case 0xf2:
		case 0xf6:
		{
			PIXEL00_22
			break;
		}
		case 0xa:
		case 0x8a:
		{
			if (edge(w[4], w[2])) {
				PIXEL00_10
			} else {
				PIXEL00_20
			}
			break;
		}
		case 0x13:
		case 0x33:
		case 0x37:
		case 0x17:
		case 0x77:
		{
			if (edge(w[2], w[6])) {
				PIXEL00_11
			} else {
				PIXEL00_60
			}
			break;
		}
		}

		switch (pattern) {
		case 0x37:
		case 0x17:
		case 0xb6:
		case 0x96:
		case 0xbe:
		case 0x77:
		{
			if (edge(w[2], w[6])) {
				PIXEL01_0
			} else {
				PIXEL01_90
			}
			break;
		}
		case 0x2:
		case 0x22:
		case 0x82:
		case 0xa2:
		case 0x3:
		case 0x23:
		case 0x83:
		case 0xa3:
		case 0xa:
		case 0x8a:
		case 0x42:
		case 0xb:
		case 0x8b:
		case 0x2a:
		case 0xaa:
		case 0x43:
		case 0xc2:
		case 0x62:
		case 0x4a:
		case 0x6b:
		case 0x6a:
		case 0x4b:
		case 0x63:
		case 0xe2:
		case 0xc3:
		case 0xca:
		case 0xab:
		case 0x2b:
		case 0xcb:
		case 0xe3:
		case 0xea:
		case 0xeb:
		{
			PIXEL01_21
			break;
		}
		case 0x13:
		case 0x33:
		case 0x92:
		case 0xb2:
		{
			if (edge(w[2], w[6])) {
				PIXEL01_10
			} else {
				PIXEL01_90
			}
			break;
		}
		case 0x3a:
		case 0x53:
		case 0x9a:
		case 0x72:
		case 0x5a:
		case 0xf2:
		case 0x3b:
		case 0x7a:
		case 0xda:
		case 0x5b:
		case 0xba:
		case 0x73:
		case 0xb3:
		case 0x93:
		{
			if (edge(w[2], w[6])) {
				PIXEL01_10
			} else {
				PIXEL01_70
			}
			break;
		}
		case 0x16:
		case 0x36:
		case 0x1a:
		case 0x1f:
		case 0x52:
		case 0xd6:
		case 0x56:
		case 0x1e:
		case 0x3e:
		case 0x76:
		case 0x9e:
		case 0x57:
		case 0x5e:
		case 0x7e:
		case 0x5f:
		case 0xde:
		case 0x3f:
		case 0xf6:
		case 0xfe:
		case 0x7f:
		{
			if (edge(w[2], w[6])) {
				PIXEL01_0
			} else {
				PIXEL01_20
			}
			break;
		}
		case 0x14:
		case 0x15:
		case 0x34:
		case 0x35:
		case 0x94:
		case 0x95:
		case 0xb4:
		case 0x1c:
		case 0x1d:
		case 0x9c:
		case 0x3c:
		case 0x5c:
		case 0x7c:
		case 0xbc:
		case 0x3d:
		case 0x9d:
		case 0xdc:
		case 0xb5:
		case 0x5d:
		case 0x75:
		case 0x74:
		case 0xbd:
		case 0x7d:
		case 0xf5:
		case 0xf4:
		case 0xfc:
		case 0xfd:
		{
			PIXEL01_11
			break;
		}
		case 0x54:
		case 0x55:
		case 0xd5:
		case 0xd4:
		case 0xdd:
		{
			if (edge(w[6], w[8])) {
				PIXEL01_11
			} else {
				PIXEL01_60
			}
			break;
		}
		case 0x1b:
		case 0xd2:
		case 0xd3:
		case 0x9b:
		case 0xdb:
		case 0xbb:
		case 0xf3:
		case 0xfa:
		case 0x7b:
		case 0xfb:
		{
			PIXEL01_10
			break;
		}
		case 0xe:
		case 0x8e:
		case 0x8f:
		case 0xf:
		case 0xcf:
		{
			if (edge(w[4], w[2])) {
				PIXEL01_12
			} else {
				PIXEL01_61
			}
			break;
		}
		case 0x0:
		case 0x1:
		case 0x4:
		case 0x20:
		case 0x80:
		case 0x5:
		case 0x84:
		case 0xa0:
		case 0x21:
		case 0x81:
		case 0x24:
		case 0x85:
		case 0xa4:
		case 0xa1:
		case 0x25:
		case 0xa5:
		case 0x40:
		case 0x41:
		case 0x44:
		case 0x45:
		case 0x8:
		case 0xc:
		case 0x88:
		case 0x8c:
		case 0xc0:
		case 0xc1:
		case 0xc4:
		case 0xc5:
		case 0x60:
		case 0x61:
		case 0x64:
		case 0x65:
		case 0x28:
		case 0x2c:
		case 0xa8:
		case 0xac:
		case 0x9:
		case 0xd:
		case 0x89:
		case 0x8d:
		case 0x48:
		case 0x4c:
		case 0xe0:
		case 0xe4:
		case 0xe1:
		case 0x29:
		case 0xa9:
		case 0x2d:
		case 0x68:
		case 0x6c:
		case 0xc8:
		case 0xcc:
		case 0x49:
		case 0x4d:
		case 0xec:
		case 0xe8:
		case 0x6d:
		case 0x69:
		case 0xe5:
		case 0xad:
		case 0xcd:
		case 0xc9:
		case 0xed:
		case 0xe9:
		{
			PIXEL01_20
			break;
		}
		case 0xb7:
		case 0x97:
		case 0x9f:
		case 0xd7:
		case 0xbf:
		case 0xdf:
		case 0xf7:
		case 0xff:
		{
			if (edge(w[2], w[6])) {
				PIXEL01_0
			} else {
				PIXEL01_100
			}
			break;
		}
		case 0x6:
		case 0x26:
		case 0x86:
		case 0xa6:
		case 0x7:
		case 0x27:
		case 0x87:
		case 0x46:
		case 0xc6:
		case 0x47:
		case 0x66:
		case 0x4e:
		case 0x6e:
		case 0x67:
		case 0xe6:
		case 0xc7:
		case 0x4f:
		case 0xa7:
		case 0xce:
		case 0xae:
		case 0x2e:
		case 0xe7:
		case 0xee:
		case 0xaf:
		case 0x2f:
		case 0x6f:
		case 0xef:
		{
			PIXEL01_12
			break;
		}
		case 0x10:
		case 0x11:
		case 0x30:
		case 0x31:
		case 0x90:
		case 0x91:
		case 0xb0:
		case 0xb1:
		case 0x50:
		case 0x51:
		case 0x18:
		case 0xd0:
		case 0xd1:
		case 0x70:
		case 0x71:
		case 0x98:
		case 0x38:
		case 0x19:
		case 0x58:
		case 0xf8:
		case 0xd8:
		case 0x78:
		case 0xb8:
		case 0x39:
		case 0x99:
		case 0x59:
		case 0xf1:
		case 0xf0:
		case 0xd9:
		case 0xb9:
		case 0x79:
		case 0xf9:
		{
			PIXEL01_22
			break;
		}
		case 0x12:
		case 0x32:
		{
			if (edge(w[2], w[6])) {
				PIXEL01_10
			} else {
				PIXEL01_20
			}
			break;
		}
		}

		switch (pattern) {
		case 0x2a:
		case 0xaa:
		case 0xab:
		case 0x2b:
		case 0xbb:
		{
			if (edge(w[4], w[2])) {
				PIXEL10_11
			} else {
				PIXEL10_60
			}
			break;
		}
		case 0x40:
		case 0x41:
		case 0x44:
		case 0x45:
		case 0xc0:
		case 0xc1:
		case 0xc4:
		case 0xc5:
		case 0x50:
		case 0x51:
		case 0x42:
		case 0xd0:
		case 0xd1:
		case 0x54:
		case 0x55:
		case 0x43:
		case 0x46:
		case 0xc2:
		case 0x52:
		case 0xd6:
		case 0x56:
		case 0xd2:
		case 0xc6:
		case 0x47:
		case 0xc3:
		case 0x53:
		case 0xd5:
		case 0xd4:
		case 0xd3:
		case 0xc7:
		case 0x57:
		case 0xd7:
		{
			PIXEL10_21
			break;
		}
		case 0x48:
		case 0x4c:
		{
			if (edge(w[8], w[4])) {
				PIXEL10_10
			} else {
				PIXEL10_20
			}
			break;
		}
		case 0xed:
		case 0xe9:
		case 0xf9:
		case 0xeb:
		case 0xfd:
		case 0xfb:
		case 0xef:
		case 0xff:
		{
			if (edge(w[8], w[4])) {
				PIXEL10_0
			} else {
				PIXEL10_100
			}
			break;
		}
		case 0xec:
		case 0xe8:
		case 0x6d:
		case 0x69:
		case 0x7d:
		case 0xee:
		{
			if (edge(w[8], w[4])) {
				PIXEL10_0
			} else {
				PIXEL10_90
			}
			break;
		}
		case 0x28:
		case 0x2c:
		case 0xa8:
		case 0xac:
		case 0x29:
		case 0xa9:
		case 0x2d:
		case 0x38:
		case 0xb8:
		case 0x39:
		case 0x3c:
		case 0x3a:
		case 0x3e:
		case 0xbc:
		case 0xb9:
		case 0x3d:
		case 0x3b:
		case 0xad:
		case 0xba:
		case 0xae:
		case 0x2e:
		case 0xbd:
		case 0xbe:
		case 0xaf:
		case 0x2f:
		case 0x3f:
		case 0xbf:
		{
			PIXEL10_11
			break;
		}
		case 0xd8:
		case 0x4b:
		case 0xcb:
		case 0xd9:
		case 0xdb:
		case 0xdd:
		case 0xcf:
		case 0x5f:
		case 0xde:
		case 0xdf:
		{
			PIXEL10_10
			break;
		}
		case 0x0:
		case 0x1:
		case 0x4:
		case 0x20:
		case 0x80:
		case 0x5:
		case 0x84:
		case 0xa0:
		case 0x21:
		case 0x81:
		case 0x24:
		case 0x85:
		case 0xa4:
		case 0xa1:
		case 0x25:
		case 0xa5:
		case 0x2:
		case 0x22:
		case 0x82:
		case 0xa2:
		case 0x10:
		case 0x11:
		case 0x30:
		case 0x31:
		case 0x3:
		case 0x23:
		case 0x83:
		case 0xa3:
		case 0x6:
		case 0x26:
		case 0x86:
		case 0xa6:
		case 0x14:
		case 0x15:
		case 0x34:
		case 0x35:
		case 0x90:
		case 0x91:
		case 0xb0:
		case 0xb1:
		case 0x12:
		case 0x32:
		case 0x7:
		case 0x27:
		case 0x87:
		case 0x94:
		case 0x95:
		case 0xb4:
		case 0x16:
		case 0x36:
		case 0x13:
		case 0x33:
		case 0x92:
		case 0xb2:
		case 0x37:
		case 0x17:
		case 0xb6:
		case 0x96:
		case 0xa7:
		case 0xb5:
		case 0xb3:
		case 0x93:
		case 0xb7:
		case 0x97:
		{
			PIXEL10_20
			break;
		}
		case 0x60:
		case 0x61:
		case 0x64:
		case 0x65:
		case 0xe0:
		case 0xe4:
		case 0xe1:
		case 0x62:
		case 0x63:
		case 0xe2:
		case 0x66:
		case 0x72:
		case 0x76:
		case 0x67:
		case 0xe3:
		case 0xe6:
		case 0xf2:
		case 0xe5:
		case 0x73:
		case 0x75:
		case 0x74:
		case 0xe7:
		case 0x77:
		case 0xf5:
		case 0xf4:
		case 0xf6:
		case 0xf7:
		{
			PIXEL10_12
			break;
		}
		case 0x68:
		case 0x6c:
		case 0x58:
		case 0xf8:
		case 0x4a:
		case 0x6b:
		case 0x6a:
		case 0x78:
		case 0x7c:
		case 0x6e:
		case 0xea:
		case 0x79:
		case 0x7a:
		case 0x7e:
		case 0xfa:
		case 0x7b:
		case 0xfc:
		case 0x6f:
		case 0xfe:
		case 0x7f:
		{
			if (edge(w[8], w[4])) {
				PIXEL10_0
			} else {
				PIXEL10_20
			}
			break;
		}
		case 0x8:
		case 0xc:
		case 0x88:
		case 0x8c:
		case 0x9:
		case 0xd:
		case 0x89:
		case 0x8d:
		case 0xa:
		case 0x8a:
		case 0x18:
		case 0xb:
		case 0x8b:
		case 0xe:
		case 0x8e:
		case 0x1c:
		case 0x98:
		case 0x19:
		case 0x1a:
		case 0x1f:
		case 0x1b:
		case 0x1e:
		case 0x1d:
		case 0x9c:
		case 0x99:
		case 0x9a:
		case 0x8f:
		case 0xf:
		case 0x9b:
		case 0x9d:
		case 0x9e:
		case 0x9f:
		{
			PIXEL10_22
			break;
		}
		case 0x5c:
		case 0xca:
		case 0x4e:
		case 0x59:
		case 0x5a:
		case 0xdc:
		case 0x4f:
		case 0x5e:
		case 0xda:
		case 0x5b:
		case 0x5d:
		case 0xce:
		case 0xcd:
		case 0xc9:
		{
			if (edge(w[8], w[4])) {
				PIXEL10_10
			} else {
				PIXEL10_70
			}
			break;
		}
		case 0x70:
		case 0x71:
		case 0xf1:
		case 0xf0:
		case 0xf3:
		{
			if (edge(w[6], w[8])) {
				PIXEL10_12
			} else {
				PIXEL10_61
			}
			break;
		}
		case 0xc8:
		case 0xcc:
		case 0x49:
		case 0x4d:
		{
			if (edge(w[8], w[4])) {
				PIXEL10_10
			} else {
				PIXEL10_90
			}
			break;
		}
		}

		switch (pattern) {
		case 0x10:
		case 0x11:
		case 0x30:
		case 0x31:
		case 0x14:
		case 0x15:
		case 0x34:
		case 0x35:
		case 0x12:
		case 0x32:
		case 0x18:
		case 0x16:
		case 0x36:
		case 0x13:
		case 0x33:
		case 0x1c:
		case 0x38:
		case 0x19:
		case 0x1a:
		case 0x1f:
		case 0x1b:
		case 0x1e:
		case 0x1d:
		case 0x39:
		case 0x3c:
		case 0x3a:
		case 0x37:
		case 0x17:
		case 0x3e:
		case 0x3d:
		case 0x3b:
		case 0x3f:
		{
			PIXEL11_21
			break;
		}
		case 0x53:
		case 0x5c:
		case 0x72:
		case 0x59:
		case 0x5a:
		case 0x79:
		case 0x57:
		case 0x7a:
		case 0x5e:
		case 0x5b:
		case 0x73:
		case 0x5d:
		case 0x75:
		case 0x74:
		{
			if (edge(w[6], w[8])) {
				PIXEL11_10
			} else {
				PIXEL11_70
			}
			break;
		}
		case 0x54:
		case 0x55:
		case 0x70:
		case 0x71:
		{
			if (edge(w[6], w[8])) {
				PIXEL11_10
			} else {
				PIXEL11_90
			}
			break;
		}
		case 0xc0:
		case 0xc1:
		case 0xc4:
		case 0xc5:
		case 0xe0:
		case 0xe4:
		case 0xe1:
		case 0xc2:
		case 0xc6:
		case 0xe2:
		case 0xc3:
		case 0xca:
		case 0xcb:
		case 0xe3:
		case 0xe6:
		case 0xc7:
		case 0xea:
		case 0xe5:
		case 0xce:
		case 0xcd:
		case 0xc9:
		case 0xe7:
		case 0xcf:
		case 0xed:
		case 0xe9:
		case 0xeb:
		case 0xef:
		{
			PIXEL11_11
			break;
		}
		case 0x56:
		case 0x78:
		case 0x7c:
		case 0x76:
		case 0x7e:
		case 0x7d:
		case 0x77:
		case 0x7b:
		case 0x5f:
		case 0x7f:
		{
			PIXEL11_10
			break;
		}
		case 0xc8:
		case 0xcc:
		case 0xec:
		case 0xe8:
		case 0xee:
		{
			if (edge(w[8], w[4])) {
				PIXEL11_11
			} else {
				PIXEL11_60
			}
			break;
		}
		case 0xd5:
		case 0xd4:
		case 0xf1:
		case 0xf0:
		case 0xdd:
		case 0xf3:
		{
			if (edge(w[6], w[8])) {
				PIXEL11_0
			} else {
				PIXEL11_90
			}
			break;
		}
		case 0x0:
		case 0x1:
		case 0x4:
		case 0x20:
		case 0x80:
		case 0x5:
		case 0x84:
		case 0xa0:
		case 0x21:
		case 0x81:
		case 0x24:
		case 0x85:
		case 0xa4:
		case 0xa1:
		case 0x25:
		case 0xa5:
		case 0x2:
		case 0x22:
		case 0x82:
		case 0xa2:
		case 0x8:
		case 0xc:
		case 0x88:
		case 0x8c:
		case 0x3:
		case 0x23:
		case 0x83:
		case 0xa3:
		case 0x6:
		case 0x26:
		case 0x86:
		case 0xa6:
		case 0x28:
		case 0x2c:
		case 0xa8:
		case 0xac:
		case 0x9:
		case 0xd:
		case 0x89:
		case 0x8d:
		case 0xa:
		case 0x8a:
		case 0x7:
		case 0x27:
		case 0x87:
		case 0x29:
		case 0xa9:
		case 0x2d:
		case 0xb:
		case 0x8b:
		case 0x2a:
		case 0xaa:
		case 0xe:
		case 0x8e:
		case 0xab:
		case 0x2b:
		case 0x8f:
		case 0xf:
		case 0xa7:
		case 0xad:
		case 0xae:
		case 0x2e:
		case 0xaf:
		case 0x2f:
		{
			PIXEL11_20
			break;
		}
		case 0x50:
		case 0x51:
		{
			if (edge(w[6], w[8])) {
				PIXEL11_10
			} else {
				PIXEL11_20
			}
			break;
		}
		case 0x90:
		case 0x91:
		case 0xb0:
		case 0xb1:
		case 0x94:
		case 0x95:
		case 0xb4:
		case 0x98:
		case 0xb8:
		case 0x9c:
		case 0x99:
		case 0x9a:
		case 0x9b:
		case 0xbc:
		case 0xb9:
		case 0x9d:
		case 0x9e:
		case 0xb5:
		case 0xba:
		case 0xb3:
		case 0x93:
		case 0xbd:
		case 0xbb:
		case 0xb7:
		case 0x97:
		case 0x9f:
		case 0xbf:
		{
			PIXEL11_12
			break;
		}
		case 0x40:
		case 0x41:
		case 0x44:
		case 0x45:
		case 0x60:
		case 0x61:
		case 0x64:
		case 0x65:
		case 0x48:
		case 0x4c:
		case 0x42:
		case 0x68:
		case 0x6c:
		case 0x49:
		case 0x4d:
		case 0x43:
		case 0x46:
		case 0x62:
		case 0x4a:
		case 0x6b:
		case 0x6a:
		case 0x4b:
		case 0x63:
		case 0x47:
		case 0x66:
		case 0x4e:
		case 0x6d:
		case 0x69:
		case 0x6e:
		case 0x67:
		case 0x4f:
		case 0x6f:
		{
			PIXEL11_22
			break;
		}
		case 0x92:
		case 0xb2:
		case 0xb6:
		case 0x96:
		case 0xbe:
		{
			if (edge(w[2], w[6])) {
				PIXEL11_12
			} else {
				PIXEL11_61
			}
			break;
		}
		case 0xf5:
		case 0xf4:
		case 0xfc:
		case 0xf6:
		case 0xfe:
		case 0xfd:
		case 0xf7:
		case 0xff:
		{
			if (edge(w[6], w[8])) {
				PIXEL11_0
			} else {
				PIXEL11_100
			}
			break;
		}
		case 0xd0:
		case 0xd1:
		case 0x52:
		case 0xd6:
		case 0x58:
		case 0xf8:
		case 0xd8:
		case 0xd2:
		case 0xd3:
		case 0xd9:
		case 0xdc:
		case 0xf2:
		case 0xda:
		case 0xdb:
		case 0xfa:
		case 0xde:
		case 0xf9:
		case 0xd7:
		case 0xfb:
		case 0xdf:
		{
			if (edge(w[6], w[8])) {
				PIXEL11_0
			} else {
				PIXEL11_20
			}
			break;
		}
		}

		pIn += 1;
		pOut += 2;
	}
}

} // namespace openmsx

