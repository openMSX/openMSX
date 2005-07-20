// $Id$

#include "V9990BitmapConverter.hh"
#include "V9990VRAM.hh"
#include "V9990.hh"
#include "GLUtil.hh"
#include <string.h>
#include <cassert>
#include <algorithm>

using std::min;
using std::max;

namespace openmsx {

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
template <> class V9990BitmapConverter<NoExpansion> {};
template class V9990BitmapConverter<ExpandFilter<GLuint>::ExpandType>;
#endif // COMPONENT_GL


template <class Pixel>
V9990BitmapConverter<Pixel>::V9990BitmapConverter(
	V9990& vdp_,
	SDL_PixelFormat format_,
	Pixel* palette64_, Pixel* palette256_, Pixel* palette32768_)
	: vdp(vdp_), vram(vdp.getVRAM()), format(format_)
        , palette64(palette64_), palette256(palette256_), palette32768(palette32768_)
{
	// make sure function pointers have valid values
	setDisplayMode(P1);
	setColorMode(PP);
}

template <class Pixel>
V9990BitmapConverter<Pixel>::~V9990BitmapConverter()
{
}

// - Rasterizers -------------------------------------------------------

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYUV(
	Pixel* pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram.readVRAM(address++);
		}

		char u = (data[2] & 7) + ((data[3] & 3) << 3) - ((data[3] & 4) << 3);
		char v = (data[0] & 7) + ((data[1] & 3) << 3) - ((data[1] & 4) << 3);

		for (int i = 0; i < 4; ++i) {
			char y = (data[i] & 0xF8) >> 3;
			char r = max(0, min(31, y + u));
			char g = max(0, min(31, (5 * y - 2 * u - v) / 4));
			char b = max(0, min(31, y + v));
			*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
		}
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYUVP(
	Pixel* pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram.readVRAM(address++);
		}

		char u = (data[2] & 7) + ((data[3] & 3) << 3) - ((data[3] & 4) << 3);
		char v = (data[0] & 7) + ((data[1] & 3) << 3) - ((data[1] & 4) << 3);

		for (int i = 0; i < 4; ++i) {
			if (data[i] & 0x08) {
				*pixelPtr++ = palette64[data[i] >> 4];
			} else {
				char y = (data[i] & 0xF8) >> 3;
				char r = max(0, min(31, y + u));
				char g = max(0, min(31, (5 * y - 2 * u - v) / 4));
				char b = max(0, min(31, y + v));
				*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
			}
		}
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYJK(
	Pixel* pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram.readVRAM(address++);
		}

		char j = (data[2] & 7) + ((data[3] & 3) << 3) - ((data[3] & 4) << 3);
		char k = (data[0] & 7) + ((data[1] & 3) << 3) - ((data[1] & 4) << 3);

		for (int i = 0; i < 4; ++i) {
			char y = data[i] >> 3;
			char r = max(0, min(31, y + j));
			char g = max(0, min(31, y + k));
			char b = max(0, min(31, (5 * y - 2 * j - k) / 4));
			*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
		}
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYJKP(
	Pixel* pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram.readVRAM(address++);
		}

		char j = (data[2] & 7) + ((data[3] & 3) << 3) - ((data[3] & 4) << 3);
		char k = (data[0] & 7) + ((data[1] & 3) << 3) - ((data[3] & 4) << 3);

