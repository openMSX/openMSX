// $Id$

#include "BitmapConverter.hh"

namespace openmsx {

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
	const Pixel *palette16, const Pixel *palette256,
	const Pixel *palette32768, Blender<Pixel> blender)
	: blender(blender)
{
	this->palette16 = palette16;
	this->palette256 = palette256;
	this->palette32768 = palette32768;
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
		if (zoom != Renderer::ZOOM_256) {
			*pixelPtr++ = palette16[(colour >> 6) & 3];
			*pixelPtr++ = palette16[(colour >> 4) & 3];
			*pixelPtr++ = palette16[(colour >> 2) & 3];
			*pixelPtr++ = palette16[(colour >> 0) & 3];
		} else {
			*pixelPtr++ = blender.blend(palette16[(colour >> 6) & 3],
			                            palette16[(colour >> 4) & 3]);
			*pixelPtr++ = blender.blend(palette16[(colour >> 2) & 3],
			                            palette16[(colour >> 0) & 3]);
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderGraphic6(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	for (int n = 128; n--; ) {
		byte colour = *vramPtr0++;
		if (zoom != Renderer::ZOOM_256) {
			*pixelPtr++ = palette16[colour >> 4];
			*pixelPtr++ = palette16[colour & 0x0F];
		} else {
			*pixelPtr++ = blender.blend(palette16[colour >> 4],
			                            palette16[colour & 0x0F]);
		}
		colour = *vramPtr1++;
		if (zoom != Renderer::ZOOM_256) {
			*pixelPtr++ = palette16[colour >> 4];
			*pixelPtr++ = palette16[colour & 0x0F];
		} else {
			*pixelPtr++ = blender.blend(palette16[colour >> 4],
			                            palette16[colour & 0x0F]);
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

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderYJK(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	for (int n = 64; n--; ) {
		byte p[4];
		p[0] = *vramPtr0++;
		p[1] = *vramPtr1++;
		p[2] = *vramPtr0++;
		p[3] = *vramPtr1++;

		char j = (p[2] & 7) + ((p[3] & 3) << 3) - ((p[3] & 4) << 3);
		char k = (p[0] & 7) + ((p[1] & 3) << 3) - ((p[1] & 4) << 3);

		for (int i = 0; i < 4; i++) {
			char y = p[i] >> 3;
			char r = y + j;
			char g = y + k;
			char b = (5 * y - 2 * j - k) / 4;
			if (r < 0) r = 0; else if (r > 31) r = 31;
			if (g < 0) g = 0; else if (g > 31) g = 31;
			if (b < 0) b = 0; else if (b > 31) b = 31;
			int col = (r << 10) + (g << 5) + b;
			
			if (zoom == Renderer::ZOOM_512) {
				pixelPtr[0] = pixelPtr[1] = palette32768[col];
				pixelPtr += 2;
			} else {
				*pixelPtr++ = palette32768[col];
			}
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderYAE(
	Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1)
{
	for (int n = 64; n--; ) {
		byte p[4];
		p[0] = *vramPtr0++;
		p[1] = *vramPtr1++;
		p[2] = *vramPtr0++;
		p[3] = *vramPtr1++;

		char j = (p[2] & 7) + ((p[3] & 3) << 3) - ((p[3] & 4) << 3);
		char k = (p[0] & 7) + ((p[1] & 3) << 3) - ((p[1] & 4) << 3);

		for (int i = 0; i < 4; i++) {
			Pixel pix;
			if (p[i] & 0x08) {
				// YAE
				pix = palette16[p[i] >> 4];
			} else {
				// YJK
				char y = p[i] >> 3;
				char r = y + j;
				char g = y + k;
				char b = (5 * y - 2 * j - k) / 4;
				if (r < 0) r = 0; else if (r > 31) r = 31;
				if (g < 0) g = 0; else if (g > 31) g = 31;
				if (b < 0) b = 0; else if (b > 31) b = 31;
				pix = palette32768[(r << 10) + (g << 5) + b];
			}
			if (zoom == Renderer::ZOOM_512) {
				pixelPtr[0] = pixelPtr[1] = pix;
				pixelPtr += 2;
			} else {
				*pixelPtr++ = pix;
			}
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

} // namespace openmsx

