// $Id$

#include "V9990BitmapConverter.hh"
#include "V9990VRAM.hh"
#include "GLUtil.hh"
#include <string.h>
#include <cassert>
#include <algorithm>

using std::min;
using std::max;

namespace openmsx {

// Force template instantiation
template class V9990BitmapConverter<word, Renderer::ZOOM_256>;
template class V9990BitmapConverter<word, Renderer::ZOOM_REAL>;
template class V9990BitmapConverter<unsigned int, Renderer::ZOOM_256>;
template class V9990BitmapConverter<unsigned int, Renderer::ZOOM_REAL>;


#ifdef COMPONENT_GL
// On some systems, "GLuint" is not equivalent to "unsigned int",
// so BitmapConverter must be instantiated separately for those systems.
// But on systems where it is equivalent, it's an error to expand
// the same template twice.
// The following piece of template metaprogramming expands
// V9990BitmapConverter<GLuint, Renderer::ZOOM_REAL> to an empty class if
// "GLuint" is equivalent to "unsigned int"; otherwise it is expanded to
// the actual V9990BitmapConverter implementation.
//
// See BitmapConverter.cc
class NoExpansion {};
// ExpandFilter::ExpandType = (Type == unsigned int ? NoExpansion : Type)
template <class Type> class ExpandFilter {
	typedef Type ExpandType;
};
template <> class ExpandFilter<unsigned int> {
	typedef NoExpansion ExpandType;
};
template <Renderer::Zoom zoom> class V9990BitmapConverter<NoExpansion, zoom> {};
template class V9990BitmapConverter<
	ExpandFilter<GLuint>::ExpandType, Renderer::ZOOM_REAL >;
#endif // COMPONENT_GL


template <class Pixel, Renderer::Zoom zoom>
V9990BitmapConverter<Pixel, zoom>::V9990BitmapConverter(
	V9990VRAM* vram_,
	SDL_PixelFormat format_,
	Pixel* palette64_, Pixel* palette256_, Pixel* palette32768_)
	: vram(vram_), format(format_)
        , palette64(palette64_), palette256(palette256_), palette32768(palette32768_)
{
	// make sure function pointers have valid values
	setDisplayMode(P1);
	setColorMode(PP);
}

template <class Pixel, Renderer::Zoom zoom>
V9990BitmapConverter<Pixel, zoom>::~V9990BitmapConverter()
{
}

// - Rasterizers -------------------------------------------------------

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBYUV(
	Pixel* pixelPtr, uint address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram->readVRAM(address++);
		}
		int v = data[0] & 0x07 + ((data[1] & 0x07) << 3);
		int u = data[2] & 0x07 + ((data[3] & 0x07) << 3);
		if (v & 32) v -= 64; 
		if (u & 32) u -= 64; 
		for (int i = 0; i < 4; ++i) {
			int y = (data[i] & 0xF8) >> 3;
			int r = max(0, min(31, y + u));
			int g = max(0, min(31, (5 * y - 2 * u - v) / 4));
			int b = max(0, min(31, y + v));
			*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBYUVP(
	Pixel* pixelPtr, uint address, int nrPixels)
{
	// TODO Use palette
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram->readVRAM(address++);
		}
		int v = data[0] & 0x07 + ((data[1] & 0x07) << 3);
		int u = data[2] & 0x07 + ((data[3] & 0x07) << 3);
		if (v & 32) v -= 64; 
		if (u & 32) u -= 64; 
		for (int i = 0; i < 4; ++i) {
			int y = (data[i] & 0xF8) >> 3;
			int r = max(0, min(31, y + u));
			int g = max(0, min(31, (5 * y - 2 * u - v) / 4));
			int b = max(0, min(31, y + v));
			*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBYJK(
	Pixel* pixelPtr, uint address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram->readVRAM(address++);
		}
		int k = data[0] & 0x07 + ((data[1] & 0x07) << 3);
		int j = data[2] & 0x07 + ((data[3] & 0x07) << 3);
		if (k & 32) k -= 64; 
		if (j & 32) j -= 64; 
		for (int i = 0; i < 4; ++i) {
			int y = (data[i] & 0xF8) >> 3;
			int r = max(0, min(31, y + j));
			int g = max(0, min(31, y + k));
			int b = max(0, min(31, (5 * y - 2 * j - k) / 4));
			*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBYJKP(
	Pixel* pixelPtr, uint address, int nrPixels)
{
	// TODO Use palette
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram->readVRAM(address++);
		}
		int k = data[0] & 0x07 + ((data[1] & 0x07) << 3);
		int j = data[2] & 0x07 + ((data[3] & 0x07) << 3);
		if (k & 32) k -= 64; 
		if (j & 32) j -= 64; 
		for (int i = 0; i < 4; ++i) {
			int y = (data[i] & 0xF8) >> 3;
			int r = max(0, min(31, y + j));
			int g = max(0, min(31, y + k));
			int b = max(0, min(31, (5 * y - 2 * j - k) / 4));
			*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBD16(
	Pixel* pixelPtr, uint address, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		byte low  = vram->readVRAM(address++);
		byte high = vram->readVRAM(address++);
		*pixelPtr++ = palette32768[(low + 256 * high) & 0x7FFF];
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBD8(
	Pixel* pixelPtr, uint address, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		*pixelPtr++ = palette256[vram->readVRAM(address++)];
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBP6(
	Pixel* pixelPtr, uint address, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		*pixelPtr++ = palette64[vram->readVRAM(address++) & 0x3F];
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBP4(
	Pixel* pixelPtr, uint address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		byte data = vram->readVRAM(address++);
		*pixelPtr++ = palette64[data >> 4];
		*pixelPtr++ = palette64[data & 0x0F];
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterBP2(
	Pixel* pixelPtr, uint address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data = vram->readVRAM(address++);
		*pixelPtr++ = palette64[(data & 0xC0) >> 6];
		*pixelPtr++ = palette64[(data & 0x30) >> 4];
		*pixelPtr++ = palette64[(data & 0x0C) >> 2];
		*pixelPtr++ = palette64[(data & 0x03)];
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::rasterP(
	Pixel* pixelPtr, uint address, int nrPixels)
{
	rasterBP4(pixelPtr, address, nrPixels);
}

// -- Blenders --------------------------------------------------------

template <class Pixel, Renderer::Zoom zoom>
inline unsigned int V9990BitmapConverter<Pixel, zoom>::red(Pixel pix) {
	return (pix & format.Rmask) >> format.Rshift;
}

template <class Pixel, Renderer::Zoom zoom>
inline unsigned int V9990BitmapConverter<Pixel, zoom>::green(Pixel pix) {
	return (pix & format.Gmask) >> format.Gshift;
}

template <class Pixel, Renderer::Zoom zoom>
inline unsigned int V9990BitmapConverter<Pixel, zoom>::blue(Pixel pix) {
	return (pix & format.Bmask) >> format.Bshift;
}

template <class Pixel, Renderer::Zoom zoom>
inline Pixel V9990BitmapConverter<Pixel, zoom>::combine(
	unsigned r, unsigned g, unsigned b)
{
	return (Pixel)(((r << format.Rshift) & format.Rmask) |
	               ((g << format.Gshift) & format.Gmask) |
	               ((b << format.Bshift) & format.Bmask));
}

template <class Pixel, Renderer::Zoom zoom>
template <unsigned w1, unsigned w2>
inline Pixel V9990BitmapConverter<Pixel, zoom>::blendPixels2(const Pixel* source)
{
	unsigned total = w1 + w2;
	unsigned r = (red  (source[0]) * w1 + red  (source[1]) * w2) / total;
	unsigned g = (green(source[0]) * w1 + green(source[1]) * w2) / total;
	unsigned b = (blue (source[0]) * w1 + blue (source[1]) * w2) / total;
	return combine(r, g, b);
}

template <class Pixel, Renderer::Zoom zoom>
template <unsigned w1, unsigned w2, unsigned w3>
inline Pixel V9990BitmapConverter<Pixel, zoom>::blendPixels3(const Pixel* source)
{
	unsigned total = w1 + w2 + w3;
	unsigned r = (red  (source[0]) * w1 +
	              red  (source[1]) * w2 +
	              red  (source[2]) * w3) / total;
	unsigned g = (green(source[0]) * w1 +
	              green(source[1]) * w2 +
	              green(source[2]) * w3) / total;
	unsigned b = (blue (source[0]) * w1 +
	              blue (source[1]) * w2 +
	              blue (source[2]) * w3) / total;
	return combine(r, g, b);
}

template <class Pixel, Renderer::Zoom zoom>
template <unsigned w1, unsigned w2, unsigned w3, unsigned w4>
inline Pixel V9990BitmapConverter<Pixel, zoom>::blendPixels4(const Pixel* source)
{
	unsigned total = w1 + w2 + w3;
	unsigned r = (red  (source[0]) * w1 +
	              red  (source[1]) * w2 +
	              red  (source[2]) * w3 +
	              red  (source[3]) * w4) / total;
	unsigned g = (green(source[0]) * w1 +
	              green(source[1]) * w2 +
	              green(source[2]) * w3 +
	              green(source[3]) * w4) / total;
	unsigned b = (blue (source[0]) * w1 +
	              blue (source[1]) * w2 +
	              blue (source[2]) * w3 +
	              blue (source[3]) * w4) / total;
	return combine(r, g, b);
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_1on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		Pixel pix = inPixels[p];
		*outPixels++ = pix;
		*outPixels++ = pix;
		*outPixels++ = pix;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_1on2(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		Pixel pix = inPixels[p];
		*outPixels++ = pix;
		*outPixels++ = pix;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_1on1(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	memcpy(outPixels, inPixels, nrPixels * sizeof(Pixel));
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_2on1(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		*outPixels++ = blendPixels2<1, 1>(&inPixels[p]);
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_4on1(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		*outPixels++ = blendPixels4<1, 1, 1, 1>(&inPixels[p]);
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_2on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		*outPixels++ =                     inPixels[p + 0];
		*outPixels++ = blendPixels2<1, 1>(&inPixels[p + 0]);
		*outPixels++ =                     inPixels[p + 1];
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_4on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		*outPixels++ = blendPixels2<3, 1>(&inPixels[p + 0]); 
		*outPixels++ = blendPixels2<1, 1>(&inPixels[p + 1]);
		*outPixels++ = blendPixels2<1, 3>(&inPixels[p + 2]);
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_8on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 8) {
		*outPixels++ = blendPixels3<3, 3, 2>   (&inPixels[p + 0]);
		*outPixels++ = blendPixels4<1, 3, 3, 1>(&inPixels[p + 2]);
		*outPixels++ = blendPixels3<2, 3, 3>   (&inPixels[p + 5]);
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::blend_none(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	memset(outPixels, 0, nrPixels * sizeof(Pixel));
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::setDisplayMode(V9990DisplayMode mode)
{
	// Select pixel blender
	if(zoom == Renderer::ZOOM_256) {
		switch (mode) {
			case P1:
			case B1: blendMethod = &V9990BitmapConverter::blend_1on1; break;
			case P2:
			case B3: blendMethod = &V9990BitmapConverter::blend_2on1; break;
			case B7: blendMethod = &V9990BitmapConverter::blend_4on1; break;
			case B0: blendMethod = &V9990BitmapConverter::blend_2on3; break;
			case B2: blendMethod = &V9990BitmapConverter::blend_4on3; break;
			case B4: blendMethod = &V9990BitmapConverter::blend_8on3; break;
			case B5:
			case B6: blendMethod = &V9990BitmapConverter::blend_none; break;
			default: assert(false);
		}
	} else {
		switch (mode) {
			case P1:
			case B1: blendMethod = &V9990BitmapConverter::blend_1on2; break;
			case P2:
			case B3: blendMethod = &V9990BitmapConverter::blend_1on1; break;
			case B7: blendMethod = &V9990BitmapConverter::blend_2on1; break;
			case B0: blendMethod = &V9990BitmapConverter::blend_1on3; break;
			case B2: blendMethod = &V9990BitmapConverter::blend_2on3; break;
			case B4: blendMethod = &V9990BitmapConverter::blend_4on3; break;
			case B5:
			case B6: blendMethod = &V9990BitmapConverter::blend_none; break;
			default: assert(false);
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::setColorMode(V9990ColorMode mode)
{
	switch (mode) {
		case PP:    rasterMethod = &V9990BitmapConverter::rasterP;     break;
		case BYUV:  rasterMethod = &V9990BitmapConverter::rasterBYUV;  break;
		case BYUVP: rasterMethod = &V9990BitmapConverter::rasterBYUVP; break;
		case BYJK:  rasterMethod = &V9990BitmapConverter::rasterBYJK;  break;
		case BYJKP: rasterMethod = &V9990BitmapConverter::rasterBYJKP; break;
		case BD16:  rasterMethod = &V9990BitmapConverter::rasterBD16;  break;
		case BD8:   rasterMethod = &V9990BitmapConverter::rasterBD8;   break;
		case BP6:   rasterMethod = &V9990BitmapConverter::rasterBP6;   break;
		case BP4:   rasterMethod = &V9990BitmapConverter::rasterBP4;   break;
		case BP2:   rasterMethod = &V9990BitmapConverter::rasterBP2;   break;
		default:    assert (false);
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990BitmapConverter<Pixel, zoom>::convertLine(
	Pixel* linePtr, uint address, int nrPixels)
{
	assert(nrPixels <= 640);
	Pixel tmp[640 * sizeof(Pixel)];
	(this->*rasterMethod)(tmp, address, nrPixels);
	(this->*blendMethod)(tmp, linePtr, nrPixels);
}

} // namespace openmsx

