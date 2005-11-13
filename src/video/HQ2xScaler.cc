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
#include <algorithm>
#include <cassert>

using std::min;

namespace openmsx {

template <class Pixel>
HQ2xScaler<Pixel>::HQ2xScaler(SDL_PixelFormat* format)
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
		if (edge(c5, c1)) pattern |= 0x01;
		if (edge(c5, c2)) pattern |= 0x02;
		if (edge(c5, c3)) pattern |= 0x04;
		if (edge(c5, c4)) pattern |= 0x08;
		if (edge(c5, c6)) pattern |= 0x10;
		if (edge(c5, c7)) pattern |= 0x20;
		if (edge(c5, c8)) pattern |= 0x40;
		if (edge(c5, c9)) pattern |= 0x80;

		unsigned pixel1, pixel2, pixel3, pixel4;
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
			// manually inline this case:
			//   default inline heuristics of gcc don't inline this
			//   code. But because this case is so common and
			//   inlining really does improve speed, we manually
			//   inline it.
			//pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			//pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			//pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			//pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			pixel1 = (2 * c5 + c4 + c2) / 4;
			pixel2 = (2 * c5 + c2 + c6) / 4;
			pixel3 = (2 * c5 + c8 + c4) / 4;
			pixel4 = (2 * c5 + c6 + c8) / 4;
			break;

