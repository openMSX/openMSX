// $Id$

#include "BitmapConverter.hh"

// Force template instantiation for these types.
// Without this, object file contains no method implementations.
template class BitmapConverter<byte, Renderer::ZOOM_256>;
template class BitmapConverter<byte, Renderer::ZOOM_512>;
template class BitmapConverter<word, Renderer::ZOOM_256>;
template class BitmapConverter<word, Renderer::ZOOM_512>;
template class BitmapConverter<unsigned int, Renderer::ZOOM_256>;
template class BitmapConverter<unsigned int, Renderer::ZOOM_512>;
template class BitmapConverter<unsigned int, Renderer::ZOOM_REAL>;

template <class Pixel, Renderer::Zoom zoom>
BitmapConverter<Pixel, zoom>::BitmapConverter(
	const Pixel *palette16, const Pixel *palette256)
{
	this->palette16 = palette16;
	this->palette256 = palette256;
}

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderGraphic4(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	for (int n = 128; n--; ) {
		byte colour = *vramPtr0++;
		if (zoom == Renderer::ZOOM_512) {
			pixelPtr[0] = pixelPtr[1] = palette16[colour >> 4];
			pixelPtr[2] = pixelPtr[3] = palette16[colour & 15];
			pixelPtr += 4;
		} else {
			*pixelPtr++ = palette16[colour >> 4];
			*pixelPtr++ = palette16[colour & 15];
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderGraphic5(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	for (int n = 128; n--; ) {
		byte colour = *vramPtr0++;
		*pixelPtr++ = palette16[colour >> 6];
		if (zoom != Renderer::ZOOM_256) {
			*pixelPtr++ = palette16[(colour >> 4) & 3];
		}
		*pixelPtr++ = palette16[(colour >> 2) & 3];
		if (zoom != Renderer::ZOOM_256) {
			*pixelPtr++ = palette16[colour & 3];
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderGraphic6(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	for (int n = 128; n--; ) {
		byte colour = *vramPtr0++;
		*pixelPtr++ = palette16[colour >> 4];
		if (zoom != Renderer::ZOOM_256) {
			*pixelPtr++ = palette16[colour & 0x0F];
		}
		colour = *vramPtr1++;
		*pixelPtr++ = palette16[colour >> 4];
		if (zoom != Renderer::ZOOM_256) {
			*pixelPtr++ = palette16[colour & 0x0F];
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderGraphic7(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	for (int n = 128; n--; ) {
		if (zoom == Renderer::ZOOM_512) {
			pixelPtr[0] = pixelPtr[1] = palette256[*vramPtr0++];
			pixelPtr[2] = pixelPtr[3] = palette256[*vramPtr1++];
			pixelPtr += 4;
		} else {
			*pixelPtr++ = palette256[*vramPtr0++];
			*pixelPtr++ = palette256[*vramPtr1++];
		}
	}
}

// TODO: Check what happens on real V9938.
template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderBogus(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	Pixel colour = palette16[0];
	if (zoom == Renderer::ZOOM_512) {
		for (int n = 512; n--; ) *pixelPtr++ = colour;
	} else {
		for (int n = 256; n--; ) *pixelPtr++ = colour;
	}
}

