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

inline Pix16 HQ2xScaler<word>::readPixel(word *pIn) {
	word pixel = *pIn;
	// TODO: Use surface info instead.
	return pixel;
}

inline Pix16 HQ2xScaler<unsigned int>::readPixel(unsigned int* pIn) {
	unsigned int pixel = *pIn;
	// TODO: Use surface info instead.
	int r = (pixel & 0xFF0000) >> (16 + 8 - 5);
	int g = (pixel & 0x00FF00) >> (8 + 8 - 6);
	int b = (pixel & 0x0000FF) >> (8 - 5);
	return (Pix16)((r << 11) | (g << 5) | b);
}

inline void HQ2xScaler<word>::pset(word* pOut, Pix32 colour)
{
	// TODO: Use surface info instead.
	int r = (colour & 0xFF0000) >> (16 + 8 - 5);
	int g = (colour & 0x00FF00) >> (8 + 8 - 6);
	int b = (colour & 0x0000FF) >> (8 - 5);
	*pOut = (word)((r << 11) | (g << 5) | b);
}

inline void HQ2xScaler<unsigned int>::pset(unsigned int* pOut, Pix32 colour)
{
	*pOut = colour;
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
        case 0:
        case 1:
        case 4:
        case 32:
        case 128:
        case 5:
        case 132:
        case 160:
        case 33:
        case 129:
        case 36:
        case 133:
        case 164:
        case 161:
        case 37:
        case 165:
        {
          PIXEL00_20
          PIXEL01_20
          PIXEL10_20
          PIXEL11_20
          break;
        }
        case 2:
        case 34:
        case 130:
        case 162:
        {
          PIXEL00_22
          PIXEL01_21
          PIXEL10_20
          PIXEL11_20
          break;
        }
        case 16:
        case 17:
        case 48:
        case 49:
        {
          PIXEL00_20
          PIXEL01_22
          PIXEL10_20
          PIXEL11_21
          break;
        }
        case 64:
        case 65:
        case 68:
        case 69:
        {
          PIXEL00_20
          PIXEL01_20
          PIXEL10_21
          PIXEL11_22
          break;
        }
        case 8:
        case 12:
        case 136:
        case 140:
        {
          PIXEL00_21
          PIXEL01_20
          PIXEL10_22
          PIXEL11_20
          break;
        }
        case 3:
        case 35:
        case 131:
        case 163:
        {
          PIXEL00_11
          PIXEL01_21
          PIXEL10_20
          PIXEL11_20
          break;
        }
        case 6:
        case 38:
        case 134:
        case 166:
        {
          PIXEL00_22
          PIXEL01_12
          PIXEL10_20
          PIXEL11_20
          break;
        }
        case 20:
        case 21:
        case 52:
        case 53:
        {
          PIXEL00_20
          PIXEL01_11
          PIXEL10_20
          PIXEL11_21
          break;
        }
        case 144:
        case 145:
        case 176:
        case 177:
        {
          PIXEL00_20
          PIXEL01_22
          PIXEL10_20
          PIXEL11_12
          break;
        }
        case 192:
        case 193:
        case 196:
        case 197:
        {
          PIXEL00_20
          PIXEL01_20
          PIXEL10_21
          PIXEL11_11
          break;
        }
        case 96:
        case 97:
        case 100:
        case 101:
        {
          PIXEL00_20
          PIXEL01_20
          PIXEL10_12
          PIXEL11_22
          break;
        }
        case 40:
        case 44:
        case 168:
        case 172:
        {
          PIXEL00_21
          PIXEL01_20
          PIXEL10_11
          PIXEL11_20
          break;
        }
        case 9:
        case 13:
        case 137:
        case 141:
        {
          PIXEL00_12
          PIXEL01_20
          PIXEL10_22
          PIXEL11_20
          break;
        }
        case 18:
        case 50:
        {
          PIXEL00_22
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_20
          PIXEL11_21
          break;
        }
        case 80:
        case 81:
        {
          PIXEL00_20
          PIXEL01_22
          PIXEL10_21
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 72:
        case 76:
        {
          PIXEL00_21
          PIXEL01_20
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_22
          break;
        }
        case 10:
        case 138:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_21
          PIXEL10_22
          PIXEL11_20
          break;
        }
        case 66:
        {
          PIXEL00_22
          PIXEL01_21
          PIXEL10_21
          PIXEL11_22
          break;
        }
        case 24:
        {
          PIXEL00_21
          PIXEL01_22
          PIXEL10_22
          PIXEL11_21
          break;
        }
        case 7:
        case 39:
        case 135:
        {
          PIXEL00_11
          PIXEL01_12
          PIXEL10_20
          PIXEL11_20
          break;
        }
        case 148:
        case 149:
        case 180:
        {
          PIXEL00_20
          PIXEL01_11
          PIXEL10_20
          PIXEL11_12
          break;
        }
        case 224:
        case 228:
        case 225:
        {
          PIXEL00_20
          PIXEL01_20
          PIXEL10_12
          PIXEL11_11
          break;
        }
        case 41:
        case 169:
        case 45:
        {
          PIXEL00_12
          PIXEL01_20
          PIXEL10_11
          PIXEL11_20
          break;
        }
        case 22:
        case 54:
        {
          PIXEL00_22
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_20
          PIXEL11_21
          break;
        }
        case 208:
        case 209:
        {
          PIXEL00_20
          PIXEL01_22
          PIXEL10_21
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 104:
        case 108:
        {
          PIXEL00_21
          PIXEL01_20
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_22
          break;
        }
        case 11:
        case 139:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_21
          PIXEL10_22
          PIXEL11_20
          break;
        }
        case 19:
        case 51:
        {
          if (edge(w[2], w[6]))
          {
            PIXEL00_11
            PIXEL01_10
          }
          else
          {
            PIXEL00_60
            PIXEL01_90
          }
          PIXEL10_20
          PIXEL11_21
          break;
        }
        case 146:
        case 178:
        {
          PIXEL00_22
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
            PIXEL11_12
          }
          else
          {
            PIXEL01_90
            PIXEL11_61
          }
          PIXEL10_20
          break;
        }
        case 84:
        case 85:
        {
          PIXEL00_20
          if (edge(w[6], w[8]))
          {
            PIXEL01_11
            PIXEL11_10
          }
          else
          {
            PIXEL01_60
            PIXEL11_90
          }
          PIXEL10_21
          break;
        }
        case 112:
        case 113:
        {
          PIXEL00_20
          PIXEL01_22
          if (edge(w[6], w[8]))
          {
            PIXEL10_12
            PIXEL11_10
          }
          else
          {
            PIXEL10_61
            PIXEL11_90
          }
          break;
        }
        case 200:
        case 204:
        {
          PIXEL00_21
          PIXEL01_20
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
            PIXEL11_11
          }
          else
          {
            PIXEL10_90
            PIXEL11_60
          }
          break;
        }
        case 73:
        case 77:
        {
          if (edge(w[8], w[4]))
          {
            PIXEL00_12
            PIXEL10_10
          }
          else
          {
            PIXEL00_61
            PIXEL10_90
          }
          PIXEL01_20
          PIXEL11_22
          break;
        }
        case 42:
        case 170:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
            PIXEL10_11
          }
          else
          {
            PIXEL00_90
            PIXEL10_60
          }
          PIXEL01_21
          PIXEL11_20
          break;
        }
        case 14:
        case 142:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
            PIXEL01_12
          }
          else
          {
            PIXEL00_90
            PIXEL01_61
          }
          PIXEL10_22
          PIXEL11_20
          break;
        }
        case 67:
        {
          PIXEL00_11
          PIXEL01_21
          PIXEL10_21
          PIXEL11_22
          break;
        }
        case 70:
        {
          PIXEL00_22
          PIXEL01_12
          PIXEL10_21
          PIXEL11_22
          break;
        }
        case 28:
        {
          PIXEL00_21
          PIXEL01_11
          PIXEL10_22
          PIXEL11_21
          break;
        }
        case 152:
        {
          PIXEL00_21
          PIXEL01_22
          PIXEL10_22
          PIXEL11_12
          break;
        }
        case 194:
        {
          PIXEL00_22
          PIXEL01_21
          PIXEL10_21
          PIXEL11_11
          break;
        }
        case 98:
        {
          PIXEL00_22
          PIXEL01_21
          PIXEL10_12
          PIXEL11_22
          break;
        }
        case 56:
        {
          PIXEL00_21
          PIXEL01_22
          PIXEL10_11
          PIXEL11_21
          break;
        }
        case 25:
        {
          PIXEL00_12
          PIXEL01_22
          PIXEL10_22
          PIXEL11_21
          break;
        }
        case 26:
        case 31:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_22
          PIXEL11_21
          break;
        }
        case 82:
        case 214:
        {
          PIXEL00_22
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_21
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 88:
        case 248:
        {
          PIXEL00_21
          PIXEL01_22
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 74:
        case 107:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_21
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_22
          break;
        }
        case 27:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_10
          PIXEL10_22
          PIXEL11_21
          break;
        }
        case 86:
        {
          PIXEL00_22
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_21
          PIXEL11_10
          break;
        }
        case 216:
        {
          PIXEL00_21
          PIXEL01_22
          PIXEL10_10
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 106:
        {
          PIXEL00_10
          PIXEL01_21
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_22
          break;
        }
        case 30:
        {
          PIXEL00_10
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_22
          PIXEL11_21
          break;
        }
        case 210:
        {
          PIXEL00_22
          PIXEL01_10
          PIXEL10_21
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 120:
        {
          PIXEL00_21
          PIXEL01_22
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_10
          break;
        }
        case 75:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_21
          PIXEL10_10
          PIXEL11_22
          break;
        }
        case 29:
        {
          PIXEL00_12
          PIXEL01_11
          PIXEL10_22
          PIXEL11_21
          break;
        }
        case 198:
        {
          PIXEL00_22
          PIXEL01_12
          PIXEL10_21
          PIXEL11_11
          break;
        }
        case 184:
        {
          PIXEL00_21
          PIXEL01_22
          PIXEL10_11
          PIXEL11_12
          break;
        }
        case 99:
        {
          PIXEL00_11
          PIXEL01_21
          PIXEL10_12
          PIXEL11_22
          break;
        }
        case 57:
        {
          PIXEL00_12
          PIXEL01_22
          PIXEL10_11
          PIXEL11_21
          break;
        }
        case 71:
        {
          PIXEL00_11
          PIXEL01_12
          PIXEL10_21
          PIXEL11_22
          break;
        }
        case 156:
        {
          PIXEL00_21
          PIXEL01_11
          PIXEL10_22
          PIXEL11_12
          break;
        }
        case 226:
        {
          PIXEL00_22
          PIXEL01_21
          PIXEL10_12
          PIXEL11_11
          break;
        }
        case 60:
        {
          PIXEL00_21
          PIXEL01_11
          PIXEL10_11
          PIXEL11_21
          break;
        }
        case 195:
        {
          PIXEL00_11
          PIXEL01_21
          PIXEL10_21
          PIXEL11_11
          break;
        }
        case 102:
        {
          PIXEL00_22
          PIXEL01_12
          PIXEL10_12
          PIXEL11_22
          break;
        }
        case 153:
        {
          PIXEL00_12
          PIXEL01_22
          PIXEL10_22
          PIXEL11_12
          break;
        }
        case 58:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          PIXEL10_11
          PIXEL11_21
          break;
        }
        case 83:
        {
          PIXEL00_11
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          PIXEL10_21
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 92:
        {
          PIXEL00_21
          PIXEL01_11
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 202:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          PIXEL01_21
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          PIXEL11_11
          break;
        }
        case 78:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          PIXEL01_12
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          PIXEL11_22
          break;
        }
        case 154:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          PIXEL10_22
          PIXEL11_12
          break;
        }
        case 114:
        {
          PIXEL00_22
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          PIXEL10_12
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 89:
        {
          PIXEL00_12
          PIXEL01_22
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 90:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 55:
        case 23:
        {
          if (edge(w[2], w[6]))
          {
            PIXEL00_11
            PIXEL01_0
          }
          else
          {
            PIXEL00_60
            PIXEL01_90
          }
          PIXEL10_20
          PIXEL11_21
          break;
        }
        case 182:
        case 150:
        {
          PIXEL00_22
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
            PIXEL11_12
          }
          else
          {
            PIXEL01_90
            PIXEL11_61
          }
          PIXEL10_20
          break;
        }
        case 213:
        case 212:
        {
          PIXEL00_20
          if (edge(w[6], w[8]))
          {
            PIXEL01_11
            PIXEL11_0
          }
          else
          {
            PIXEL01_60
            PIXEL11_90
          }
          PIXEL10_21
          break;
        }
        case 241:
        case 240:
        {
          PIXEL00_20
          PIXEL01_22
          if (edge(w[6], w[8]))
          {
            PIXEL10_12
            PIXEL11_0
          }
          else
          {
            PIXEL10_61
            PIXEL11_90
          }
          break;
        }
        case 236:
        case 232:
        {
          PIXEL00_21
          PIXEL01_20
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
            PIXEL11_11
          }
          else
          {
            PIXEL10_90
            PIXEL11_60
          }
          break;
        }
        case 109:
        case 105:
        {
          if (edge(w[8], w[4]))
          {
            PIXEL00_12
            PIXEL10_0
          }
          else
          {
            PIXEL00_61
            PIXEL10_90
          }
          PIXEL01_20
          PIXEL11_22
          break;
        }
        case 171:
        case 43:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
            PIXEL10_11
          }
          else
          {
            PIXEL00_90
            PIXEL10_60
          }
          PIXEL01_21
          PIXEL11_20
          break;
        }
        case 143:
        case 15:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
            PIXEL01_12
          }
          else
          {
            PIXEL00_90
            PIXEL01_61
          }
          PIXEL10_22
          PIXEL11_20
          break;
        }
        case 124:
        {
          PIXEL00_21
          PIXEL01_11
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_10
          break;
        }
        case 203:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_21
          PIXEL10_10
          PIXEL11_11
          break;
        }
        case 62:
        {
          PIXEL00_10
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_11
          PIXEL11_21
          break;
        }
        case 211:
        {
          PIXEL00_11
          PIXEL01_10
          PIXEL10_21
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 118:
        {
          PIXEL00_22
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_12
          PIXEL11_10
          break;
        }
        case 217:
        {
          PIXEL00_12
          PIXEL01_22
          PIXEL10_10
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 110:
        {
          PIXEL00_10
          PIXEL01_12
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_22
          break;
        }
        case 155:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_10
          PIXEL10_22
          PIXEL11_12
          break;
        }
        case 188:
        {
          PIXEL00_21
          PIXEL01_11
          PIXEL10_11
          PIXEL11_12
          break;
        }
        case 185:
        {
          PIXEL00_12
          PIXEL01_22
          PIXEL10_11
          PIXEL11_12
          break;
        }
        case 61:
        {
          PIXEL00_12
          PIXEL01_11
          PIXEL10_11
          PIXEL11_21
          break;
        }
        case 157:
        {
          PIXEL00_12
          PIXEL01_11
          PIXEL10_22
          PIXEL11_12
          break;
        }
        case 103:
        {
          PIXEL00_11
          PIXEL01_12
          PIXEL10_12
          PIXEL11_22
          break;
        }
        case 227:
        {
          PIXEL00_11
          PIXEL01_21
          PIXEL10_12
          PIXEL11_11
          break;
        }
        case 230:
        {
          PIXEL00_22
          PIXEL01_12
          PIXEL10_12
          PIXEL11_11
          break;
        }
        case 199:
        {
          PIXEL00_11
          PIXEL01_12
          PIXEL10_21
          PIXEL11_11
          break;
        }
        case 220:
        {
          PIXEL00_21
          PIXEL01_11
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 158:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_22
          PIXEL11_12
          break;
        }
        case 234:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          PIXEL01_21
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_11
          break;
        }
        case 242:
        {
          PIXEL00_22
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          PIXEL10_12
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 59:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          PIXEL10_11
          PIXEL11_21
          break;
        }
        case 121:
        {
          PIXEL00_12
          PIXEL01_22
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 87:
        {
          PIXEL00_11
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_21
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 79:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_12
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          PIXEL11_22
          break;
        }
        case 122:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 94:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 218:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 91:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 229:
        {
          PIXEL00_20
          PIXEL01_20
          PIXEL10_12
          PIXEL11_11
          break;
        }
        case 167:
        {
          PIXEL00_11
          PIXEL01_12
          PIXEL10_20
          PIXEL11_20
          break;
        }
        case 173:
        {
          PIXEL00_12
          PIXEL01_20
          PIXEL10_11
          PIXEL11_20
          break;
        }
        case 181:
        {
          PIXEL00_20
          PIXEL01_11
          PIXEL10_20
          PIXEL11_12
          break;
        }
        case 186:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          PIXEL10_11
          PIXEL11_12
          break;
        }
        case 115:
        {
          PIXEL00_11
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          PIXEL10_12
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 93:
        {
          PIXEL00_12
          PIXEL01_11
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 206:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          PIXEL01_12
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          PIXEL11_11
          break;
        }
        case 205:
        case 201:
        {
          PIXEL00_12
          PIXEL01_20
          if (edge(w[8], w[4]))
          {
            PIXEL10_10
          }
          else
          {
            PIXEL10_70
          }
          PIXEL11_11
          break;
        }
        case 174:
        case 46:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_10
          }
          else
          {
            PIXEL00_70
          }
          PIXEL01_12
          PIXEL10_11
          PIXEL11_20
          break;
        }
        case 179:
        case 147:
        {
          PIXEL00_11
          if (edge(w[2], w[6]))
          {
            PIXEL01_10
          }
          else
          {
            PIXEL01_70
          }
          PIXEL10_20
          PIXEL11_12
          break;
        }
        case 117:
        case 116:
        {
          PIXEL00_20
          PIXEL01_11
          PIXEL10_12
          if (edge(w[6], w[8]))
          {
            PIXEL11_10
          }
          else
          {
            PIXEL11_70
          }
          break;
        }
        case 189:
        {
          PIXEL00_12
          PIXEL01_11
          PIXEL10_11
          PIXEL11_12
          break;
        }
        case 231:
        {
          PIXEL00_11
          PIXEL01_12
          PIXEL10_12
          PIXEL11_11
          break;
        }
        case 126:
        {
          PIXEL00_10
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_10
          break;
        }
        case 219:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_10
          PIXEL10_10
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 125:
        {
          if (edge(w[8], w[4]))
          {
            PIXEL00_12
            PIXEL10_0
          }
          else
          {
            PIXEL00_61
            PIXEL10_90
          }
          PIXEL01_11
          PIXEL11_10
          break;
        }
        case 221:
        {
          PIXEL00_12
          if (edge(w[6], w[8]))
          {
            PIXEL01_11
            PIXEL11_0
          }
          else
          {
            PIXEL01_60
            PIXEL11_90
          }
          PIXEL10_10
          break;
        }
        case 207:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
            PIXEL01_12
          }
          else
          {
            PIXEL00_90
            PIXEL01_61
          }
          PIXEL10_10
          PIXEL11_11
          break;
        }
        case 238:
        {
          PIXEL00_10
          PIXEL01_12
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
            PIXEL11_11
          }
          else
          {
            PIXEL10_90
            PIXEL11_60
          }
          break;
        }
        case 190:
        {
          PIXEL00_10
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
            PIXEL11_12
          }
          else
          {
            PIXEL01_90
            PIXEL11_61
          }
          PIXEL10_11
          break;
        }
        case 187:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
            PIXEL10_11
          }
          else
          {
            PIXEL00_90
            PIXEL10_60
          }
          PIXEL01_10
          PIXEL11_12
          break;
        }
        case 243:
        {
          PIXEL00_11
          PIXEL01_10
          if (edge(w[6], w[8]))
          {
            PIXEL10_12
            PIXEL11_0
          }
          else
          {
            PIXEL10_61
            PIXEL11_90
          }
          break;
        }
        case 119:
        {
          if (edge(w[2], w[6]))
          {
            PIXEL00_11
            PIXEL01_0
          }
          else
          {
            PIXEL00_60
            PIXEL01_90
          }
          PIXEL10_12
          PIXEL11_10
          break;
        }
        case 237:
        case 233:
        {
          PIXEL00_12
          PIXEL01_20
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_100
          }
          PIXEL11_11
          break;
        }
        case 175:
        case 47:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_100
          }
          PIXEL01_12
          PIXEL10_11
          PIXEL11_20
          break;
        }
        case 183:
        case 151:
        {
          PIXEL00_11
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_100
          }
          PIXEL10_20
          PIXEL11_12
          break;
        }
        case 245:
        case 244:
        {
          PIXEL00_20
          PIXEL01_11
          PIXEL10_12
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_100
          }
          break;
        }
        case 250:
        {
          PIXEL00_10
          PIXEL01_10
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 123:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_10
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_10
          break;
        }
        case 95:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_10
          PIXEL11_10
          break;
        }
        case 222:
        {
          PIXEL00_10
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_10
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 252:
        {
          PIXEL00_21
          PIXEL01_11
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_100
          }
          break;
        }
        case 249:
        {
          PIXEL00_12
          PIXEL01_22
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_100
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 235:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_21
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_100
          }
          PIXEL11_11
          break;
        }
        case 111:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_100
          }
          PIXEL01_12
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_22
          break;
        }
        case 63:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_100
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_11
          PIXEL11_21
          break;
        }
        case 159:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_100
          }
          PIXEL10_22
          PIXEL11_12
          break;
        }
        case 215:
        {
          PIXEL00_11
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_100
          }
          PIXEL10_21
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 246:
        {
          PIXEL00_22
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          PIXEL10_12
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_100
          }
          break;
        }
        case 254:
        {
          PIXEL00_10
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_100
          }
          break;
        }
        case 253:
        {
          PIXEL00_12
          PIXEL01_11
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_100
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_100
          }
          break;
        }
        case 251:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          PIXEL01_10
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_100
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 239:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_100
          }
          PIXEL01_12
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_100
          }
          PIXEL11_11
          break;
        }
        case 127:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_100
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_20
          }
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_20
          }
          PIXEL11_10
          break;
        }
        case 191:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_100
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_100
          }
          PIXEL10_11
          PIXEL11_12
          break;
        }
        case 223:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_20
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_100
          }
          PIXEL10_10
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_20
          }
          break;
        }
        case 247:
        {
          PIXEL00_11
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_100
          }
          PIXEL10_12
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_100
          }
          break;
        }
        case 255:
        {
          if (edge(w[4], w[2]))
          {
            PIXEL00_0
          }
          else
          {
            PIXEL00_100
          }
          if (edge(w[2], w[6]))
          {
            PIXEL01_0
          }
          else
          {
            PIXEL01_100
          }
          if (edge(w[8], w[4]))
          {
            PIXEL10_0
          }
          else
          {
            PIXEL10_100
          }
          if (edge(w[6], w[8]))
          {
            PIXEL11_0
          }
          else
          {
            PIXEL11_100
          }
          break;
        }
      }

		pIn += 1;
		pOut += 2;
	}
}

} // namespace openmsx

