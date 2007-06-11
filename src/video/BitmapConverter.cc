// $Id$

#include "BitmapConverter.hh"
#include "GLUtil.hh"
#include "Math.hh"

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
	for (unsigned i = 0; i < 128; ++i) {
		unsigned data = vramPtr0[i];
		pixelPtr[2 * i + 0] = palette16[data >> 4];
		pixelPtr[2 * i + 1] = palette16[data & 15];
	}
}

template <class Pixel>
void BitmapConverter<Pixel>::renderGraphic5(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* /*vramPtr1*/)
{
	for (unsigned i = 0; i < 128; ++i) {
		unsigned data = vramPtr0[i];
		pixelPtr[4 * i + 0] = palette16[ 0 + ((data >> 6) & 3)];
		pixelPtr[4 * i + 1] = palette16[16 + ((data >> 4) & 3)];
		pixelPtr[4 * i + 2] = palette16[ 0 + ((data >> 2) & 3)];
		pixelPtr[4 * i + 3] = palette16[16 + ((data >> 0) & 3)];
	}
}

template <class Pixel>
void BitmapConverter<Pixel>::renderGraphic6(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1)
{
	for (unsigned i = 0; i < 128; ++i) {
		unsigned data0 = vramPtr0[i];
		unsigned data1 = vramPtr1[i];
		pixelPtr[4 * i + 0] = palette16[data0 >> 4];
		pixelPtr[4 * i + 1] = palette16[data0 & 15];
		pixelPtr[4 * i + 2] = palette16[data1 >> 4];
		pixelPtr[4 * i + 3] = palette16[data1 & 15];
	}
}

template <class Pixel>
void BitmapConverter<Pixel>::renderGraphic7(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1)
{
	for (unsigned i = 0; i < 128; ++i) {
		pixelPtr[2 * i + 0] = palette256[vramPtr0[i]];
		pixelPtr[2 * i + 1] = palette256[vramPtr1[i]];
	}
}

template <class Pixel>
void BitmapConverter<Pixel>::renderYJK(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1)
{
	for (unsigned i = 0; i < 64; ++i) {
		unsigned p[4];
		p[0] = vramPtr0[2 * i + 0];
		p[1] = vramPtr1[2 * i + 0];
		p[2] = vramPtr0[2 * i + 1];
		p[3] = vramPtr1[2 * i + 1];

		int j = (p[2] & 7) + ((p[3] & 3) << 3) - ((p[3] & 4) << 3);
		int k = (p[0] & 7) + ((p[1] & 3) << 3) - ((p[1] & 4) << 3);

		for (unsigned n = 0; n < 4; ++n) {
			int y = p[n] >> 3;
			int r = Math::clip<0, 31>(y + j);
			int g = Math::clip<0, 31>(y + k);
			int b = Math::clip<0, 31>((5 * y - 2 * j - k) / 4);
			int col = (r << 10) + (g << 5) + b;
			pixelPtr[4 * i + n] = palette32768[col];
		}
	}
}

template <class Pixel>
void BitmapConverter<Pixel>::renderYAE(
	Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1)
{
	for (unsigned i = 0; i < 64; ++i) {
		unsigned p[4];
		p[0] = vramPtr0[2 * i + 0];
		p[1] = vramPtr1[2 * i + 0];
		p[2] = vramPtr0[2 * i + 1];
		p[3] = vramPtr1[2 * i + 1];

		int j = (p[2] & 7) + ((p[3] & 3) << 3) - ((p[3] & 4) << 3);
		int k = (p[0] & 7) + ((p[1] & 3) << 3) - ((p[1] & 4) << 3);

		for (unsigned n = 0; n < 4; ++n) {
			Pixel pix;
			if (p[n] & 0x08) {
				// YAE
				pix = palette16[p[n] >> 4];
			} else {
				// YJK
				int y = p[n] >> 3;
				int r = Math::clip<0, 31>(y + j);
				int g = Math::clip<0, 31>(y + k);
				int b = Math::clip<0, 31>((5 * y - 2 * j - k) / 4);
				pix = palette32768[(r << 10) + (g << 5) + b];
			}
			pixelPtr[4 * i + n] = pix;
		}
	}
}

// TODO: Check what happens on real V9938.
template <class Pixel>
void BitmapConverter<Pixel>::renderBogus(
	Pixel* pixelPtr, const byte* /*vramPtr0*/, const byte* /*vramPtr1*/)
{
	Pixel color = palette16[0];
	for (unsigned i = 0; i < 256; ++i) {
		pixelPtr[i] = color;
	}
}

// Force template instantiation.
template class BitmapConverter<word>;
template class BitmapConverter<unsigned>;
#ifdef COMPONENT_GL
template<> class BitmapConverter<GLUtil::NoExpansion> {};
template class BitmapConverter<GLUtil::ExpandGL>;
#endif // COMPONENT_GL

} // namespace openmsx
