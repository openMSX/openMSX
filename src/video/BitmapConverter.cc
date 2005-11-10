// $Id$

#include "BitmapConverter.hh"
#include "GLUtil.hh"

namespace openmsx {

template <class Pixel>
BitmapConverter<Pixel>::BitmapConverter(
	const Pixel* palette16, const Pixel* palette256,
	const Pixel* palette32768)
{
	this->palette16 = palette16;
	this->palette256 = palette256;
	this->palette32768 = palette32768;
}

template <class Pixel>
void BitmapConverter<Pixel>::renderGraphic4(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* /*vramPtr1*/)
{
	for (int n = 128; n--; ) {
		byte colour = *vramPtr0++;
		*pixelPtr++ = palette16[colour >> 4];
		*pixelPtr++ = palette16[colour & 15];
	}
}

template <class Pixel>
void BitmapConverter<Pixel>::renderGraphic5(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* /*vramPtr1*/)
{
	for (int n = 128; n--; ) {
		byte colour = *vramPtr0++;
		*pixelPtr++ = palette16[ 0 + ((colour >> 6) & 3)];
		*pixelPtr++ = palette16[16 + ((colour >> 4) & 3)];
		*pixelPtr++ = palette16[ 0 + ((colour >> 2) & 3)];
		*pixelPtr++ = palette16[16 + ((colour >> 0) & 3)];
	}
}

template <class Pixel>
void BitmapConverter<Pixel>::renderGraphic6(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1)
{
	for (int n = 128; n--; ) {
		byte color1 = *vramPtr0++;
		*pixelPtr++ = palette16[color1 >> 4];
		*pixelPtr++ = palette16[color1 & 0x0F];

		byte color2 = *vramPtr1++;
		*pixelPtr++ = palette16[color2 >> 4];
		*pixelPtr++ = palette16[color2 & 0x0F];
	}
}

template <class Pixel>
void BitmapConverter<Pixel>::renderGraphic7(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1)
{
	for (int n = 128; n--; ) {
		*pixelPtr++ = palette256[*vramPtr0++];
		*pixelPtr++ = palette256[*vramPtr1++];
	}
}

template <class Pixel>
void BitmapConverter<Pixel>::renderYJK(
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

template <class Pixel>
void BitmapConverter<Pixel>::renderYAE(
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
template <class Pixel>
void BitmapConverter<Pixel>::renderBogus(
	Pixel* pixelPtr, const byte* /*vramPtr0*/, const byte* /*vramPtr1*/)
{
	Pixel colour = palette16[0];
	for (int n = 256; n--; ) *pixelPtr++ = colour;
}


// Force template instantiation.
template class BitmapConverter<word>;
template class BitmapConverter<unsigned>;

#ifdef COMPONENT_GL
// The type "GLuint" can be either be equivalent to "word", "unsigned" or still
// some completely other type. In the later case we need to expand the
// BitmapConverter<GLuint> template, in the two former cases it is an error to
// expand it again (it is already instantiated above).
// The following piece of template metaprogramming takes care of this.

class NoExpansion {};
// ExpandFilter<T>::type = (T == word || T == unsigned) ? NoExpansion : T
template<class T> class ExpandFilter  { typedef T           type; };
template<> class ExpandFilter<word>     { typedef NoExpansion type; };
template<> class ExpandFilter<unsigned> { typedef NoExpansion type; };

template<> class BitmapConverter<NoExpansion> {};
template class BitmapConverter<ExpandFilter<GLuint>::type>;

#endif // COMPONENT_GL

} // namespace openmsx
