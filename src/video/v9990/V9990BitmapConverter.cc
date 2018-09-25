#include "V9990BitmapConverter.hh"
#include "V9990VRAM.hh"
#include "V9990.hh"
#include "Math.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include "components.hh"
#include <cassert>
#include <cstdint>

namespace openmsx {

template <class Pixel>
V9990BitmapConverter<Pixel>::V9990BitmapConverter(
		V9990& vdp_, const Pixel* palette64_,
		const Pixel* palette256_, const Pixel* palette32768_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, palette64(palette64_), palette256(palette256_), palette32768(palette32768_)
{
	// make sure function pointers have valid values
	setColorMode(PP, B0);
}

template<bool YJK, bool PAL, bool SKIP, typename Pixel>
static inline void draw_YJK_YUV_PAL(
	V9990VRAM& vram,
	const Pixel* __restrict palette64, const Pixel* __restrict palette32768,
	Pixel* __restrict out, unsigned& address, int firstX = 0)
{
	byte data[4];
	for (auto& d : data) {
		d = vram.readVRAMBx(address++);
	}

	int u = (data[2] & 7) + ((data[3] & 3) << 3) - ((data[3] & 4) << 3);
	int v = (data[0] & 7) + ((data[1] & 3) << 3) - ((data[1] & 4) << 3);

	for (int i = SKIP ? firstX : 0; i < 4; ++i) {
		if (PAL && (data[i] & 0x08)) {
			*out++ = palette64[data[i] >> 4];
		} else {
			int y = (data[i] & 0xF8) >> 3;
			int r = Math::clip<0, 31>(y + u);
			int g = Math::clip<0, 31>((5 * y - 2 * u - v) / 4);
			int b = Math::clip<0, 31>(y + v);
			// The only difference between YUV and YJK is that
			// green and blue are swapped.
			if (YJK) std::swap(g, b);
			*out++ = palette32768[(g << 10) + (r << 5) + b];
		}
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYUV(
	Pixel* __restrict out, unsigned x, unsigned y, int nrPixels)
{
	unsigned address = (x & ~3) + y * vdp.getImageWidth();
	if (x & 3) {
		draw_YJK_YUV_PAL<false, false, true>(
			vram, palette64, palette32768, out, address, x & 3);
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		draw_YJK_YUV_PAL<false, false, false>(
			vram, palette64, palette32768, out, address);
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYUVP(
	Pixel* __restrict out, unsigned x, unsigned y, int nrPixels)
{
	// TODO this mode cannot be shown in B4 and higher resolution modes
	//      (So the dual palette for B4 modes is not an issue here.)
	unsigned address = (x & ~3) + y * vdp.getImageWidth();
	if (x & 3) {
		draw_YJK_YUV_PAL<false, true, true>(
			vram, palette64, palette32768, out, address, x & 3);
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		draw_YJK_YUV_PAL<false, true, false>(
			vram, palette64, palette32768, out, address);
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYJK(
	Pixel* __restrict out, unsigned x, unsigned y, int nrPixels)
{
	unsigned address = (x & ~3)+ y * vdp.getImageWidth();
	if (x & 3) {
		draw_YJK_YUV_PAL<true, false, true>(
			vram, palette64, palette32768, out, address, x & 3);
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		draw_YJK_YUV_PAL<true, false, false>(
			vram, palette64, palette32768, out, address);
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBYJKP(
	Pixel* __restrict out, unsigned x, unsigned y, int nrPixels)
{
	// TODO this mode cannot be shown in B4 and higher resolution modes
	//      (So the dual palette for B4 modes is not an issue here.)
	unsigned address = (x & ~3) + y * vdp.getImageWidth();
	if (x & 3) {
		draw_YJK_YUV_PAL<true, true, true>(
			vram, palette64, palette32768, out, address, x & 3);
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		draw_YJK_YUV_PAL<true, true, false>(
			vram, palette64, palette32768, out, address);
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBD16(
	Pixel* __restrict out, unsigned x, unsigned y, int nrPixels)
{
	unsigned address = 2 * (x + y * vdp.getImageWidth());
	if (vdp.isSuperimposing()) {
		Pixel transparant = palette256[0];
		for (/**/; nrPixels > 0; --nrPixels) {
			byte high = vram.readVRAMBx(address + 1);
			if (high & 0x80) {
				*out = transparant;
			} else {
				byte low  = vram.readVRAMBx(address + 0);
				*out = palette32768[low + 256 * high];
			}
			address += 2;
			out += 1;
		}
	} else {
		for (/**/; nrPixels > 0; --nrPixels) {
			byte low  = vram.readVRAMBx(address++);
			byte high = vram.readVRAMBx(address++);
			*out++ = palette32768[(low + 256 * high) & 0x7FFF];
		}
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBD8(
	Pixel* __restrict out, unsigned x, unsigned y, int nrPixels)
{
	unsigned address = x + y * vdp.getImageWidth();
	for (/**/; nrPixels > 0; --nrPixels) {
		*out++ = palette256[vram.readVRAMBx(address++)];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBP6(
	Pixel* __restrict out, unsigned x, unsigned y, int nrPixels)
{
	unsigned address = x + y * vdp.getImageWidth();
	for (/**/; nrPixels > 0; --nrPixels) {
		*out++ = palette64[vram.readVRAMBx(address++) & 0x3F];
	}
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBP4(
	Pixel* __restrict out, unsigned x, unsigned y, int nrPixels)
{
	assert(nrPixels > 0);
	unsigned address = (x + y * vdp.getImageWidth()) / 2;
	byte offset = (vdp.getPaletteOffset() & 0xC) << 2;
	const Pixel* pal = &palette64[offset];
	if (x & 1) {
		byte data = vram.readVRAMBx(address++);
		*out++ = pal[data & 0x0F];
		--nrPixels;
	}
	for (/**/; nrPixels > 0; nrPixels -= 2) {
		byte data = vram.readVRAMBx(address++);
		*out++ = pal[data >> 4];
		*out++ = pal[data & 0x0F];
	}
	// Note: this possibly draws 1 pixel too many, but that's ok.
}
template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBP4HiRes(
	Pixel* __restrict out, unsigned x, unsigned y, int nrPixels)
{
	// Verified on real HW:
	//   Bit PLT05 in palette offset is ignored, instead for even pixels
	//   bit 'PLT05' is '0', for odd pixels it's '1'.
	unsigned address = (x + y * vdp.getImageWidth()) / 2;
	byte offset = (vdp.getPaletteOffset() & 0x4) << 2;
	const Pixel* pal1 = &palette64[offset |  0];
	const Pixel* pal2 = &palette64[offset | 32];
	if (x & 1) {
		byte data = vram.readVRAMBx(address++);
		*out++ = pal2[data & 0x0F];
		--nrPixels;
	}
	for (/**/; nrPixels > 0; nrPixels -= 2) {
		byte data = vram.readVRAMBx(address++);
		*out++ = pal1[data >> 4  ];
		*out++ = pal2[data & 0x0F];
	}
	// Note: this possibly draws 1 pixel too many, but that's ok.
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBP2(
	Pixel* __restrict out, unsigned x, unsigned y, int nrPixels)
{
	assert(nrPixels > 0);
	unsigned address = (x + y * vdp.getImageWidth()) / 4;
	byte offset = vdp.getPaletteOffset() << 2;
	const Pixel* pal = &palette64[offset];
	if (x & 3) {
		byte data = vram.readVRAMBx(address++);
		if ((x & 3) <= 1) *out++ = pal[(data & 0x30) >> 4];
		if ((x & 3) <= 2) *out++ = pal[(data & 0x0C) >> 2];
		if (true)         *out++ = pal[(data & 0x03) >> 0];
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		byte data = vram.readVRAMBx(address++);
		*out++ = pal[(data & 0xC0) >> 6];
		*out++ = pal[(data & 0x30) >> 4];
		*out++ = pal[(data & 0x0C) >> 2];
		*out++ = pal[(data & 0x03) >> 0];
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}
template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterBP2HiRes(
	Pixel* __restrict out, unsigned x, unsigned y, int nrPixels)
{
	// Verified on real HW:
	//   Bit PLT05 in palette offset is ignored, instead for even pixels
	//   bit 'PLT05' is '0', for odd pixels it's '1'.
	assert(nrPixels > 0);
	unsigned address = (x + y * vdp.getImageWidth()) / 4;
	byte offset = (vdp.getPaletteOffset() & 0x7) << 2;
	const Pixel* pal1 = &palette64[offset |  0];
	const Pixel* pal2 = &palette64[offset | 32];
	if (x & 3) {
		byte data = vram.readVRAMBx(address++);
		if ((x & 3) <= 1) *out++ = pal2[(data & 0x30) >> 4];
		if ((x & 3) <= 2) *out++ = pal1[(data & 0x0C) >> 2];
		if (true)         *out++ = pal2[(data & 0x03) >> 0];
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		byte data = vram.readVRAMBx(address++);
		*out++ = pal1[(data & 0xC0) >> 6];
		*out++ = pal2[(data & 0x30) >> 4];
		*out++ = pal1[(data & 0x0C) >> 2];
		*out++ = pal2[(data & 0x03) >> 0];
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}

template <class Pixel>
void V9990BitmapConverter<Pixel>::rasterP(
	Pixel* /*out*/, unsigned /*x*/, unsigned /*y*/, int /*nrPixels*/)
{
	UNREACHABLE;
}

static bool isHighRes(V9990DisplayMode display)
{
	return (display == B4) || (display == B5) ||
	       (display == B6) || (display == B7);
}
template <class Pixel>
void V9990BitmapConverter<Pixel>::setColorMode(V9990ColorMode color,
                                               V9990DisplayMode display)
{
	switch (color) {
	case PP:    rasterMethod = &V9990BitmapConverter::rasterP;     break;
	case BYUV:  rasterMethod = &V9990BitmapConverter::rasterBYUV;  break;
	case BYUVP: rasterMethod = &V9990BitmapConverter::rasterBYUVP; break;
	case BYJK:  rasterMethod = &V9990BitmapConverter::rasterBYJK;  break;
	case BYJKP: rasterMethod = &V9990BitmapConverter::rasterBYJKP; break;
	case BD16:  rasterMethod = &V9990BitmapConverter::rasterBD16;  break;
	case BD8:   rasterMethod = &V9990BitmapConverter::rasterBD8;   break;
	case BP6:   rasterMethod = &V9990BitmapConverter::rasterBP6;   break;
	case BP4:   rasterMethod = isHighRes(display) ?
	                           &V9990BitmapConverter::rasterBP4HiRes :
	                           &V9990BitmapConverter::rasterBP4;   break;
	case BP2:   rasterMethod = isHighRes(display) ?
	                           &V9990BitmapConverter::rasterBP2HiRes :
	                           &V9990BitmapConverter::rasterBP2;   break;
	default:    UNREACHABLE;
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
	// TODO investigate dual palette in B4 and higher modes
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
	drawCursor(buffer, displayY, 0x7FE08, 0x7FF80);
	drawCursor(buffer, displayY, 0x7FE00, 0x7FF00);
}


template <class Pixel>
void V9990BitmapConverter<Pixel>::convertLine(
	Pixel* linePtr, unsigned x, unsigned y, int nrPixels,
	int cursorY, bool drawSprites)
{
	// TODO cursor goes wrong when startX != 0
	assert(nrPixels <= 1024);
	(this->*rasterMethod)(linePtr, x, y, nrPixels);
	if (drawSprites) {
		drawCursors(linePtr, cursorY);
	}
}

// Force template instantiation
#if HAVE_16BPP
template class V9990BitmapConverter<uint16_t>;
#endif
#if HAVE_32BPP || COMPONENT_GL
template class V9990BitmapConverter<uint32_t>;
#endif

} // namespace openmsx
