/*
TODO:
- Clean up renderGraphics2, it is currently very hard to understand
  with all the masks and quarters etc.
- Correctly implement vertical scroll in text modes.
  Can be implemented by reordering blitting, but uses a smaller
  wrap than GFX modes: 8 lines instead of 256 lines.
*/

#include "CharacterConverter.hh"

#include "VDP.hh"
#include "VDPVRAM.hh"

#include "ranges.hh"
#include "xrange.hh"

#include <bit>
#include <cstdint>

#ifdef __SSE2__
#include "emmintrin.h" // SSE2
#endif

namespace openmsx {

using Pixel = CharacterConverter::Pixel;

CharacterConverter::CharacterConverter(
	VDP& vdp_, std::span<const Pixel, 16> palFg_, std::span<const Pixel, 16> palBg_)
	: vdp(vdp_), vram(vdp.getVRAM()), palFg(palFg_), palBg(palBg_)
{
}

void CharacterConverter::setDisplayMode(DisplayMode mode)
{
	modeBase = mode.getBase();
	assert(modeBase < 0x0C);
}

void CharacterConverter::convertLine(std::span<Pixel> buf, int line) const
{
	// TODO: Support YJK on modes other than Graphic 6/7.
	switch (modeBase) {
	case DisplayMode::GRAPHIC1:   // screen 1
		renderGraphic1(subspan<256>(buf), line);
		break;
	case DisplayMode::TEXT1:      // screen 0, width 40
		renderText1(subspan<256>(buf), line);
		break;
	case DisplayMode::MULTICOLOR: // screen 3
		renderMulti(subspan<256>(buf), line);
		break;
	case DisplayMode::GRAPHIC2:   // screen 2
		renderGraphic2(subspan<256>(buf), line);
		break;
	case DisplayMode::GRAPHIC3:   // screen 4
		renderGraphic2(subspan<256>(buf), line); // graphic3, actually
		break;
	case  DisplayMode::TEXT2:     // screen 0, width 80
		renderText2(subspan<512>(buf), line);
		break;
	case DisplayMode::TEXT1Q:     // TMSxxxx only
		if (vdp.isMSX1VDP()) {
			renderText1Q(subspan<256>(buf), line);
		} else {
			renderBlank (subspan<256>(buf));
		}
		break;
	case DisplayMode::MULTIQ:     // TMSxxxx only
		if (vdp.isMSX1VDP()) {
			renderMultiQ(subspan<256>(buf), line);
		} else {
			renderBlank (subspan<256>(buf));
		}
		break;
	default: // remaining (non-bitmap) modes
		if (vdp.isMSX1VDP()) {
			renderBogus(subspan<256>(buf));
		} else {
			renderBlank(subspan<256>(buf));
		}
	}
}

#ifdef __SSE2__
// Copied from Scale2xScaler.cc, TODO move to common location?
static inline __m128i select(__m128i a0, __m128i a1, __m128i mask)
{
	return _mm_xor_si128(_mm_and_si128(_mm_xor_si128(a0, a1), mask), a0);
}
#endif

static inline void draw6(
	Pixel* __restrict & pixelPtr, Pixel fg, Pixel bg, byte pattern)
{
	pixelPtr[0] = (pattern & 0x80) ? fg : bg;
	pixelPtr[1] = (pattern & 0x40) ? fg : bg;
	pixelPtr[2] = (pattern & 0x20) ? fg : bg;
	pixelPtr[3] = (pattern & 0x10) ? fg : bg;
	pixelPtr[4] = (pattern & 0x08) ? fg : bg;
	pixelPtr[5] = (pattern & 0x04) ? fg : bg;
	pixelPtr += 6;
}

static inline void draw8(
	Pixel* __restrict & pixelPtr, Pixel fg, Pixel bg, byte pattern)
{
#ifdef __SSE2__
	// SSE2 version, 32bpp
	const __m128i m74 = _mm_set_epi32(0x10, 0x20, 0x40, 0x80);
	const __m128i m30 = _mm_set_epi32(0x01, 0x02, 0x04, 0x08);
	const __m128i zero = _mm_setzero_si128();

	__m128i fg4 = _mm_set1_epi32(fg);
	__m128i bg4 = _mm_set1_epi32(bg);
	__m128i pat = _mm_set1_epi32(pattern);

	__m128i b74 = _mm_cmpeq_epi32(_mm_and_si128(pat, m74), zero);
	__m128i b30 = _mm_cmpeq_epi32(_mm_and_si128(pat, m30), zero);

	auto* out = std::bit_cast<__m128i*>(pixelPtr);
	_mm_storeu_si128(out + 0, select(fg4, bg4, b74));
	_mm_storeu_si128(out + 1, select(fg4, bg4, b30));
	pixelPtr += 8;
	return;
#endif

	// C++ version
	pixelPtr[0] = (pattern & 0x80) ? fg : bg;
	pixelPtr[1] = (pattern & 0x40) ? fg : bg;
	pixelPtr[2] = (pattern & 0x20) ? fg : bg;
	pixelPtr[3] = (pattern & 0x10) ? fg : bg;
	pixelPtr[4] = (pattern & 0x08) ? fg : bg;
	pixelPtr[5] = (pattern & 0x04) ? fg : bg;
	pixelPtr[6] = (pattern & 0x02) ? fg : bg;
	pixelPtr[7] = (pattern & 0x01) ? fg : bg;
	pixelPtr += 8;
}

void CharacterConverter::renderText1(std::span<Pixel, 256> buf, int line) const
{
	Pixel fg = palFg[vdp.getForegroundColor()];
	Pixel bg = palFg[vdp.getBackgroundColor()];

	// 8 * 256 is small enough to always be contiguous
	auto patternArea = vram.patternTable.getReadArea<256 * 8>(0);
	auto l = (line + vdp.getVerticalScroll()) & 7;

	// Note: Because line width is not a power of two, reading an entire line
	//       from a VRAM pointer returned by readArea will not wrap the index
	//       correctly. Therefore we read one character at a time.
	unsigned nameStart = (line / 8) * 40;
	unsigned nameEnd = nameStart + 40;
	Pixel* __restrict pixelPtr = buf.data();
	for (auto name : xrange(nameStart, nameEnd)) {
		unsigned charCode = vram.nameTable.readNP((name + 0xC00) | (~0u << 12));
		auto pattern = patternArea[l + charCode * 8];
		draw6(pixelPtr, fg, bg, pattern);
	}
}

void CharacterConverter::renderText1Q(std::span<Pixel, 256> buf, int line) const
{
	Pixel fg = palFg[vdp.getForegroundColor()];
	Pixel bg = palFg[vdp.getBackgroundColor()];

	unsigned patternBaseLine = (~0u << 13) | ((line + vdp.getVerticalScroll()) & 7);

	// Note: Because line width is not a power of two, reading an entire line
	//       from a VRAM pointer returned by readArea will not wrap the index
	//       correctly. Therefore we read one character at a time.
	unsigned nameStart = (line / 8) * 40;
	unsigned nameEnd = nameStart + 40;
	unsigned patternQuarter = (line & 0xC0) << 2;
	Pixel* __restrict pixelPtr = buf.data();
	for (auto name : xrange(nameStart, nameEnd)) {
		unsigned charCode = vram.nameTable.readNP((name + 0xC00) | (~0u << 12));
		unsigned patternNr = patternQuarter | charCode;
		auto pattern = vram.patternTable.readNP(
			patternBaseLine | (patternNr * 8));
		draw6(pixelPtr, fg, bg, pattern);
	}
}

void CharacterConverter::renderText2(std::span<Pixel, 512> buf, int line) const
{
	Pixel plainFg = palFg[vdp.getForegroundColor()];
	Pixel plainBg = palFg[vdp.getBackgroundColor()];
	Pixel blinkFg, blinkBg;
	if (vdp.getBlinkState()) {
		int fg = vdp.getBlinkForegroundColor();
		blinkFg = palBg[fg ? fg : vdp.getBlinkBackgroundColor()];
		blinkBg = palBg[vdp.getBlinkBackgroundColor()];
	} else {
		blinkFg = plainFg;
		blinkBg = plainBg;
	}

	// 8 * 256 is small enough to always be contiguous
	auto patternArea = vram.patternTable.getReadArea<256 * 8>(0);
	auto l = (line + vdp.getVerticalScroll()) & 7;

	unsigned colorStart = (line / 8) * (80 / 8);
	unsigned nameStart  = (line / 8) * 80;
	Pixel* __restrict pixelPtr = buf.data();
	for (auto i : xrange(80 / 8)) {
		unsigned colorPattern = vram.colorTable.readNP(
			(colorStart + i) | (~0u << 9));
		auto nameArea = vram.nameTable.getReadArea<8>(
			(nameStart + 8 * i) | (~0u << 12));
		draw6(pixelPtr,
		      (colorPattern & 0x80) ? blinkFg : plainFg,
		      (colorPattern & 0x80) ? blinkBg : plainBg,
		      patternArea[l + nameArea[0] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x40) ? blinkFg : plainFg,
		      (colorPattern & 0x40) ? blinkBg : plainBg,
		      patternArea[l + nameArea[1] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x20) ? blinkFg : plainFg,
		      (colorPattern & 0x20) ? blinkBg : plainBg,
		      patternArea[l + nameArea[2] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x10) ? blinkFg : plainFg,
		      (colorPattern & 0x10) ? blinkBg : plainBg,
		      patternArea[l + nameArea[3] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x08) ? blinkFg : plainFg,
		      (colorPattern & 0x08) ? blinkBg : plainBg,
		      patternArea[l + nameArea[4] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x04) ? blinkFg : plainFg,
		      (colorPattern & 0x04) ? blinkBg : plainBg,
		      patternArea[l + nameArea[5] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x02) ? blinkFg : plainFg,
		      (colorPattern & 0x02) ? blinkBg : plainBg,
		      patternArea[l + nameArea[6] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x01) ? blinkFg : plainFg,
		      (colorPattern & 0x01) ? blinkBg : plainBg,
		      patternArea[l + nameArea[7] * 8]);
	}
}

std::span<const byte, 32> CharacterConverter::getNamePtr(int line, int scroll) const
{
	// no need to test whether multi-page scrolling is enabled,
	// indexMask in the nameTable already takes care of it
	return vram.nameTable.getReadArea<32>(
		((line / 8) * 32) | ((scroll & 0x20) ? 0x8000 : 0));
}
void CharacterConverter::renderGraphic1(std::span<Pixel, 256> buf, int line) const
{
	auto patternArea = vram.patternTable.getReadArea<256 * 8>(0);
	auto l = line & 7;
	auto colorArea = vram.colorTable.getReadArea<256 / 8>(0);

	int scroll = vdp.getHorizontalScrollHigh();
	auto namePtr = getNamePtr(line, scroll);
	Pixel* __restrict pixelPtr = buf.data();
	repeat(32, [&] {
		auto charCode = namePtr[scroll & 0x1F];
		auto pattern = patternArea[l + charCode * 8];
		auto color = colorArea[charCode / 8];
		Pixel fg = palFg[color >> 4];
		Pixel bg = palFg[color & 0x0F];
		draw8(pixelPtr, fg, bg, pattern);
		if (!(++scroll & 0x1F)) namePtr = getNamePtr(line, scroll);
	});
}

void CharacterConverter::renderGraphic2(std::span<Pixel, 256> buf, int line) const
{
	int quarter8 = (((line / 8) * 32) & ~0xFF) * 8;
	int line7 = line & 7;
	int scroll = vdp.getHorizontalScrollHigh();
	auto namePtr = getNamePtr(line, scroll);

	Pixel* __restrict pixelPtr = buf.data();
	if (vram.colorTable  .isContinuous((8 * 256) - 1) &&
	    vram.patternTable.isContinuous((8 * 256) - 1) &&
	    ((scroll & 0x1f) == 0)) {
		// Both color and pattern table can be accessed contiguously
		// (no mirroring) and there's no v9958 horizontal scrolling.
		// This is very common, so make an optimized version for this.
		auto patternArea = vram.patternTable.getReadArea<256 * 8>(quarter8);
		auto colorArea   = vram.colorTable  .getReadArea<256 * 8>(quarter8);
		for (auto n : xrange(32)) {
			auto charCode8 = namePtr[n] * 8;
			auto pattern = patternArea[line7 + charCode8];
			auto color   = colorArea  [line7 + charCode8];
			Pixel fg = palFg[color >> 4];
			Pixel bg = palFg[color & 0x0F];
			draw8(pixelPtr, fg, bg, pattern);
		}
	} else {
		// Slower variant, also works when:
		// - there is mirroring in the color table
		// - there is mirroring in the pattern table (TMS9929)
		// - V9958 horizontal scroll feature is used
		unsigned baseLine = (~0u << 13) | quarter8 | line7;
		repeat(32, [&] {
			unsigned charCode8 = namePtr[scroll & 0x1F] * 8;
			unsigned index = charCode8 | baseLine;
			auto pattern = vram.patternTable.readNP(index);
			auto color   = vram.colorTable  .readNP(index);
			Pixel fg = palFg[color >> 4];
			Pixel bg = palFg[color & 0x0F];
			draw8(pixelPtr, fg, bg, pattern);
			if (!(++scroll & 0x1F)) namePtr = getNamePtr(line, scroll);
		});
	}
}

void CharacterConverter::renderMultiHelper(
	Pixel* __restrict pixelPtr, int line,
	unsigned mask, unsigned patternQuarter) const
{
	unsigned baseLine = mask | ((line / 4) & 7);
	unsigned scroll = vdp.getHorizontalScrollHigh();
	auto namePtr = getNamePtr(line, scroll);
	repeat(32, [&] {
		unsigned patternNr = patternQuarter | namePtr[scroll & 0x1F];
		unsigned color = vram.patternTable.readNP((patternNr * 8) | baseLine);
		Pixel cl = palFg[color >> 4];
		Pixel cr = palFg[color & 0x0F];
		pixelPtr[0] = cl; pixelPtr[1] = cl;
		pixelPtr[2] = cl; pixelPtr[3] = cl;
		pixelPtr[4] = cr; pixelPtr[5] = cr;
		pixelPtr[6] = cr; pixelPtr[7] = cr;
		pixelPtr += 8;
		if (!(++scroll & 0x1F)) namePtr = getNamePtr(line, scroll);
	});
}
void CharacterConverter::renderMulti(std::span<Pixel, 256> buf, int line) const
{
	unsigned mask = (~0u << 11);
	renderMultiHelper(buf.data(), line, mask, 0);
}

void CharacterConverter::renderMultiQ(
	std::span<Pixel, 256> buf, int line) const
{
	unsigned mask = (~0u << 13);
	unsigned patternQuarter = (line * 4) & ~0xFF;  // (line / 8) * 32
	renderMultiHelper(buf.data(), line, mask, patternQuarter);
}

void CharacterConverter::renderBogus(std::span<Pixel, 256> buf) const
{
	Pixel* __restrict pixelPtr = buf.data();
	Pixel fg = palFg[vdp.getForegroundColor()];
	Pixel bg = palFg[vdp.getBackgroundColor()];
	auto draw = [&](int n, Pixel col) {
		pixelPtr = std::fill_n(pixelPtr, n, col);
	};
	draw(8, bg);
	repeat(40, [&] {
		draw(4, fg);
		draw(2, bg);
	});
	draw(8, bg);
}

void CharacterConverter::renderBlank(std::span<Pixel, 256> buf) const
{
	// when this is in effect, the VRAM is not refreshed anymore, but that
	// is not emulated
	ranges::fill(buf, palFg[15]);
}

} // namespace openmsx
