// $Id$

#include "BitmapConverter.hh"

// Force template instantiation for these types.
// Without this, object file contains no method implementations.
template class BitmapConverter<byte>;
template class BitmapConverter<word>;
template class BitmapConverter<unsigned int>;

template <class Pixel> BitmapConverter<Pixel>::BitmapConverter(
	const Pixel *palette16, const Pixel *palette256)
{
	this->palette16 = palette16;
	this->palette256 = palette256;
}

template <class Pixel> void BitmapConverter<Pixel>::renderGraphic4(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	for (int n = 128; n--; ) {
		byte colour = *vramPtr0++;
		pixelPtr[0] = pixelPtr[1] = palette16[colour >> 4];
		pixelPtr[2] = pixelPtr[3] = palette16[colour & 15];
		pixelPtr += 4;
	}
}

template <class Pixel> void BitmapConverter<Pixel>::renderGraphic5(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	for (int n = 128; n--; ) {
		byte colour = *vramPtr0++;
		*pixelPtr++ = palette16[colour >> 6];
		*pixelPtr++ = palette16[(colour >> 4) & 3];
		*pixelPtr++ = palette16[(colour >> 2) & 3];
		*pixelPtr++ = palette16[colour & 3];
	}
}

template <class Pixel> void BitmapConverter<Pixel>::renderGraphic6(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	for (int n = 128; n--; ) {
		byte colour = *vramPtr0++;
		*pixelPtr++ = palette16[colour >> 4];
		*pixelPtr++ = palette16[colour & 0x0F];
		colour = *vramPtr1++;
		*pixelPtr++ = palette16[colour >> 4];
		*pixelPtr++ = palette16[colour & 0x0F];
	}
}

template <class Pixel> void BitmapConverter<Pixel>::renderGraphic7(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	for (int n = 128; n--; ) {
		pixelPtr[0] = pixelPtr[1] = palette256[*vramPtr0++];
		pixelPtr[2] = pixelPtr[3] = palette256[*vramPtr1++];
		pixelPtr += 4;
	}
}

// TODO: Check what happens on real V9938.
template <class Pixel> void BitmapConverter<Pixel>::renderBogus(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	Pixel colour = palette16[0];
	for (int n = 512; n--; ) *pixelPtr++ = colour;
}