		for (int i = 0; i < 4; ++i) {
			if (data[i] & 0x08) {
				*pixelPtr++ = palette64[data[i] >> 4];
			} else {
				char y = data[i] >> 3;
				char r = max(0, min(31, y + j));
				char g = max(0, min(31, y + k));
				char b = max(0, min(31, (5 * y - 2 * j - k) / 4));
				*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
			}
		}
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBD16(
	Pixel* pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		byte low  = vram.readVRAM(address++);
		byte high = vram.readVRAM(address++);
		*pixelPtr++ = palette32768[(low + 256 * high) & 0x7FFF];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBD8(
	Pixel* pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		*pixelPtr++ = palette256[vram.readVRAM(address++)];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBP6(
	Pixel* pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		*pixelPtr++ = palette64[vram.readVRAM(address++) & 0x3F];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBP4(
	Pixel* pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		byte data = vram.readVRAM(address++);
		*pixelPtr++ = palette64[data >> 4];
		*pixelPtr++ = palette64[data & 0x0F];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBP2(
	Pixel* pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data = vram.readVRAM(address++);
		*pixelPtr++ = palette64[(data & 0xC0) >> 6];
		*pixelPtr++ = palette64[(data & 0x30) >> 4];
		*pixelPtr++ = palette64[(data & 0x0C) >> 2];
		*pixelPtr++ = palette64[(data & 0x03)];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterP(
	Pixel* pixelPtr, unsigned address, int nrPixels)
{
	rasterBP4(pixelPtr, address, nrPixels);
}

// -- Blenders --------------------------------------------------------

template <class Pixel>
inline unsigned int V9990BitmapConverter<Pixel>::red(Pixel pix) {
	return (pix & format.Rmask) >> format.Rshift;
}

template <class Pixel>
inline unsigned int V9990BitmapConverter<Pixel>::green(Pixel pix) {
	return (pix & format.Gmask) >> format.Gshift;
}

template <class Pixel>
inline unsigned int V9990BitmapConverter<Pixel>::blue(Pixel pix) {
	return (pix & format.Bmask) >> format.Bshift;
}

template <class Pixel>
inline Pixel V9990BitmapConverter<Pixel>::combine(
	unsigned r, unsigned g, unsigned b)
{
	return (Pixel)(((r << format.Rshift) & format.Rmask) |
	               ((g << format.Gshift) & format.Gmask) |
	               ((b << format.Bshift) & format.Bmask));
}

template <class Pixel>
template <unsigned w1, unsigned w2>
inline Pixel V9990BitmapConverter<Pixel>::blendPixels2(const Pixel* source)
{
	unsigned total = w1 + w2;
	unsigned r = (red  (source[0]) * w1 + red  (source[1]) * w2) / total;
	unsigned g = (green(source[0]) * w1 + green(source[1]) * w2) / total;
	unsigned b = (blue (source[0]) * w1 + blue (source[1]) * w2) / total;
	return combine(r, g, b);
}

template <class Pixel>
template <unsigned w1, unsigned w2, unsigned w3>
inline Pixel V9990BitmapConverter<Pixel>::blendPixels3(const Pixel* source)
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

template <class Pixel>
template <unsigned w1, unsigned w2, unsigned w3, unsigned w4>
inline Pixel V9990BitmapConverter<Pixel>::blendPixels4(const Pixel* source)
{
	unsigned total = w1 + w2 + w3 + w4;
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

template <class Pixel>
void V9990BitmapConverter<Pixel>::blend_1on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		Pixel pix = inPixels[p];
		*outPixels++ = pix;
		*outPixels++ = pix;
		*outPixels++ = pix;
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::blend_1on2(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		Pixel pix = inPixels[p];
		*outPixels++ = pix;
		*outPixels++ = pix;
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::blend_1on1(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	memcpy(outPixels, inPixels, nrPixels * sizeof(Pixel));
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::blend_2on1(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		*outPixels++ = blendPixels2<1, 1>(&inPixels[p]);
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::blend_4on1(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		*outPixels++ = blendPixels4<1, 1, 1, 1>(&inPixels[p]);
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::blend_2on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		*outPixels++ =                     inPixels[p + 0];
		*outPixels++ = blendPixels2<1, 1>(&inPixels[p + 0]);
		*outPixels++ =                     inPixels[p + 1];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::blend_4on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		*outPixels++ = blendPixels2<3, 1>(&inPixels[p + 0]);
		*outPixels++ = blendPixels2<1, 1>(&inPixels[p + 1]);
		*outPixels++ = blendPixels2<1, 3>(&inPixels[p + 2]);
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::blend_8on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 8) {
		*outPixels++ = blendPixels3<3, 3, 2>   (&inPixels[p + 0]);
		*outPixels++ = blendPixels4<1, 3, 3, 1>(&inPixels[p + 2]);
		*outPixels++ = blendPixels3<2, 3, 3>   (&inPixels[p + 5]);
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::blend_none(
	const Pixel* /*inPixels*/, Pixel* outPixels, int nrPixels)
{
	memset(outPixels, 0, nrPixels * sizeof(Pixel));
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::setDisplayMode(V9990DisplayMode mode)
{
	// Select pixel blender
	/*if (zoom == Renderer::ZOOM_256) {
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
	}*/

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

template <class Pixel>
void V9990BitmapConverter<Pixel>::setColorMode(V9990ColorMode mode)
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


template <class Pixel>
void V9990BitmapConverter<Pixel>::drawCursor(
	Pixel* buffer, int displayY, unsigned attrAddr, unsigned patAddr)
{
	int cursorY = vram.readVRAM(attrAddr + 0) +
	             (vram.readVRAM(attrAddr + 2) & 1) * 256;
	++cursorY; // one line later
	int cursorLine = (displayY - cursorY) & 511;
	if (cursorLine >= 32) return;

	byte attr = vram.readVRAM(attrAddr + 6);
	if (attr & 0x10) {
		// don't display
		return;
	}

	unsigned pattern = (vram.readVRAM(patAddr + 4 * cursorLine + 0) << 24)
	                 + (vram.readVRAM(patAddr + 4 * cursorLine + 1) << 16)
	                 + (vram.readVRAM(patAddr + 4 * cursorLine + 2) <<  8)
	                 + (vram.readVRAM(patAddr + 4 * cursorLine + 3) <<  0);
	if (!pattern) {
		// optimization, completely transparant line
		return;
	}
	unsigned x = vram.readVRAM(attrAddr + 4) + (attr & 3) * 256;

	// TODO EOR colors
	Pixel color = palette64[(vdp.getPaletteOffset() << 2) + (attr >> 6)];
	for (int i = 0; i < 32; ++i) {
		if (pattern & 0x80000000) {
			buffer[(x + i) & 1023] = color;
		}
		pattern <<= 1;
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::drawCursors(Pixel* buffer, int displayY)
{
	// TODO which cursor is in front?
	drawCursor(buffer, displayY, 0x7FE00, 0x7FF00);
	drawCursor(buffer, displayY, 0x7FE08, 0x7FF80);
}


template <class Pixel>
void V9990BitmapConverter<Pixel>::convertLine(
	Pixel* linePtr, unsigned address, int nrPixels, int displayY)
{
	assert(nrPixels <= 1024);
	Pixel tmp[1024 * sizeof(Pixel)];
	(this->*rasterMethod)(tmp, address, nrPixels);
	if (vdp.spritesEnabled()) {
		drawCursors(tmp, displayY);
	}
	(this->*blendMethod)(tmp, linePtr, nrPixels);
}


// Force template instantiation
template class V9990BitmapConverter<Uint16>;
template class V9990BitmapConverter<Uint32>;

} // namespace openmsx