		case 2:
		case 34:
		case 130:
		case 162:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 16:
		case 17:
		case 48:
		case 49:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 64:
		case 65:
		case 68:
		case 69:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 8:
		case 12:
		case 136:
		case 140:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 3:
		case 35:
		case 131:
		case 163:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 6:
		case 38:
		case 134:
		case 166:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 20:
		case 21:
		case 52:
		case 53:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 144:
		case 145:
		case 176:
		case 177:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 192:
		case 193:
		case 196:
		case 197:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 96:
		case 97:
		case 100:
		case 101:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 40:
		case 44:
		case 168:
		case 172:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 9:
		case 13:
		case 137:
		case 141:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 18:
		case 50:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 80:
		case 81:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 72:
		case 76:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 10:
		case 138:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 66:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 24:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 7:
		case 39:
		case 135:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 148:
		case 149:
		case 180:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 224:
		case 228:
		case 225:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 41:
		case 169:
		case 45:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 22:
		case 54:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 208:
		case 209:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 104:
		case 108:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 11:
		case 139:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 19:
		case 51:
			pixel1 = edge(c2, c6) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c2, c4);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<2, 3, 3>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 146:
		case 178:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<2, 3, 3>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = edge(c2, c6) ? interpolate<3, 1>(c5, c8)
			                      : interpolate<5, 2, 1>(c5, c6, c8);
			break;

		case 84:
		case 85:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = edge(c6, c8) ? interpolate<3, 1>(c5, c2)
			                      : interpolate<5, 2, 1>(c5, c6, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<2, 3, 3>(c5, c6, c8);
			break;

		case 112:
		case 113:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = edge(c6, c8) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<2, 3, 3>(c5, c6, c8);
			break;

		case 200:
		case 204:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			pixel4 = edge(c8, c4) ? interpolate<3, 1>(c5, c6)
				              : interpolate<5, 2, 1>(c5, c8, c6);
			break;

		case 73:
		case 77:
			pixel1 = edge(c8, c4) ? interpolate<3, 1>(c5, c2)
			                      : interpolate<5, 2, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 42:
		case 170:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = edge(c4, c2) ? interpolate<3, 1>(c5, c8)
			                      : interpolate<5, 2, 1>(c5, c4, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 14:
		case 142:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel2 = edge(c4, c2) ? interpolate<3, 1>(c5, c6)
			                      : interpolate<5, 2, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 67:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 70:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 28:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 152:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 194:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 98:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 56:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 25:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 26:
		case 31:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 82:
		case 214:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 88:
		case 248:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 74:
		case 107:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 27:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c3);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 86:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = interpolate<3, 1>(c5, c9);
			break;

		case 216:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 106:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 30:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 210:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = interpolate<3, 1>(c5, c3);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 120:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c9);
			break;

		case 75:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<3, 1>(c5, c7);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 29:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 198:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 184:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 99:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 57:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 71:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 156:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 226:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 60:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 195:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 102:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 153:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 58:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 83:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 92:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 202:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 78:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 154:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 114:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 89:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 90:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 55:
		case 23:
			pixel1 = edge(c2, c6) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c2, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 3, 3>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 182:
		case 150:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 3, 3>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = edge(c2, c6) ? interpolate<3, 1>(c5, c8)
			                      : interpolate<5, 2, 1>(c5, c6, c8);
			break;

		case 213:
		case 212:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = edge(c6, c8) ? interpolate<3, 1>(c5, c2)
			                      : interpolate<5, 2, 1>(c5, c6, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 3, 3>(c5, c6, c8);
			break;

		case 241:
		case 240:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = edge(c6, c8) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 3, 3>(c5, c6, c8);
			break;

		case 236:
		case 232:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			pixel4 = edge(c8, c4) ? interpolate<3, 1>(c5, c6)
			                      : interpolate<5, 2, 1>(c5, c8, c6);
			break;

		case 109:
		case 105:
			pixel1 = edge(c8, c4) ? interpolate<3, 1>(c5, c2)
			                      : interpolate<5, 2, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 171:
		case 43:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = edge(c4, c2) ? interpolate<3, 1>(c5, c8)
			                      : interpolate<5, 2, 1>(c5, c4, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 143:
		case 15:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel2 = edge(c4, c2) ? interpolate<3, 1>(c5, c6)
			                      : interpolate<5, 2, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 124:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c9);
			break;

		case 203:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<3, 1>(c5, c7);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 62:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 211:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<3, 1>(c5, c3);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 118:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<3, 1>(c5, c9);
			break;

		case 217:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 110:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 155:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c3);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 188:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 185:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 61:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 157:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 103:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 227:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 230:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 199:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 220:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 158:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 234:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 242:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 59:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 121:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 87:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 79:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 122:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 94:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 218:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 91:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 229:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 167:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 173:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 181:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 186:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 115:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 93:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 206:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 205:
		case 201:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 174:
		case 46:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 179:
		case 147:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = edge(c2, c6) ? interpolate<3, 1>(c5, c3)
			                      : interpolate<6, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 117:
		case 116:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = edge(c6, c8) ? interpolate<3, 1>(c5, c9)
			                      : interpolate<6, 1, 1>(c5, c6, c8);
			break;

		case 189:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 231:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 126:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c9);
			break;

		case 219:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c3);
			pixel3 = interpolate<3, 1>(c5, c7);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 125:
			pixel1 = edge(c8, c4) ? interpolate<3, 1>(c5, c2)
			                      : interpolate<5, 2, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c9);
			break;

		case 221:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = edge(c6, c8) ? interpolate<3, 1>(c5, c2)
			                      : interpolate<5, 2, 1>(c5, c6, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 3, 3>(c5, c6, c8);
			break;

		case 207:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel2 = edge(c4, c2) ? interpolate<3, 1>(c5, c6)
			                      : interpolate<5, 2, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c7);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 238:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			pixel4 = edge(c8, c4) ? interpolate<3, 1>(c5, c6)
			                      : interpolate<5, 2, 1>(c5, c8, c6);
			break;

		case 190:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 3, 3>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = edge(c2, c6) ? interpolate<3, 1>(c5, c8)
			                      : interpolate<5, 2, 1>(c5, c6, c8);
			break;

		case 187:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c3);
			pixel3 = edge(c4, c2) ? interpolate<3, 1>(c5, c8)
			                      : interpolate<5, 2, 1>(c5, c4, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 243:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = interpolate<3, 1>(c5, c3);
			pixel3 = edge(c6, c8) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 3, 3>(c5, c6, c8);
			break;

		case 119:
			pixel1 = edge(c2, c6) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c2, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 3, 3>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = interpolate<3, 1>(c5, c9);
			break;

		case 237:
		case 233:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 175:
		case 47:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 183:
		case 151:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<14, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 245:
		case 244:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<14, 1, 1>(c5, c6, c8);
			break;

		case 250:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel2 = interpolate<3, 1>(c5, c3);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 123:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c3);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c9);
			break;

		case 95:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c7);
			pixel4 = interpolate<3, 1>(c5, c9);
			break;

		case 222:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c7);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 252:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<14, 1, 1>(c5, c6, c8);
			break;

		case 249:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 235:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<2, 1, 1>(c5, c3, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 111:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c6);
			break;

		case 63:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<2, 1, 1>(c5, c9, c8);
			break;

		case 159:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<14, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 215:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<14, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 246:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<14, 1, 1>(c5, c6, c8);
			break;

		case 254:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<14, 1, 1>(c5, c6, c8);
			break;

		case 253:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel2 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<14, 1, 1>(c5, c6, c8);
			break;

		case 251:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c3);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 239:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel2 = interpolate<3, 1>(c5, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c6);
			break;

		case 127:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<2, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			pixel4 = interpolate<3, 1>(c5, c9);
			break;

		case 191:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<14, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c8);
			pixel4 = interpolate<3, 1>(c5, c8);
			break;

		case 223:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<14, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c7);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<2, 1, 1>(c5, c6, c8);
			break;

		case 247:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<14, 1, 1>(c5, c2, c6);
			pixel3 = interpolate<3, 1>(c5, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<14, 1, 1>(c5, c6, c8);
			break;

		case 255:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel2 = edge(c2, c6) ? c5
			                      : interpolate<14, 1, 1>(c5, c2, c6);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			pixel4 = edge(c6, c8) ? c5
			                      : interpolate<14, 1, 1>(c5, c6, c8);
			break;

		default:
			assert(false);
			pixel1 = pixel2 = pixel3 = pixel4 = 0; // avoid warning
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

		byte pattern = 0;
		if (edge(c5, c1)) pattern |= 0x01;
		if (edge(c5, c2)) pattern |= 0x02;
		if (edge(c5, c3)) pattern |= 0x04;
		if (edge(c5, c4)) pattern |= 0x08;
		if (edge(c5, c6)) pattern |= 0x10;
		if (edge(c5, c7)) pattern |= 0x20;
		if (edge(c5, c8)) pattern |= 0x40;
		if (edge(c5, c9)) pattern |= 0x80;

		unsigned pixel1, pixel3;
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
			// manually inline this case:
			//   default inline heuristics of gcc don't inline this
			//   code. But because this case is so common and
			//   inlining really does improve speed, we manually
			//   inline it.
			//pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			//pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			pixel1 = (2 * c5 + c4 + c2) / 4;
			pixel3 = (2 * c5 + c8 + c4) / 4;
			break;

		case 2:
		case 34:
		case 130:
		case 162:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 16:
		case 17:
		case 48:
		case 49:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 64:
		case 65:
		case 68:
		case 69:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 8:
		case 12:
		case 136:
		case 140:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 3:
		case 35:
		case 131:
		case 163:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 6:
		case 38:
		case 134:
		case 166:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 20:
		case 21:
		case 52:
		case 53:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 144:
		case 145:
		case 176:
		case 177:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 192:
		case 193:
		case 196:
		case 197:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 96:
		case 97:
		case 100:
		case 101:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 40:
		case 44:
		case 168:
		case 172:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 9:
		case 13:
		case 137:
		case 141:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 18:
		case 50:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 80:
		case 81:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 72:
		case 76:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 10:
		case 138:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 66:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 24:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 7:
		case 39:
		case 135:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 148:
		case 149:
		case 180:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 224:
		case 228:
		case 225:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 41:
		case 169:
		case 45:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 22:
		case 54:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 208:
		case 209:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 104:
		case 108:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 11:
		case 139:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 19:
		case 51:
			pixel1 = edge(c2, c6) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c2, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 146:
		case 178:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 84:
		case 85:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 112:
		case 113:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = edge(c6, c8) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c8, c4);
			break;

		case 200:
		case 204:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			break;

		case 73:
		case 77:
			pixel1 = edge(c8, c4) ? interpolate<3, 1>(c5, c2)
			                      : interpolate<5, 2, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			break;

		case 42:
		case 170:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel3 = edge(c4, c2) ? interpolate<3, 1>(c5, c8)
			                      : interpolate<5, 2, 1>(c5, c4, c8);
			break;

		case 14:
		case 142:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 67:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 70:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 28:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 152:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 194:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 98:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 56:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 25:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 26:
		case 31:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 82:
		case 214:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 88:
		case 248:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 74:
		case 107:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 27:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 86:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 216:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			break;

		case 106:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 30:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 210:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 120:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 75:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			break;

		case 29:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 198:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 184:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 99:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 57:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 71:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 156:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 226:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 60:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 195:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 102:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 153:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 58:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 83:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 92:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 202:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 78:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 154:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 114:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 89:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 90:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 55:
		case 23:
			pixel1 = edge(c2, c6) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c2, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 182:
		case 150:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 213:
		case 212:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 241:
		case 240:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = edge(c6, c8) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c8, c4);
			break;

		case 236:
		case 232:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			break;

		case 109:
		case 105:
			pixel1 = edge(c8, c4) ? interpolate<3, 1>(c5, c2)
			                      : interpolate<5, 2, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			break;

		case 171:
		case 43:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel3 = edge(c4, c2) ? interpolate<3, 1>(c5, c8)
			                      : interpolate<5, 2, 1>(c5, c4, c8);
			break;

		case 143:
		case 15:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 124:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 203:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			break;

		case 62:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 211:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 118:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 217:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			break;

		case 110:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 155:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 188:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 185:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 61:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 157:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 103:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 227:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 230:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 199:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 220:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 158:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 234:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 242:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 59:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 121:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 87:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 79:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 122:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 94:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 218:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 91:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 229:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 167:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 173:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 181:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 186:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 115:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 93:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 206:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 205:
		case 201:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? interpolate<3, 1>(c5, c7)
			                      : interpolate<6, 1, 1>(c5, c8, c4);
			break;

		case 174:
		case 46:
			pixel1 = edge(c4, c2) ? interpolate<3, 1>(c5, c1)
			                      : interpolate<6, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 179:
		case 147:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 117:
		case 116:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 189:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 231:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 126:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 219:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			break;

		case 125:
			pixel1 = edge(c8, c4) ? interpolate<3, 1>(c5, c2)
			                      : interpolate<5, 2, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			break;

		case 221:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			break;

		case 207:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			break;

		case 238:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 3, 3>(c5, c8, c4);
			break;

		case 190:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 187:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 3, 3>(c5, c4, c2);
			pixel3 = edge(c4, c2) ? interpolate<3, 1>(c5, c8)
			                      : interpolate<5, 2, 1>(c5, c4, c8);
			break;

		case 243:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = edge(c6, c8) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c8, c4);
			break;

		case 119:
			pixel1 = edge(c2, c6) ? interpolate<3, 1>(c5, c4)
			                      : interpolate<5, 2, 1>(c5, c2, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 237:
		case 233:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			break;

		case 175:
		case 47:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 183:
		case 151:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 245:
		case 244:
			pixel1 = interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 250:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 123:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 95:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			break;

		case 222:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel3 = interpolate<3, 1>(c5, c7);
			break;

		case 252:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 249:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			break;

		case 235:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			break;

		case 111:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 63:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 159:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c8);
			break;

		case 215:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<2, 1, 1>(c5, c7, c4);
			break;

		case 246:
			pixel1 = interpolate<2, 1, 1>(c5, c1, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 254:
			pixel1 = interpolate<3, 1>(c5, c1);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 253:
			pixel1 = interpolate<3, 1>(c5, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			break;

		case 251:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			break;

		case 239:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			break;

		case 127:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<2, 1, 1>(c5, c8, c4);
			break;

		case 191:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c8);
			break;

		case 223:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<2, 1, 1>(c5, c4, c2);
			pixel3 = interpolate<3, 1>(c5, c7);
			break;

		case 247:
			pixel1 = interpolate<3, 1>(c5, c4);
			pixel3 = interpolate<3, 1>(c5, c4);
			break;

		case 255:
			pixel1 = edge(c4, c2) ? c5
			                      : interpolate<14, 1, 1>(c5, c4, c2);
			pixel3 = edge(c8, c4) ? c5
			                      : interpolate<14, 1, 1>(c5, c8, c4);
			break;

		default:
			assert(false);
			pixel1 = pixel3 = 0; // avoid warning
		}
		pset(out1++, pixel3);
		pset(out0++, pixel1);
	}
}

