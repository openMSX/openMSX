// $Id$

// 2xSaI is Copyright (c) 1999-2001 by Derek Liauw Kie Fa.
//   http://elektron.its.tudelft.nl/~dalikifa/
// 2xSaI is free under GPL.
//
// Modified for use in openMSX by Maarten ter Huurne.

#include "SaI2xScaler.hh"
#include "openmsx.hh"
#include <cassert>

using std::min;
using std::max;

namespace openmsx {

template <class Pixel>
SaI2xScaler<Pixel>::SaI2xScaler(SDL_PixelFormat* format)
	: blender(Blender<Pixel>::createFromFormat(format))
{
}

const int WIDTH256 = 320; // TODO: Specify this in a clean way.
const int HEIGHT = 480;

template <class Pixel>
void SaI2xScaler<Pixel>::scaleLine256(
	const Pixel* srcLine0, const Pixel* srcLine1,
	const Pixel* srcLine2, const Pixel* srcLine3,
	Pixel* dstUpper, Pixel* dstLower )
{
	// TODO: Scale border pixels as well.
	for (int x = 0; x < WIDTH256; x++) {
		// Map of the pixels:
		//   I|E F|J
		//   G|A B|K
		//   H|C D|L
		//   M|N O|P

		int xl = max(x - 1, 0);
		int xr = min(x + 1, WIDTH256 - 1);
		int xrr = min(x + 2, WIDTH256 - 1);

		// TODO: Possible performance improvements:
		// - Play with order of fetching (effect on data cache).
		// - Try not fetching at all (using srcLineN[x] in algorithm).
		// - Try rotating the fetched colours (either in vars or in array).
		Pixel colorI = srcLine0[xl];
		Pixel colorE = srcLine0[x];
		Pixel colorF = srcLine0[xr];
		Pixel colorJ = srcLine0[xrr];

		Pixel colorG = srcLine1[xl];
		Pixel colorA = srcLine1[x];
		Pixel colorB = srcLine1[xr];
		Pixel colorK = srcLine1[xrr];

		Pixel colorH = srcLine2[xl];
		Pixel colorC = srcLine2[x];
		Pixel colorD = srcLine2[xr];
		Pixel colorL = srcLine2[xrr];

		Pixel colorM = srcLine3[xl];
		Pixel colorN = srcLine3[x];
		Pixel colorO = srcLine3[xr];
		//Pixel colorP = srcLine3[xrr];

		Pixel product, product1, product2;

		if (colorA == colorD && colorB != colorC) {
			product =
				( ( (colorA == colorE && colorB == colorL)
				  || ( colorA == colorC && colorA == colorF
				       && colorB != colorE && colorB == colorJ )
				  )
				? colorA
				: blender.blend(colorA, colorB)
				);
			product1 =
				( ( (colorA == colorG && colorC == colorO)
				  || ( colorA == colorB && colorA == colorH
				       && colorG != colorC && colorC == colorM )
				  )
				? colorA
				: blender.blend(colorA, colorC)
				);
			product2 = colorA;
		} else if (colorB == colorC && colorA != colorD) {
			product =
				( ( (colorB == colorF && colorA == colorH)
				  || ( colorB == colorE && colorB == colorD
				       && colorA != colorF && colorA == colorI )
				  )
				? colorB
				: blender.blend(colorA, colorB)
				);
			product1 =
				( ( (colorC == colorH && colorA == colorF)
				  || ( colorC == colorG && colorC == colorD
				       && colorA != colorH && colorA == colorI )
				  )
				? colorC
				: blender.blend(colorA, colorC)
				);
			product2 = colorB;
		} else if (colorA == colorD && colorB == colorC) {
			if (colorA == colorB) {
				product = product1 = product2 = colorA;
			} else {
				int r = 0;
				if (colorE == colorG) {
					if (colorA == colorE) r--; else if (colorB == colorE) r++;
				}
				if (colorF == colorK) {
					if (colorA == colorF) r--; else if (colorB == colorF) r++;
				}
				if (colorH == colorN) {
					if (colorA == colorH) r--; else if (colorB == colorH) r++;
				}
				if (colorL == colorO) {
					if (colorA == colorL) r--; else if (colorB == colorL) r++;
				}
				product = product1 = blender.blend(colorA, colorB);
				product2 = r > 0 ? colorA : (r < 0 ? colorB : product);
			}
		} else {
			product =
				( colorA == colorC && colorA == colorF
				  && colorB != colorE && colorB == colorJ
				? colorA
				: ( colorB == colorE && colorB == colorD
				    && colorA != colorF && colorA == colorI
				  ? colorB
				  : blender.blend(colorA, colorB)
				  )
				);
			product1 =
				( colorA == colorB && colorA == colorH
				  && colorG != colorC && colorC == colorM
				? colorA
				: ( colorC == colorG && colorC == colorD
				    && colorA != colorH && colorA == colorI
				  ? colorC
				  : blender.blend(colorA, colorC)
				  )
				);
			product2 = blender.blend( // TODO: Quad-blend may be better?
				blender.blend(colorA, colorB),
				blender.blend(colorC, colorD)
				);
		}

		dstUpper[x * 2] = colorA;
		dstUpper[x * 2 + 1] = product;
		dstLower[x * 2] = product1;
		dstLower[x * 2 + 1] = product2;
	}
}

const int WIDTH512 = 640; // TODO: Specify this in a clean way.

template <class Pixel>
void SaI2xScaler<Pixel>::scaleLine512(
	const Pixel* srcLine0, const Pixel* srcLine1,
	const Pixel* srcLine2, const Pixel* srcLine3,
	Pixel* dstUpper, Pixel* dstLower )
{
	// Apply 2xSaI and keep the bottom-left pixel.
	// It's not great, but at least it looks better than doubling the pixel
	// like SimpleScaler does.
	dstUpper[0] = srcLine1[0];
	dstLower[0] = blender.blend(srcLine1[0], srcLine2[0]);
	for (int x = 1; x < WIDTH512 - 1; x++) {
		// Map of the pixels:
		//   I E F
		//   G A B
		//   H C D
		//   M N O

		Pixel colorI = srcLine0[x - 1];
		//Pixel colorE = srcLine0[x];
		Pixel colorF = srcLine0[x + 1];

		Pixel colorG = srcLine1[x - 1];
		Pixel colorA = srcLine1[x];
		Pixel colorB = srcLine1[x + 1];

		Pixel colorH = srcLine2[x - 1];
		Pixel colorC = srcLine2[x];
		Pixel colorD = srcLine2[x + 1];

		Pixel colorM = srcLine3[x - 1];
		//Pixel colorN = srcLine3[x];
		Pixel colorO = srcLine3[x + 1];

		Pixel product1;

		if (colorA == colorD && colorB != colorC) {
			product1 =
				( ( (colorA == colorG && colorC == colorO)
				  || ( colorA == colorB && colorA == colorH
				       && colorG != colorC && colorC == colorM )
				  )
				? colorA
				: blender.blend(colorA, colorC)
				);
		} else if (colorB == colorC && colorA != colorD) {
			product1 =
				( ( (colorC == colorH && colorA == colorF)
				  || ( colorC == colorG && colorC == colorD
				       && colorA != colorH && colorA == colorI )
				  )
				? colorC
				: blender.blend(colorA, colorC)
				);
		} else if (colorA == colorD && colorB == colorC) {
			if (colorA == colorC) {
				product1 = colorA;
			} else {
				product1 = blender.blend(colorA, colorC);
			}
		} else {
			product1 =
				( colorA == colorB && colorA == colorH
				  && colorG != colorC && colorC == colorM
				? colorA
				: ( colorC == colorG && colorC == colorD
				    && colorA != colorH && colorA == colorI
				  ? colorC
				  : blender.blend(colorA, colorC)
				  )
				);
		}

		dstUpper[x] = colorA;
		dstLower[x] = product1;
	}
	dstUpper[WIDTH512 - 1] = srcLine1[WIDTH512 - 1];
	dstLower[WIDTH512 - 1] =
		blender.blend(srcLine1[WIDTH512 - 1], srcLine2[WIDTH512 - 1]);
}

template <class Pixel>
void SaI2xScaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY, SDL_Surface* dst, int dstY )
{
	assert(dst->w == WIDTH256 * 2);
	for (int y = srcY; y < endSrcY; y++) {
		const Pixel* srcLine0 = Scaler<Pixel>::linePtr(src, max(y - 1, srcY));
		const Pixel* srcLine1 = Scaler<Pixel>::linePtr(src, y);
		const Pixel* srcLine2 = Scaler<Pixel>::linePtr(src, min(y + 1, endSrcY - 1));
		const Pixel* srcLine3 = Scaler<Pixel>::linePtr(src, min(y + 2, endSrcY - 1));
		Pixel* dstUpper = Scaler<Pixel>::linePtr(dst, dstY++);
		Pixel* dstLower = Scaler<Pixel>::linePtr(dst, min(dstY++, HEIGHT - 1));

		scaleLine256(
			srcLine0, srcLine1, srcLine2, srcLine3, dstUpper, dstLower );
	}
}

template <class Pixel>
void SaI2xScaler<Pixel>::scale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	assert(dst->w == WIDTH512);
	for (int y = srcY; y < endSrcY; y++) {
		const Pixel* srcLine0 = Scaler<Pixel>::linePtr(src, max(y - 1, srcY));
		const Pixel* srcLine1 = Scaler<Pixel>::linePtr(src, y);
		const Pixel* srcLine2 = Scaler<Pixel>::linePtr(src, min(y + 1, endSrcY - 1));
		const Pixel* srcLine3 = Scaler<Pixel>::linePtr(src, min(y + 2, endSrcY - 1));
		Pixel* dstUpper = Scaler<Pixel>::linePtr(dst, dstY++);
		Pixel* dstLower = Scaler<Pixel>::linePtr(dst, min(dstY++, HEIGHT - 1));

		scaleLine512(
			srcLine0, srcLine1, srcLine2, srcLine3, dstUpper, dstLower );
	}
}


// Force template instantiation.
template class SaI2xScaler<word>;
template class SaI2xScaler<unsigned int>;

} // namespace openmsx

