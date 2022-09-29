#include "V9990BitmapConverter.hh"
#include "V9990VRAM.hh"
#include "V9990.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include "components.hh"
#include <cassert>
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel>
V9990BitmapConverter<Pixel>::V9990BitmapConverter(
		V9990& vdp_,
		std::span<const Pixel,    64> palette64_,  std::span<const int16_t,  64> palette64_32768_,
		std::span<const Pixel,   256> palette256_, std::span<const int16_t, 256> palette256_32768_,
		std::span<const Pixel, 32768> palette32768_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, palette64 (palette64_ ), palette64_32768 (palette64_32768_ )
	, palette256(palette256_), palette256_32768(palette256_32768_)
	, palette32768(palette32768_)
{
	setColorMode(PP, B0); // initialize with dummy values
}

template<bool YJK, bool PAL, bool SKIP, std::unsigned_integral Pixel, typename ColorLookup>
static inline void draw_YJK_YUV_PAL(
	ColorLookup color, V9990VRAM& vram,
	Pixel* __restrict& out, unsigned& address, int firstX = 0)
{
	byte data[4];
	for (auto& d : data) {
		d = vram.readVRAMBx(address++);
	}

	int u = (data[2] & 7) + ((data[3] & 3) << 3) - ((data[3] & 4) << 3);
	int v = (data[0] & 7) + ((data[1] & 3) << 3) - ((data[1] & 4) << 3);

	for (auto i : xrange(SKIP ? firstX : 0, 4)) {
		if (PAL && (data[i] & 0x08)) {
			*out++ = color.lookup64(data[i] >> 4);
		} else {
			int y = (data[i] & 0xF8) >> 3;
			int r = std::clamp(y + u,                   0, 31);
			int g = std::clamp((5 * y - 2 * u - v) / 4, 0, 31);
			int b = std::clamp(y + v,                   0, 31);
			// The only difference between YUV and YJK is that
			// green and blue are swapped.
			if constexpr (YJK) std::swap(g, b);
			*out++ = color.lookup32768((g << 10) + (r << 5) + b);
		}
	}
}

template<std::unsigned_integral Pixel, typename ColorLookup>
static void rasterBYUV(
	ColorLookup color, V9990& vdp, V9990VRAM& vram,
	std::span<Pixel> buf, unsigned x, unsigned y)
{
	Pixel* __restrict out = buf.data();
	int nrPixels = buf.size();
	unsigned address = (x & ~3) + y * vdp.getImageWidth();
	if (x & 3) {
		draw_YJK_YUV_PAL<false, false, true>(
			color, vram, out, address, x & 3);
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		draw_YJK_YUV_PAL<false, false, false>(
			color, vram, out, address);
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}

template<std::unsigned_integral Pixel, typename ColorLookup>
static void rasterBYUVP(
	ColorLookup color, V9990& vdp, V9990VRAM& vram,
	std::span<Pixel> buf, unsigned x, unsigned y)
{
	// TODO this mode cannot be shown in B4 and higher resolution modes
	//      (So the dual palette for B4 modes is not an issue here.)
	Pixel* __restrict out = buf.data();
	int nrPixels = buf.size();
	unsigned address = (x & ~3) + y * vdp.getImageWidth();
	if (x & 3) {
		draw_YJK_YUV_PAL<false, true, true>(
			color, vram, out, address, x & 3);
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		draw_YJK_YUV_PAL<false, true, false>(
			color, vram, out, address);
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}

template<std::unsigned_integral Pixel, typename ColorLookup>
static void rasterBYJK(
	ColorLookup color, V9990& vdp, V9990VRAM& vram,
	std::span<Pixel> buf, unsigned x, unsigned y)
{
	Pixel* __restrict out = buf.data();
	int nrPixels = buf.size();
	unsigned address = (x & ~3) + y * vdp.getImageWidth();
	if (x & 3) {
		draw_YJK_YUV_PAL<true, false, true>(
			color, vram, out, address, x & 3);
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		draw_YJK_YUV_PAL<true, false, false>(
			color, vram, out, address);
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}

template<std::unsigned_integral Pixel, typename ColorLookup>
static void rasterBYJKP(
	ColorLookup color, V9990& vdp, V9990VRAM& vram,
	std::span<Pixel> buf, unsigned x, unsigned y)
{
	// TODO this mode cannot be shown in B4 and higher resolution modes
	//      (So the dual palette for B4 modes is not an issue here.)
	Pixel* __restrict out = buf.data();
	int nrPixels = buf.size();
	unsigned address = (x & ~3) + y * vdp.getImageWidth();
	if (x & 3) {
		draw_YJK_YUV_PAL<true, true, true>(
			color, vram, out, address, x & 3);
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		draw_YJK_YUV_PAL<true, true, false>(
			color, vram, out, address);
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}

template<std::unsigned_integral Pixel, typename ColorLookup>
static void rasterBD16(
	ColorLookup color, V9990& vdp, V9990VRAM& vram,
	std::span<Pixel> buf, unsigned x, unsigned y)
{
	Pixel* __restrict out = buf.data();
	int nrPixels = buf.size();
	unsigned address = 2 * (x + y * vdp.getImageWidth());
	if (vdp.isSuperimposing()) {
		auto transparant = color.lookup256(0);
		for (/**/; nrPixels > 0; --nrPixels) {
			byte high = vram.readVRAMBx(address + 1);
			if (high & 0x80) {
				*out = transparant;
			} else {
				byte low  = vram.readVRAMBx(address + 0);
				*out = color.lookup32768(low + 256 * high);
			}
			address += 2;
			out += 1;
		}
	} else {
		for (/**/; nrPixels > 0; --nrPixels) {
			byte low  = vram.readVRAMBx(address++);
			byte high = vram.readVRAMBx(address++);
			*out++ = color.lookup32768((low + 256 * high) & 0x7FFF);
		}
	}
}

template<std::unsigned_integral Pixel, typename ColorLookup>
static void rasterBD8(
	ColorLookup color, V9990& vdp, V9990VRAM& vram,
	std::span<Pixel> buf, unsigned x, unsigned y)
{
	Pixel* __restrict out = buf.data();
	int nrPixels = buf.size();
	unsigned address = x + y * vdp.getImageWidth();
	for (/**/; nrPixels > 0; --nrPixels) {
		*out++ = color.lookup256(vram.readVRAMBx(address++));
	}
}

template<std::unsigned_integral Pixel, typename ColorLookup>
static void rasterBP6(
	ColorLookup color, V9990& vdp, V9990VRAM& vram,
	std::span<Pixel> buf, unsigned x, unsigned y)
{
	Pixel* __restrict out = buf.data();
	int nrPixels = buf.size();
	unsigned address = x + y * vdp.getImageWidth();
	for (/**/; nrPixels > 0; --nrPixels) {
		*out++ = color.lookup64(vram.readVRAMBx(address++) & 0x3F);
	}
}

template<std::unsigned_integral Pixel, typename ColorLookup>
static void rasterBP4(
	ColorLookup color, V9990& vdp, V9990VRAM& vram,
	std::span<Pixel> buf, unsigned x, unsigned y)
{
	Pixel* __restrict out = buf.data();
	int nrPixels = buf.size();
	assert(nrPixels > 0);
	unsigned address = (x + y * vdp.getImageWidth()) / 2;
	color.set64Offset((vdp.getPaletteOffset() & 0xC) << 2);
	if (x & 1) {
		byte data = vram.readVRAMBx(address++);
		*out++ = color.lookup64(data & 0x0F);
		--nrPixels;
	}
	for (/**/; nrPixels > 0; nrPixels -= 2) {
		byte data = vram.readVRAMBx(address++);
		*out++ = color.lookup64(data >> 4);
		*out++ = color.lookup64(data & 0x0F);
	}
	// Note: this possibly draws 1 pixel too many, but that's ok.
}
template<std::unsigned_integral Pixel, typename ColorLookup>
static void rasterBP4HiRes(
	ColorLookup color, V9990& vdp, V9990VRAM& vram,
	std::span<Pixel> buf, unsigned x, unsigned y)
{
	// Verified on real HW:
	//   Bit PLT05 in palette offset is ignored, instead for even pixels
	//   bit 'PLT05' is '0', for odd pixels it's '1'.
	Pixel* __restrict out = buf.data();
	int nrPixels = buf.size();
	unsigned address = (x + y * vdp.getImageWidth()) / 2;
	color.set64Offset((vdp.getPaletteOffset() & 0x4) << 2);
	if (x & 1) {
		byte data = vram.readVRAMBx(address++);
		*out++ = color.lookup64(32 | (data & 0x0F));
		--nrPixels;
	}
	for (/**/; nrPixels > 0; nrPixels -= 2) {
		byte data = vram.readVRAMBx(address++);
		*out++ = color.lookup64( 0 | (data >> 4  ));
		*out++ = color.lookup64(32 | (data & 0x0F));
	}
	// Note: this possibly draws 1 pixel too many, but that's ok.
}

template<std::unsigned_integral Pixel, typename ColorLookup>
static void rasterBP2(
	ColorLookup color, V9990& vdp, V9990VRAM& vram,
	std::span<Pixel> buf, unsigned x, unsigned y)
{
	Pixel* __restrict out = buf.data();
	int nrPixels = buf.size();
	assert(nrPixels > 0);
	unsigned address = (x + y * vdp.getImageWidth()) / 4;
	color.set64Offset(vdp.getPaletteOffset() << 2);
	if (x & 3) {
		byte data = vram.readVRAMBx(address++);
		if ((x & 3) <= 1) *out++ = color.lookup64((data & 0x30) >> 4);
		if ((x & 3) <= 2) *out++ = color.lookup64((data & 0x0C) >> 2);
		if (true)         *out++ = color.lookup64((data & 0x03) >> 0);
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		byte data = vram.readVRAMBx(address++);
		*out++ = color.lookup64((data & 0xC0) >> 6);
		*out++ = color.lookup64((data & 0x30) >> 4);
		*out++ = color.lookup64((data & 0x0C) >> 2);
		*out++ = color.lookup64((data & 0x03) >> 0);
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}
template<std::unsigned_integral Pixel, typename ColorLookup>
static void rasterBP2HiRes(
	ColorLookup color, V9990& vdp, V9990VRAM& vram,
	std::span<Pixel> buf, unsigned x, unsigned y)
{
	// Verified on real HW:
	//   Bit PLT05 in palette offset is ignored, instead for even pixels
	//   bit 'PLT05' is '0', for odd pixels it's '1'.
	Pixel* __restrict out = buf.data();
	int nrPixels = buf.size();
	assert(nrPixels > 0);
	unsigned address = (x + y * vdp.getImageWidth()) / 4;
	color.set64Offset((vdp.getPaletteOffset() & 0x7) << 2);
	if (x & 3) {
		byte data = vram.readVRAMBx(address++);
		if ((x & 3) <= 1) *out++ = color.lookup64(32 | ((data & 0x30) >> 4));
		if ((x & 3) <= 2) *out++ = color.lookup64( 0 | ((data & 0x0C) >> 2));
		if (true)         *out++ = color.lookup64(32 | ((data & 0x03) >> 0));
		nrPixels -= 4 - (x & 3);
	}
	for (/**/; nrPixels > 0; nrPixels -= 4) {
		byte data = vram.readVRAMBx(address++);
		*out++ = color.lookup64( 0 | ((data & 0xC0) >> 6));
		*out++ = color.lookup64(32 | ((data & 0x30) >> 4));
		*out++ = color.lookup64( 0 | ((data & 0x0C) >> 2));
		*out++ = color.lookup64(32 | ((data & 0x03) >> 0));
	}
	// Note: this can draw up to 3 pixels too many, but that's ok.
}

// Helper class to translate V9990 palette indices into host Pixel values.
template<std::unsigned_integral Pixel>
class PaletteLookup
{
public:
	PaletteLookup(std::span<const Pixel,    64> palette64_,
	              std::span<const Pixel,   256> palette256_,
	              std::span<const Pixel, 32768> palette32768_)
	        : palette64Base(palette64_)
	        , palette64(palette64_)
	        , palette256(palette256_)
	        , palette32768(palette32768_)
	{
	}

	void set64Offset(size_t offset) { palette64 = palette64Base.subspan(offset); }
	[[nodiscard]] Pixel lookup64   (size_t idx) const { return palette64   [idx]; }
	[[nodiscard]] Pixel lookup256  (size_t idx) const { return palette256  [idx]; }
	[[nodiscard]] Pixel lookup32768(size_t idx) const { return palette32768[idx]; }

private:
	std::span<const Pixel,    64> palette64Base;
	std::span<const Pixel>        palette64;
	std::span<const Pixel,   256> palette256;
	std::span<const Pixel, 32768> palette32768;
};

// Helper class to translate V9990 palette indices (64-entry, 256-entry and
// 32768-entry palettes) into V9990 32768-entry palette indices.
class IndexLookup
{
public:
	IndexLookup(std::span<const int16_t, 64> palette64_, std::span<const int16_t, 256> palette256_)
	        : palette64_32768Base(palette64_)
	        , palette64_32768(palette64_)
	        , palette256_32768(palette256_)
	{
	}

	void set64Offset(size_t offset) { palette64_32768 = palette64_32768Base.subspan(offset); }
	[[nodiscard]] int16_t lookup64   (size_t idx) const { return palette64_32768 [idx]; }
	[[nodiscard]] int16_t lookup256  (size_t idx) const { return palette256_32768[idx]; }
	[[nodiscard]] int16_t lookup32768(size_t idx) const { return int16_t(idx); }

private:
	std::span<const int16_t,  64> palette64_32768Base;
	std::span<const int16_t>      palette64_32768;
	std::span<const int16_t, 256> palette256_32768;
};

template<std::unsigned_integral Pixel, typename ColorLookup>
static void raster(V9990ColorMode colorMode, bool highRes,
                   ColorLookup color, V9990& vdp, V9990VRAM& vram,
                   std::span<Pixel> out, unsigned x, unsigned y)
{
	switch (colorMode) {
	case BYUV:  return rasterBYUV <Pixel>(color, vdp, vram, out, x, y);
	case BYUVP: return rasterBYUVP<Pixel>(color, vdp, vram, out, x, y);
	case BYJK:  return rasterBYJK <Pixel>(color, vdp, vram, out, x, y);
	case BYJKP: return rasterBYJKP<Pixel>(color, vdp, vram, out, x, y);
	case BD16:  return rasterBD16 <Pixel>(color, vdp, vram, out, x, y);
	case BD8:   return rasterBD8  <Pixel>(color, vdp, vram, out, x, y);
	case BP6:   return rasterBP6  <Pixel>(color, vdp, vram, out, x, y);
	case BP4:   return highRes ? rasterBP4HiRes<Pixel>(color, vdp, vram, out, x, y)
	                           : rasterBP4     <Pixel>(color, vdp, vram, out, x, y);
	case BP2:   return highRes ? rasterBP2HiRes<Pixel>(color, vdp, vram, out, x, y)
	                           : rasterBP2     <Pixel>(color, vdp, vram, out, x, y);
	default:    UNREACHABLE;
	}
}

// Missing details in the V9990 application manual (reverse engineered from
// tests on a real gfx9000):
//  * Cursor 0 is drawn on top of cursor 1  (IOW cursor 0 has higher priority).
//    This remains the case when both cursors use the 'EOR' feature (it's not
//    so that EOR is applied twice).
//  * The CC1,CC0,EOR bits in the cursor attribute table work like this:
//     (CC1,CC0):
//       when (0,0): pick the resulting color from bitmap rendering (this is
//                   a 15 bit RGB value)
//       when (x,y): pick the color from palette with index R#28:x:y (this is
//                   also a 15 bit RGB color)
//     (EOR):
//       when 0: use the above color unchanged
//       when 1: flip all the bits in the above color (IOW XOR with 0x7fff)
//    From this follows:
//      (CC1,CC0,EOR)==(0,0,0):
//        Results in an invisible cursor: each pixel is colored the same as the
//        corresponding background pixel.
//      (CC1,CC0,EOR)==(0,0,1):
//        This is the only combination where the cursor is drawn using multiple
//        colors, each pixel is the complement of the corresponsing background
//        pixel.
//      (CC1,CC0,EOR)==(x,y,0):
//        This is the 'usual' configuration, cursor is drawn with a specific
//        color from the palette (also when bitmap rendering is not using the
//        palette, e.g. YJK or BD8 mode).
//      (CC1,CC0,EOR)==(x,y,1):
//        This undocumented mode draws the cursor with a single color which is
//        the complement of a specific palette color.
class CursorInfo
{
public:
	CursorInfo(V9990& vdp, V9990VRAM& vram, std::span<const int16_t, 64> palette64_32768,
	           unsigned attrAddr, unsigned patAddr,
		   int displayY, bool drawCursor)
	{
		x = unsigned(-1); // means not visible
		// initialize these 3 to avoid warning
		pattern = 0;
		color = 0;
		doXor = false;

		if (!drawCursor) return;

		unsigned attrY = vram.readVRAMBx(attrAddr + 0) +
		                (vram.readVRAMBx(attrAddr + 2) & 1) * 256;
		attrY += vdp.isInterlaced() ? 2 : 1; // 1 or 2 lines later
		unsigned cursorLine = (displayY - attrY) & 511;
		if (cursorLine >= 32) return;

		byte attr = vram.readVRAMBx(attrAddr + 6);
		if ((attr & 0x10) || ((attr & 0xe0) == 0x00)) {
			// don't display
			return;
		}

		pattern = (vram.readVRAMBx(patAddr + 4 * cursorLine + 0) << 24)
		        + (vram.readVRAMBx(patAddr + 4 * cursorLine + 1) << 16)
		        + (vram.readVRAMBx(patAddr + 4 * cursorLine + 2) <<  8)
		        + (vram.readVRAMBx(patAddr + 4 * cursorLine + 3) <<  0);
		if (pattern == 0) {
			// optimization, completely transparant line
			return;
		}

		// mark cursor visible
		x = vram.readVRAMBx(attrAddr + 4) + (attr & 3) * 256;

		doXor = (attr & 0xe0) == 0x20;

		auto colorIdx = vdp.getSpritePaletteOffset() + (attr >> 6);
		color = palette64_32768[colorIdx];
		if (attr & 0x20) color ^= 0x7fff;
	}

	[[nodiscard]] bool isVisible() const {
		return x != unsigned(-1);
	}
	[[nodiscard]] bool dot() const {
		return (x == 0) && (pattern & 0x80000000);
	}
	void shift() {
		if (x) {
			--x;
		} else {
			pattern <<= 1;
		}
	}

public:
	unsigned x;
	uint32_t pattern;
	int16_t color;
	bool doXor;
};

template<std::unsigned_integral Pixel>
void V9990BitmapConverter<Pixel>::convertLine(
	std::span<Pixel> dst, unsigned x, unsigned y,
	int cursorY, bool drawCursors)
{
	assert(dst.size() <= 1024);

	CursorInfo cursor0(vdp, vram, palette64_32768, 0x7fe00, 0x7ff00, cursorY, drawCursors);
	CursorInfo cursor1(vdp, vram, palette64_32768, 0x7fe08, 0x7ff80, cursorY, drawCursors);

	if (cursor0.isVisible() || cursor1.isVisible()) {
		// raster background into a temporary buffer
		uint16_t buf[1024];
		raster(colorMode, highRes,
		       IndexLookup(palette64_32768, palette256_32768),
		       vdp, vram,
		       subspan(buf, dst.size()), x, y);

		// draw sprites in this buffer
		// TODO can be optimized
		// TODO probably goes wrong when startX != 0
		// TODO investigate dual palette in B4 and higher modes
		// TODO check X-roll behavior
		for (auto i : xrange(dst.size())) {
			if (cursor0.dot()) {
				if (cursor0.doXor) {
					buf[i] ^= 0x7fff;
				} else {
					buf[i] = cursor0.color;
				}
			} else if (cursor1.dot()) {
				if (cursor1.doXor) {
					buf[i] ^= 0x7fff;
				} else {
					buf[i] = cursor1.color;
				}
			}
			cursor0.shift();
			cursor1.shift();
			if ((cursor0.pattern == 0) && (cursor1.pattern == 0)) break;
		}

		// copy buffer to destination, translate from V9990 to host colors
		for (auto i : xrange(dst.size())) {
			dst[i] = palette32768[buf[i]];
		}
	} else {
		// Optimization: no cursor(s) visible on this line, directly draw to destination
		raster(colorMode, highRes,
		       PaletteLookup<Pixel>(palette64, palette256, palette32768),
		       vdp, vram, dst, x, y);
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