template <class Pixel>
void HQ2xScaler<Pixel>::scale1x1to2x2(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	unsigned prevY = srcStartY;
	for (unsigned dstY = dstStartY; dstY < dstEndY; dstY += 2) {
		Pixel* const dummy = 0;
		const Pixel* srcPrev = src.getLinePtr(prevY, dummy);
		const Pixel* srcCurr = src.getLinePtr(srcStartY, dummy);
		const Pixel* srcNext = src.getLinePtr(
			min(srcStartY + 1, srcEndY - 1), dummy);
		Pixel* dstUpper = dst.getLinePtr(dstY, dummy);
		Pixel* dstLower = dst.getLinePtr(
			min(dstY + 1, dstEndY - 1), dummy);
		scaleLine256(srcPrev, srcCurr, srcNext, dstUpper, dstLower);
		prevY = srcStartY;
		++srcStartY;
	}
}

template <class Pixel>
void HQ2xScaler<Pixel>::scale1x1to1x2(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	unsigned prevY = srcStartY;
	for (unsigned dstY = dstStartY; dstY < dstEndY; dstY += 2) {
		Pixel* dummy = 0;
		const Pixel* srcPrev = src.getLinePtr(prevY,  dummy);
		const Pixel* srcCurr = src.getLinePtr(srcStartY, dummy);
		const Pixel* srcNext = src.getLinePtr(
			min(srcStartY + 1, srcEndY - 1), dummy);
		Pixel* dstUpper = dst.getLinePtr(dstY, dummy);
		Pixel* dstLower = dst.getLinePtr(
			min(dstY + 1, dstEndY - 1), dummy);
		scaleLine512(srcPrev, srcCurr, srcNext, dstUpper, dstLower);
		prevY = srcStartY;
		++srcStartY;
	}
}


// Force template instantiation.
template class HQ2xScaler<word>;
template class HQ2xScaler<unsigned>;

} // namespace openmsx
