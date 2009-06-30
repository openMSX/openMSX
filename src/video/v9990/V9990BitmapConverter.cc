// $Id$

#include "V9990BitmapConverter.hh"
#include "V9990VRAM.hh"
#include "V9990.hh"
#include "GLUtil.hh"
#include "Math.hh"
#include "static_assert.hh"
#include "type_traits.hh"
#include "build-info.hh"
#include <cassert>

namespace openmsx {

template <class Pixel>
V9990BitmapConverter<Pixel>::V9990BitmapConverter(
		V9990& vdp_, const Pixel* palette64_,
		const Pixel* palette256_, const Pixel* palette32768_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, palette64(palette64_), palette256(palette256_), palette32768(palette32768_)
{
	// make sure function pointers have valid values
	setColorMode(PP);
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYUV(
	Pixel* __restrict pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram.readVRAMBx(address++);
		}

		int u = (data[2] & 7) + ((data[3] & 3) << 3) - ((data[3] & 4) << 3);
		int v = (data[0] & 7) + ((data[1] & 3) << 3) - ((data[1] & 4) << 3);

		for (int i = 0; i < 4; ++i) {
			int y = (data[i] & 0xF8) >> 3;
			int r = Math::clip<0, 31>(y + u);
			int g = Math::clip<0, 31>((5 * y - 2 * u - v) / 4);
			int b = Math::clip<0, 31>(y + v);
			*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
		}
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYUVP(
	Pixel* __restrict pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram.readVRAMBx(address++);
		}

		int u = (data[2] & 7) + ((data[3] & 3) << 3) - ((data[3] & 4) << 3);
		int v = (data[0] & 7) + ((data[1] & 3) << 3) - ((data[1] & 4) << 3);

		for (int i = 0; i < 4; ++i) {
			if (data[i] & 0x08) {
				*pixelPtr++ = palette64[data[i] >> 4];
			} else {
				int y = (data[i] & 0xF8) >> 3;
				int r = Math::clip<0, 31>(y + u);
				int g = Math::clip<0, 31>((5 * y - 2 * u - v) / 4);
				int b = Math::clip<0, 31>(y + v);
				*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
			}
		}
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYJK(
	Pixel* __restrict pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram.readVRAMBx(address++);
		}

		int j = (data[2] & 7) + ((data[3] & 3) << 3) - ((data[3] & 4) << 3);
		int k = (data[0] & 7) + ((data[1] & 3) << 3) - ((data[1] & 4) << 3);

		for (int i = 0; i < 4; ++i) {
			int y = data[i] >> 3;
			int r = Math::clip<0, 31>(y + j);
			int g = Math::clip<0, 31>(y + k);
			int b = Math::clip<0, 31>((5 * y - 2 * j - k) / 4);
			*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
		}
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYJKP(
	Pixel* __restrict pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data[4];
		for (int i = 0; i < 4; ++i) {
			data[i] = vram.readVRAMBx(address++);
		}

		int j = (data[2] & 7) + ((data[3] & 3) << 3) - ((data[3] & 4) << 3);
		int k = (data[0] & 7) + ((data[1] & 3) << 3) - ((data[3] & 4) << 3);

		for (int i = 0; i < 4; ++i) {
			if (data[i] & 0x08) {
				*pixelPtr++ = palette64[data[i] >> 4];
			} else {
				int y = data[i] >> 3;
				int r = Math::clip<0, 31>(y + j);
				int g = Math::clip<0, 31>(y + k);
				int b = Math::clip<0, 31>((5 * y - 2 * j - k) / 4);
				*pixelPtr++ = palette32768[(g << 10) + (r << 5) + b];
			}
		}
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBD16(
	Pixel* __restrict pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		byte low  = vram.readVRAMBx(address++);
		byte high = vram.readVRAMBx(address++);
		*pixelPtr++ = palette32768[(low + 256 * high) & 0x7FFF];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBD8(
	Pixel* __restrict pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		*pixelPtr++ = palette256[vram.readVRAMBx(address++)];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBP6(
	Pixel* __restrict pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		*pixelPtr++ = palette64[vram.readVRAMBx(address++) & 0x3F];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBP4(
	Pixel* __restrict pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		byte data = vram.readVRAMBx(address++);
		*pixelPtr++ = palette64[data >> 4];
		*pixelPtr++ = palette64[data & 0x0F];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBP2(
	Pixel* __restrict pixelPtr, unsigned address, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		byte data = vram.readVRAMBx(address++);
		*pixelPtr++ = palette64[(data & 0xC0) >> 6];
		*pixelPtr++ = palette64[(data & 0x30) >> 4];
		*pixelPtr++ = palette64[(data & 0x0C) >> 2];
		*pixelPtr++ = palette64[(data & 0x03)];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterP(
	Pixel* /*pixelPtr*/, unsigned /*address*/, int /*nrPixels*/)
{
	assert(false);
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
	Pixel* __restrict buffer, int displayY, unsigned attrAddr, unsigned patAddr)
{
	int cursorY = vram.readVRAMBx(attrAddr + 0) +
	             (vram.readVRAMBx(attrAddr + 2) & 1) * 256;
	++cursorY; // one line later
	int cursorLine = (displayY - cursorY) & 511;
	if (cursorLine >= 32) return;

	byte attr = vram.readVRAMBx(attrAddr + 6);
	if (attr & 0x10) {
		// don't display
		return;
	}

	unsigned pattern = (vram.readVRAMBx(patAddr + 4 * cursorLine + 0) << 24)
	                 + (vram.readVRAMBx(patAddr + 4 * cursorLine + 1) << 16)
	                 + (vram.readVRAMBx(patAddr + 4 * cursorLine + 2) <<  8)
	                 + (vram.readVRAMBx(patAddr + 4 * cursorLine + 3) <<  0);
	if (!pattern) {
		// optimization, completely transparant line
		return;
	}
	unsigned x = vram.readVRAMBx(attrAddr + 4) + (attr & 3) * 256;

	// TODO EOR colors
	Pixel color = palette64[vdp.getSpritePaletteOffset() + (attr >> 6)];
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
	// TODO cursor goes wrong when startX != 0
	assert(nrPixels <= 1024);
	(this->*rasterMethod)(linePtr, address, nrPixels);
	if (vdp.spritesEnabled()) {
		drawCursors(linePtr, displayY - vdp.getCursorYOffset());
	}
}

// Force template instantiation
#if HAVE_16BPP
template class V9990BitmapConverter<word>;
#endif
#if HAVE_32BPP
template class V9990BitmapConverter<unsigned>;
#endif

#ifdef COMPONENT_GL
#ifdef _MSC_VER

// The template stuff below fails to compile on VC++, it triggers this error
//   http://msdn.microsoft.com/en-us/library/9045w50z.aspx
// It's not clear whether this is a limitation in VC++ or a C++ extension
// supported by gcc.
// But we know that 'GLuint' and 'unsigned' are the same types in windows,
// so the stuff below is not required (it would only instantiate a dummy class
// when these types are the same).
STATIC_ASSERT((is_same_type<unsigned, GLuint>::value));

#else

template <> class V9990BitmapConverter<GLUtil::NoExpansion> {};
template class V9990BitmapConverter<GLUtil::ExpandGL>;

#endif
#endif // COMPONENT_GL

} // namespace openmsx
