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
#include "xrange.hh"
#include "build-info.hh"
#include "components.hh"
#include <cstdint>

#ifdef __SSE2__
#include "emmintrin.h" // SSE2
#endif

namespace openmsx {

template<typename Pixel>
CharacterConverter<Pixel>::CharacterConverter(
	VDP& vdp_, const Pixel* palFg_, const Pixel* palBg_)
	: vdp(vdp_), vram(vdp.getVRAM()), palFg(palFg_), palBg(palBg_)
{
	modeBase = 0; // not strictly needed, but avoids Coverity warning
}

template<typename Pixel>
void CharacterConverter<Pixel>::setDisplayMode(DisplayMode mode)
{
	modeBase = mode.getBase();
	assert(modeBase < 0x0C);
}

template<typename Pixel>
void CharacterConverter<Pixel>::convertLine(Pixel* linePtr, int line)
{
	// TODO: Support YJK on modes other than Graphic 6/7.
	switch (modeBase) {
	case DisplayMode::GRAPHIC1:   // screen 1
		renderGraphic1(linePtr, line);
		break;
	case DisplayMode::TEXT1:      // screen 0, width 40
		renderText1(linePtr, line);
		break;
	case DisplayMode::MULTICOLOR: // screen 3
		renderMulti(linePtr, line);
		break;
	case DisplayMode::GRAPHIC2:   // screen 2
		renderGraphic2(linePtr, line);
		break;
	case DisplayMode::GRAPHIC3:   // screen 4
		renderGraphic2(linePtr, line); // graphic3, actually
		break;
	case  DisplayMode::TEXT2:     // screen 0, width 80
		renderText2(linePtr, line);
		break;
	case DisplayMode::TEXT1Q:     // TMSxxxx only
		if (vdp.isMSX1VDP()) {
			renderText1Q(linePtr, line);
		} else {
			renderBlank (linePtr);
		}
		break;
	case DisplayMode::MULTIQ:     // TMSxxxx only
		if (vdp.isMSX1VDP()) {
			renderMultiQ(linePtr, line);
		} else {
			renderBlank (linePtr);
		}
		break;
	default: // remaining (non-bitmap) modes
		if (vdp.isMSX1VDP()) {
			renderBogus(linePtr);
		} else {
			renderBlank(linePtr);
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

template<typename Pixel> static inline void draw6(
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

template<typename Pixel> static inline void draw8(
	Pixel* __restrict & pixelPtr, Pixel fg, Pixel bg, byte pattern)
{
#ifdef __SSE2__
	// SSE2 version, 32bpp  (16bpp is possible, but not worth it anymore)
	if constexpr (sizeof(Pixel) == 4) {
		const __m128i m74 = _mm_set_epi32(0x10, 0x20, 0x40, 0x80);
		const __m128i m30 = _mm_set_epi32(0x01, 0x02, 0x04, 0x08);
		const __m128i zero = _mm_setzero_si128();

		__m128i fg4 = _mm_set1_epi32(fg);
		__m128i bg4 = _mm_set1_epi32(bg);
		__m128i pat = _mm_set1_epi32(pattern);

		__m128i b74 = _mm_cmpeq_epi32(_mm_and_si128(pat, m74), zero);
		__m128i b30 = _mm_cmpeq_epi32(_mm_and_si128(pat, m30), zero);

		auto* out = reinterpret_cast<__m128i*>(pixelPtr);
		_mm_storeu_si128(out + 0, select(fg4, bg4, b74));
		_mm_storeu_si128(out + 1, select(fg4, bg4, b30));
		pixelPtr += 8;
		return;
	}
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

template<typename Pixel>
void CharacterConverter<Pixel>::renderText1(
	Pixel* __restrict pixelPtr, int line)
{
	Pixel fg = palFg[vdp.getForegroundColor()];
	Pixel bg = palFg[vdp.getBackgroundColor()];

	// 8 * 256 is small enough to always be contiguous
	const byte* patternArea = vram.patternTable.getReadArea(0, 256 * 8);
	patternArea += (line + vdp.getVerticalScroll()) & 7;

	// Note: Because line width is not a power of two, reading an entire line
	//       from a VRAM pointer returned by readArea will not wrap the index
	//       correctly. Therefore we read one character at a time.
	unsigned nameStart = (line / 8) * 40;
	unsigned nameEnd = nameStart + 40;
	for (auto name : xrange(nameStart, nameEnd)) {
		unsigned charcode = vram.nameTable.readNP((name + 0xC00) | (~0u << 12));
		unsigned pattern = patternArea[charcode * 8];
		draw6(pixelPtr, fg, bg, pattern);
	}
}

template<typename Pixel>
void CharacterConverter<Pixel>::renderText1Q(
	Pixel* __restrict pixelPtr, int line)
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
	for (auto name : xrange(nameStart, nameEnd)) {
		unsigned charcode = vram.nameTable.readNP((name + 0xC00) | (~0u << 12));
		unsigned patternNr = patternQuarter | charcode;
		unsigned pattern = vram.patternTable.readNP(
			patternBaseLine | (patternNr * 8));
		draw6(pixelPtr, fg, bg, pattern);
	}
}

template<typename Pixel>
void CharacterConverter<Pixel>::renderText2(
	Pixel* __restrict pixelPtr, int line)
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
	const byte* patternArea = vram.patternTable.getReadArea(0, 256 * 8);
	patternArea += (line + vdp.getVerticalScroll()) & 7;

	unsigned colorStart = (line / 8) * (80 / 8);
	unsigned nameStart  = (line / 8) * 80;
	for (auto i : xrange(80 / 8)) {
		unsigned colorPattern = vram.colorTable.readNP(
			(colorStart + i) | (~0u << 9));
		const byte* nameArea = vram.nameTable.getReadArea(
			(nameStart + 8 * i) | (~0u << 12), 8);
		draw6(pixelPtr,
		      (colorPattern & 0x80) ? blinkFg : plainFg,
		      (colorPattern & 0x80) ? blinkBg : plainBg,
		      patternArea[nameArea[0] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x40) ? blinkFg : plainFg,
		      (colorPattern & 0x40) ? blinkBg : plainBg,
		      patternArea[nameArea[1] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x20) ? blinkFg : plainFg,
		      (colorPattern & 0x20) ? blinkBg : plainBg,
		      patternArea[nameArea[2] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x10) ? blinkFg : plainFg,
		      (colorPattern & 0x10) ? blinkBg : plainBg,
		      patternArea[nameArea[3] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x08) ? blinkFg : plainFg,
		      (colorPattern & 0x08) ? blinkBg : plainBg,
		      patternArea[nameArea[4] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x04) ? blinkFg : plainFg,
		      (colorPattern & 0x04) ? blinkBg : plainBg,
		      patternArea[nameArea[5] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x02) ? blinkFg : plainFg,
		      (colorPattern & 0x02) ? blinkBg : plainBg,
		      patternArea[nameArea[6] * 8]);
		draw6(pixelPtr,
		      (colorPattern & 0x01) ? blinkFg : plainFg,
		      (colorPattern & 0x01) ? blinkBg : plainBg,
		      patternArea[nameArea[7] * 8]);
	}
}

template<typename Pixel>
const byte* CharacterConverter<Pixel>::getNamePtr(int line, int scroll)
{
	// no need to test whether multi-page scrolling is enabled,
	// indexMask in the nameTable already takes care of it
	return vram.nameTable.getReadArea(
		((line / 8) * 32) | ((scroll & 0x20) ? 0x8000 : 0), 32);
}
template<typename Pixel>
void CharacterConverter<Pixel>::renderGraphic1(
	Pixel* __restrict pixelPtr, int line)
{
	const byte* patternArea = vram.patternTable.getReadArea(0, 256 * 8);
	patternArea += line & 7;
	const byte* colorArea = vram.colorTable.getReadArea(0, 256 / 8);

	int scroll = vdp.getHorizontalScrollHigh();
	const byte* namePtr = getNamePtr(line, scroll);
	repeat(32, [&] {
		unsigned charcode = namePtr[scroll & 0x1F];
		unsigned pattern = patternArea[charcode * 8];
		unsigned color = colorArea[charcode / 8];
		Pixel fg = palFg[color >> 4];
		Pixel bg = palFg[color & 0x0F];
		draw8(pixelPtr, fg, bg, pattern);
		if (!(++scroll & 0x1F)) namePtr = getNamePtr(line, scroll);
	});
}

template<typename Pixel>
void CharacterConverter<Pixel>::renderGraphic2(
	Pixel* __restrict pixelPtr, int line)
{
	int quarter8 = (((line / 8) * 32) & ~0xFF) * 8;
	int line7 = line & 7;
	int scroll = vdp.getHorizontalScrollHigh();
	const byte* namePtr = getNamePtr(line, scroll);

	if (vram.colorTable  .isContinuous((8 * 256) - 1) &&
	    vram.patternTable.isContinuous((8 * 256) - 1) &&
	    ((scroll & 0x1f) == 0)) {
		// Both color and pattern table can be accessed contiguously
		// (no mirroring) and there's no v9958 horizontal scrolling.
		// This is very common, so make an optimized version for this.
		const byte* patternArea = vram.patternTable.getReadArea(quarter8, 8 * 256) + line7;
		const byte* colorArea   = vram.colorTable  .getReadArea(quarter8, 8 * 256) + line7;
		for (auto n : xrange(32)) {
			unsigned charCode8 = namePtr[n] * 8;
			unsigned pattern = patternArea[charCode8];
			unsigned color   = colorArea  [charCode8];
			Pixel fg = palFg[color >> 4];
			Pixel bg = palFg[color & 0x0F];
			draw8(pixelPtr, fg, bg, pattern);
		}
	} else {
		// Slower variant, also works when:
		// - there is mirroring in the color table
		// - there is mirroring in the pattern table (TMS9929)
		// - V9958 horizontal scroll feature is used
		int baseLine = (~0u << 13) | quarter8 | line7;
		repeat(32, [&] {
			unsigned charCode8 = namePtr[scroll & 0x1F] * 8;
			unsigned index = charCode8 | baseLine;
			unsigned pattern = vram.patternTable.readNP(index);
			unsigned color   = vram.colorTable  .readNP(index);
			Pixel fg = palFg[color >> 4];
			Pixel bg = palFg[color & 0x0F];
			draw8(pixelPtr, fg, bg, pattern);
			if (!(++scroll & 0x1F)) namePtr = getNamePtr(line, scroll);
		});
	}
}

template<typename Pixel>
void CharacterConverter<Pixel>::renderMultiHelper(
	Pixel* __restrict pixelPtr, int line,
	int mask, int patternQuarter)
{
	unsigned baseLine = mask | ((line / 4) & 7);
	unsigned scroll = vdp.getHorizontalScrollHigh();
	const byte* namePtr = getNamePtr(line, scroll);
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
template<typename Pixel>
void CharacterConverter<Pixel>::renderMulti(
	Pixel* __restrict pixelPtr, int line)
{
	int mask = (~0u << 11);
	renderMultiHelper(pixelPtr, line, mask, 0);
}

template<typename Pixel>
void CharacterConverter<Pixel>::renderMultiQ(
	Pixel* __restrict pixelPtr, int line)
{
	int mask = (~0u << 13);
	int patternQuarter = (line * 4) & ~0xFF;  // (line / 8) * 32
	renderMultiHelper(pixelPtr, line, mask, patternQuarter);
}

template<typename Pixel>
void CharacterConverter<Pixel>::renderBogus(
	Pixel* __restrict pixelPtr)
{
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

template<typename Pixel>
void CharacterConverter<Pixel>::renderBlank(
	Pixel* __restrict pixelPtr)
{
	// when this is in effect, the VRAM is not refreshed anymore, but that
	// is not emulated
	std::fill_n(pixelPtr, 256, palFg[15]);
}

// Force template instantiation.
#if HAVE_16BPP
template class CharacterConverter<uint16_t>;
#endif
#if HAVE_32BPP || COMPONENT_GL
template class CharacterConverter<uint32_t>;
#endif

} // namespace openmsx
