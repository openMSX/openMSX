// $Id$

#include "Scalers.hh"
#include "openmsx.hh"


namespace openmsx {

Scaler::Scaler() {
}

/** Returns an array containing an instance of every Scaler subclass,
* indexed by ScaledID.
*/
template <class Pixel>
Scaler **Scaler::createScalers(Blender<Pixel> blender) {
	Scaler **ret = new Scaler *[LAST];
	ret[SIMPLE] = new SimpleScaler();
	ret[SAI2X] = new SaI2xScaler<Pixel>(blender);
	return ret;
}

// Force template instantiation.
template Scaler **Scaler::createScalers<byte>(Blender<byte> blender);
template Scaler **Scaler::createScalers<word>(Blender<word> blender);
template Scaler **Scaler::createScalers<unsigned int>(Blender<unsigned int> blender);

void Scaler::disposeScalers(Scaler **scalers) {
	for (int i = 0; i < LAST; i++) {
		delete scalers[i];
	}
	delete[] scalers;
}

// === SimpleScaler ========================================================

SimpleScaler::SimpleScaler() {
}

void SimpleScaler::scaleLine(SDL_Surface *surface, int y) {
	// TODO: Cheating (or at least using an undocumented feature) by
	//       assuming that pixels are already doubled horizontally.
	SDL_Rect source, dest;
	source.x = 0;
	source.y = y;
	source.w = surface->w;
	source.h = 1;
	dest.x = 0;
	dest.y = y + 1;
	SDL_BlitSurface(surface, &source, surface, &dest);
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
inline static Pixel *linePtr(SDL_Surface *surface, int y) {
	if (y < 0) y = 0;
	if (y >= surface->h) y = surface->h - 1;
	return (Pixel *)((byte *)surface->pixels + y * surface->pitch);
}

template <class Pixel>
inline static int getResult1(Pixel A, Pixel B, Pixel C, Pixel D)
{
	int x = 0;
	int y = 0;
	int r = 0;
	if (A == C) x+=1; else if (B == C) y+=1;
	if (A == D) x+=1; else if (B == D) y+=1;
	if (x <= 1) r+=1;
	if (y <= 1) r-=1;
	return r;
}

template <class Pixel>
inline static int getResult2(Pixel A, Pixel B, Pixel C, Pixel D)
{
	int x = 0;
	int y = 0;
	int r = 0;
	if (A == C) x+=1; else if (B == C) y+=1;
	if (A == D) x+=1; else if (B == D) y+=1;
	if (x <= 1) r-=1;
	if (y <= 1) r+=1;
	return r;
}

template <class Pixel>
void SaI2xScaler<Pixel>::scaleLine(SDL_Surface *surface, int y)
{
	Pixel *pixels0 = linePtr<Pixel>(surface, y - 2);
	Pixel *pixels1 = linePtr<Pixel>(surface, y);
	Pixel *pixels2 = linePtr<Pixel>(surface, y + 2);
	Pixel *pixels3 = linePtr<Pixel>(surface, y + 4);

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
				product = colorA;
				product1 = colorA;
				product2 = colorA;
			} else {
				int r =
					getResult1<Pixel>(colorA, colorB, colorG, colorE) +
					getResult2<Pixel>(colorB, colorA, colorK, colorF) +
					getResult2<Pixel>(colorB, colorA, colorH, colorN) +
					getResult1<Pixel>(colorA, colorB, colorL, colorO);
				product = blender.blend(colorA, colorB);
				product1 = blender.blend(colorA, colorC);
				product2 =
					( r > 0
					? colorA
					: ( r < 0
					  ? colorB
					  : blender.blend( // TODO: Quad-blend may be better?
					      blender.blend(colorA, colorB),
					      blender.blend(colorC, colorD)
					      )
					  )
					);
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

		Pixel *pixelsN = linePtr<Pixel>(surface, y + 1);
		//pixels1[x] = colorA; // retains original colour
		pixels1[x + 1] = product;
		pixelsN[x] = product1;
		pixelsN[x + 1] = product2;
	}

}

/* test scaler: horizontal blur, vertical double
template <class Pixel>
void Scaler<Pixel>::scaleLine(SDL_Surface *surface, int y)
{
	Pixel *p = (Pixel *)
		((byte *)surface->pixels + y * surface->pitch);
	Pixel *pn = (Pixel *)
		((byte *)surface->pixels + (y + 1) * surface->pitch);
	int width = surface->w;
	for (int x = 0; x < width - 2; x += 2) {
		pn[x] = p[x];
		pn[x + 1] = p[x + 1] = blender.blend(p[x], p[x + 2]);
	}
	pn[width - 1] = p[width - 1] = p[width - 2];
}
*/

} // namespace openmsx
