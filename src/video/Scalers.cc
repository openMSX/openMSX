// $Id$

#include "Scalers.hh"
#include "openmsx.hh"
#include <cassert>


namespace openmsx {

template <class Pixel>
Scaler<Pixel>::Scaler()
{
}

// Force template instantiation.
template class Scaler<byte>;
template class Scaler<word>;
template class Scaler<unsigned int>;

template <class Pixel>
Scaler<Pixel>* Scaler<Pixel>::createScaler(ScalerID id, Blender<Pixel> blender)
{
	switch(id) {
	case SCALER_SIMPLE:
		return new SimpleScaler<Pixel>();
	case SCALER_SAI2X:
		return new SaI2xScaler<Pixel>(blender);
	default:
		assert(false);
		return NULL;
	}
}

// === SimpleScaler ========================================================

template <class Pixel>
SimpleScaler<Pixel>::SimpleScaler()
{
}

template <class Pixel>
inline void SimpleScaler<Pixel>::copyLine(SDL_Surface* surface, int y)
{
	// TODO: Cheating (or at least using an undocumented feature) by
	//       assuming that pixels are already doubled horizontally.
	memcpy(
		(byte*)surface->pixels + (y+1) * surface->pitch,
		(byte*)surface->pixels + y * surface->pitch,
		surface->pitch
		);
}

template <class Pixel>
void SimpleScaler<Pixel>::scaleLine256(SDL_Surface* surface, int y)
{
	copyLine(surface, y);
}

template <class Pixel>
void SimpleScaler<Pixel>::scaleLine512(SDL_Surface* surface, int y)
{
	copyLine(surface, y);
}

// === SaI2xScaler =========================================================
//
// 2xSaI is Copyright (c) 1999-2001 by Derek Liauw Kie Fa.
//   http://elektron.its.tudelft.nl/~dalikifa/
// 2xSaI is free under GPL.
//
// Modified for use in openMSX by Maarten ter Huurne.

// Force template instantiation.
template class SaI2xScaler<byte>;
template class SaI2xScaler<word>;
template class SaI2xScaler<unsigned int>;

template <class Pixel>
SaI2xScaler<Pixel>::SaI2xScaler(Blender<Pixel> blender)
	: blender(blender)
{
}

template <class Pixel>
inline static Pixel* linePtr(SDL_Surface* surface, int y)
{
	if (y < 0) y = 0;
	if (y >= surface->h) y = surface->h - 2;
	return (Pixel*)((byte*)surface->pixels + y * surface->pitch);
}

template <class Pixel>
void SaI2xScaler<Pixel>::scaleLine256(SDL_Surface* surface, int y)
{
	Pixel* pixels0 = linePtr<Pixel>(surface, y - 2);
	Pixel* pixels1 = linePtr<Pixel>(surface, y);
	Pixel* pixelsN = linePtr<Pixel>(surface, y + 1);
	Pixel* pixels2 = linePtr<Pixel>(surface, y + 2);
	Pixel* pixels3 = linePtr<Pixel>(surface, y + 4);

	int width = (surface->w - 4) & ~1;
	for (int x = 2; x < width; x += 2) {
		// Map of the pixels:
		//   I|E F|J
		//   G|A B|K
		//   H|C D|L
		//   M|N O|P

		Pixel colorI = pixels0[x - 2];
		Pixel colorE = pixels0[x];
		Pixel colorF = pixels0[x + 2];
		Pixel colorJ = pixels0[x + 4];

		Pixel colorG = pixels1[x - 2];
		Pixel colorA = pixels1[x];
		Pixel colorB = pixels1[x + 2];
		Pixel colorK = pixels1[x + 4];

		Pixel colorH = pixels2[x - 2];
		Pixel colorC = pixels2[x];
		Pixel colorD = pixels2[x + 2];
		Pixel colorL = pixels2[x + 4];

		Pixel colorM = pixels3[x - 2];
		Pixel colorN = pixels3[x];
		Pixel colorO = pixels3[x + 2];
		//Pixel colorP = pixels3[x + 4];

		Pixel product, product1, product2;

		if (colorA == colorD && colorB != colorC) {
			product =
				( ( (colorA == colorE && colorB == colorL)
				  || (colorA == colorC && colorA == colorF && colorB != colorE && colorB == colorJ)
				  )
				? colorA
				: blender.blend(colorA, colorB)
				);
			product1 =
				( ( (colorA == colorG && colorC == colorO)
				  || (colorA == colorB && colorA == colorH && colorG != colorC && colorC == colorM)
				  )
				? colorA
				: blender.blend(colorA, colorC)
				);
			product2 = colorA;
		} else if (colorB == colorC && colorA != colorD) {
			product =
				( ( (colorB == colorF && colorA == colorH)
				  || (colorB == colorE && colorB == colorD && colorA != colorF && colorA == colorI)
				  )
				? colorB
				: blender.blend(colorA, colorB)
				);
			product1 =
				( ( (colorC == colorH && colorA == colorF)
				  || (colorC == colorG && colorC == colorD && colorA != colorH && colorA == colorI)
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
				( colorA == colorC && colorA == colorF && colorB != colorE && colorB == colorJ
				? colorA
				: ( colorB == colorE && colorB == colorD && colorA != colorF && colorA == colorI
				  ? colorB
				  : blender.blend(colorA, colorB)
				  )
				);
			product1 =
				( colorA == colorB && colorA == colorH && colorG != colorC && colorC == colorM
				? colorA
				: ( colorC == colorG && colorC == colorD && colorA != colorH && colorA == colorI
				  ? colorC
				  : blender.blend(colorA, colorC)
				  )
				);
			product2 = blender.blend( // TODO: Quad-blend may be better?
				blender.blend(colorA, colorB),
				blender.blend(colorC, colorD)
				);
		}

		//pixels1[x] = colorA; // retains original colour
		pixels1[x + 1] = product;
		pixelsN[x] = product1;
		pixelsN[x + 1] = product2;
	}
}

template <class Pixel>
void SaI2xScaler<Pixel>::scaleLine512(SDL_Surface* surface, int y)
{
	// Apply 2xSaI and keep the bottom-left pixel.
	// It's not great, but at least it looks better than doubling the pixel
	// like SimpleScaler does.
	Pixel* pixels0 = linePtr<Pixel>(surface, y - 2);
	Pixel* pixels1 = linePtr<Pixel>(surface, y);
	Pixel* pixelsN = linePtr<Pixel>(surface, y + 1);
	Pixel* pixels2 = linePtr<Pixel>(surface, y + 2);
	Pixel* pixels3 = linePtr<Pixel>(surface, y + 4);
	int width = surface->w;

	pixelsN[0] = blender.blend(pixels1[0], pixels2[0]);
	for (int x = 1; x < width - 1; x++) {
		// Map of the pixels:
		//   I E F
		//   G A B
		//   H C D
		//   M N O

		Pixel colorI = pixels0[x - 1];
		//Pixel colorE = pixels0[x];
		Pixel colorF = pixels0[x + 1];

		Pixel colorG = pixels1[x - 1];
		Pixel colorA = pixels1[x];
		Pixel colorB = pixels1[x + 1];

		Pixel colorH = pixels2[x - 1];
		Pixel colorC = pixels2[x];
		Pixel colorD = pixels2[x + 1];

		Pixel colorM = pixels3[x - 1];
		//Pixel colorN = pixels3[x];
		Pixel colorO = pixels3[x + 1];

		Pixel product1;

		if (colorA == colorD && colorB != colorC) {
			product1 =
				( ( (colorA == colorG && colorC == colorO)
				  || (colorA == colorB && colorA == colorH && colorG != colorC && colorC == colorM)
				  )
				? colorA
				: blender.blend(colorA, colorC)
				);
		} else if (colorB == colorC && colorA != colorD) {
			product1 =
				( ( (colorC == colorH && colorA == colorF)
				  || (colorC == colorG && colorC == colorD && colorA != colorH && colorA == colorI)
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
				( colorA == colorB && colorA == colorH && colorG != colorC && colorC == colorM
				? colorA
				: ( colorC == colorG && colorC == colorD && colorA != colorH && colorA == colorI
				  ? colorC
				  : blender.blend(colorA, colorC)
				  )
				);
		}

		pixelsN[x] = product1;
	}
	pixelsN[width - 1] = blender.blend(pixels1[width - 1], pixels2[width - 1]);
}

} // namespace openmsx

