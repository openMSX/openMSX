// $Id$

#include "BitmapConverter.hh"
#include "GLUtil.hh"


namespace openmsx {

#ifdef COMPONENT_GL
// On some systems, "GLuint" is not equivalent to "unsigned int",
// so BitmapConverter must be instantiated separately for those systems.
// But on systems where it is equivalent, it's an error to expand
// the same template twice.
// The following piece of template metaprogramming expands
// BitmapConverter<GLuint, Renderer::ZOOM_REAL> to an empty class if
// "GLuint" is equivalent to "unsigned int"; otherwise it is expanded to
// the actual BitmapConverter implementation.
class NoExpansion {};
// ExpandFilter::ExpandType = (Type == unsigned int ? NoExpansion : Type)
template <class Type> class ExpandFilter {
	typedef Type ExpandType;
};
template <> class ExpandFilter<unsigned int> {
	typedef NoExpansion ExpandType;
};
template <Renderer::Zoom zoom> class BitmapConverter<NoExpansion, zoom> {};
template class BitmapConverter<
	ExpandFilter<GLuint>::ExpandType, Renderer::ZOOM_REAL >;
#endif // COMPONENT_GL


template <class Pixel, Renderer::Zoom zoom>
BitmapConverter<Pixel, zoom>::BitmapConverter(
	const Pixel* palette16, const Pixel* palette256,
	const Pixel* palette32768, Blender<Pixel> blender)
	: blender(blender)
{
	this->palette16 = palette16;
	this->palette256 = palette256;
	this->palette32768 = palette32768;
}

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderGraphic4(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* /*vramPtr1*/)
{
	for (int n = 128; n--; ) {
		byte colour = *vramPtr0++;
		*pixelPtr++ = palette16[colour >> 4];
		*pixelPtr++ = palette16[colour & 15];
	}
}

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderGraphic5(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* /*vramPtr1*/)
{
	for (int n = 128; n--; ) {
		byte colour = *vramPtr0++;
		if (zoom != Renderer::ZOOM_256) {
			*pixelPtr++ = palette16[ 0 + ((colour >> 6) & 3)];
			*pixelPtr++ = palette16[16 + ((colour >> 4) & 3)];
			*pixelPtr++ = palette16[ 0 + ((colour >> 2) & 3)];
			*pixelPtr++ = palette16[16 + ((colour >> 0) & 3)];
		} else {
			*pixelPtr++ = blender.blend(palette16[ 0 + ((colour >> 6) & 3)],
			                            palette16[16 + ((colour >> 4) & 3)]);
			*pixelPtr++ = blender.blend(palette16[ 0 + ((colour >> 2) & 3)],
			                            palette16[16 + ((colour >> 0) & 3)]);
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderGraphic6(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1)
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
	Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1)
{
	for (int n = 128; n--; ) {
		*pixelPtr++ = palette256[*vramPtr0++];
		*pixelPtr++ = palette256[*vramPtr1++];
	}
}

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderYJK(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1)
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

			*pixelPtr++ = palette32768[col];
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderYAE(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1)
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
			*pixelPtr++ = pix;
		}
	}
}

// TODO: Check what happens on real V9938.
template <class Pixel, Renderer::Zoom zoom>
void BitmapConverter<Pixel, zoom>::renderBogus(
	Pixel* pixelPtr, const byte* /*vramPtr0*/, const byte* /*vramPtr1*/)
{
	Pixel colour = palette16[0];
	for (int n = 256; n--; ) *pixelPtr++ = colour;
}


// Force template instantiation.
template class BitmapConverter<word, Renderer::ZOOM_256>;
template class BitmapConverter<word, Renderer::ZOOM_REAL>;
template class BitmapConverter<unsigned int, Renderer::ZOOM_256>;
template class BitmapConverter<unsigned int, Renderer::ZOOM_REAL>;

} // namespace openmsx

