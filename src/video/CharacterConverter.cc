// $Id$

/*
TODO:
- Clean up renderGraphics2, it is currently very hard to understand
  with all the masks and quarters etc.
- Try using a generic inlined pattern-to-pixels converter.
- Correctly implement vertical scroll in text modes.
  Can be implemented by reordering blitting, but uses a smaller
  wrap than GFX modes: 8 lines instead of 256 lines.
*/

#include "CharacterConverter.hh"
#include "GLUtil.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"

namespace openmsx {

template <class Pixel>
typename CharacterConverter<Pixel>::RenderMethod
	CharacterConverter<Pixel>::modeToRenderMethod[] = {
		// M5 M4 = 0 0  (MSX1 modes)
		&CharacterConverter::renderGraphic1,
		&CharacterConverter::renderText1,
		&CharacterConverter::renderMulti,
		&CharacterConverter::renderBogus,
		&CharacterConverter::renderGraphic2,
		&CharacterConverter::renderText1Q,
		&CharacterConverter::renderMultiQ,
		&CharacterConverter::renderBogus,
		// M5 M4 = 0 1
		&CharacterConverter::renderGraphic2, // graphic 3, actually
		&CharacterConverter::renderText2,
		&CharacterConverter::renderBogus,
		&CharacterConverter::renderBogus
	};

template <class Pixel>
CharacterConverter<Pixel>::CharacterConverter(
	VDP& vdp_, const Pixel* palFg_, const Pixel* palBg_)
	: vdp(vdp_), vram(vdp.getVRAM()), palFg(palFg_), palBg(palBg_)
{
}

template <class Pixel>
void CharacterConverter<Pixel>::setDisplayMode(DisplayMode mode)
{
	assert(mode.getBase() < 0x0C);
	renderMethod = modeToRenderMethod[mode.getBase()];
}

template <class Pixel>
void CharacterConverter<Pixel>::renderText1(
	Pixel* pixelPtr, int line)
{
	Pixel fg = palFg[vdp.getForegroundColour()];
	Pixel bg = palBg[vdp.getBackgroundColour()];

	// 8 * 256 is small enough to always be contiguous
	const byte* patternArea = vram.patternTable.getReadArea(0, 256 * 8);
	patternArea += (line + vdp.getVerticalScroll()) & 7;

	// Note: Because line width is not a power of two, reading an entire line
	//       from a VRAM pointer returned by readArea will not wrap the index
	//       correctly. Therefore we read one character at a time.
	unsigned nameStart = (line / 8) * 40;
	unsigned nameEnd = nameStart + 40;
	for (unsigned name = nameStart; name < nameEnd; ++name) {
		unsigned charcode = vram.nameTable.readNP((name + 0xC00) | (-1 << 12));
		unsigned pattern = patternArea[charcode * 8];
		pixelPtr[0] = (pattern & 0x80) ? fg : bg;
		pixelPtr[1] = (pattern & 0x40) ? fg : bg;
		pixelPtr[2] = (pattern & 0x20) ? fg : bg;
		pixelPtr[3] = (pattern & 0x10) ? fg : bg;
		pixelPtr[4] = (pattern & 0x08) ? fg : bg;
		pixelPtr[5] = (pattern & 0x04) ? fg : bg;
		pixelPtr += 6;
	}
}

template <class Pixel>
void CharacterConverter<Pixel>::renderText1Q(
	Pixel* pixelPtr, int line)
{
	Pixel fg = palFg[vdp.getForegroundColour()];
	Pixel bg = palBg[vdp.getBackgroundColour()];

	unsigned patternBaseLine = (-1 << 13) | ((line + vdp.getVerticalScroll()) & 7);

	// Note: Because line width is not a power of two, reading an entire line
	//       from a VRAM pointer returned by readArea will not wrap the index
	//       correctly. Therefore we read one character at a time.
	unsigned nameStart = (line / 8) * 40;
	unsigned nameEnd = nameStart + 40;
	unsigned patternQuarter = (line & 0xC0) << 2;
	for (unsigned name = nameStart; name < nameEnd; ++name) {
		unsigned charcode = vram.nameTable.readNP((name + 0xC00) | (-1 << 12));
		unsigned patternNr = patternQuarter | charcode;
		unsigned pattern = vram.patternTable.readNP(
			patternBaseLine | (patternNr * 8));
		pixelPtr[0] = (pattern & 0x80) ? fg : bg;
		pixelPtr[1] = (pattern & 0x40) ? fg : bg;
		pixelPtr[2] = (pattern & 0x20) ? fg : bg;
		pixelPtr[3] = (pattern & 0x10) ? fg : bg;
		pixelPtr[4] = (pattern & 0x08) ? fg : bg;
		pixelPtr[5] = (pattern & 0x04) ? fg : bg;
		pixelPtr += 6;
	}
}

template <class Pixel>
void CharacterConverter<Pixel>::renderText2(
	Pixel* pixelPtr, int line)
{
	Pixel plainFg = palFg[vdp.getForegroundColour()];
	Pixel plainBg = palBg[vdp.getBackgroundColour()];
	Pixel blinkFg, blinkBg;
 	if (vdp.getBlinkState()) {
		int fg = vdp.getBlinkForegroundColour();
		blinkFg = palBg[fg ? fg : vdp.getBlinkBackgroundColour()];
		blinkBg = palBg[vdp.getBlinkBackgroundColour()];
	} else {
		blinkFg = plainFg;
		blinkBg = plainBg;
	}

	// 8 * 256 is small enough to always be contiguous
	const byte* patternArea = vram.patternTable.getReadArea(0, 256 * 8);
	patternArea += (line + vdp.getVerticalScroll()) & 7;

	unsigned nameStart = (line / 8) * 80;
	unsigned nameEnd = nameStart + 80;
	unsigned colourPattern = 0; // avoid warning
	for (unsigned name = nameStart; name < nameEnd; ++name) {
		// Colour table contains one bit per character.
		if ((name & 7) == 0) {
			// can be optimized by unrolling 8 times, worth it?
			colourPattern = vram.colourTable.readNP((name >> 3) | (-1 << 9));
		} else {
			colourPattern <<= 1;
		}
		unsigned charcode = vram.nameTable.readNP(name | (-1 << 12));
		Pixel fg = (colourPattern & 0x80) ? blinkFg : plainFg;
		Pixel bg = (colourPattern & 0x80) ? blinkBg : plainBg;
		unsigned pattern = patternArea[charcode * 8];
		pixelPtr[0] = (pattern & 0x80) ? fg : bg;
		pixelPtr[1] = (pattern & 0x40) ? fg : bg;
		pixelPtr[2] = (pattern & 0x20) ? fg : bg;
		pixelPtr[3] = (pattern & 0x10) ? fg : bg;
		pixelPtr[4] = (pattern & 0x08) ? fg : bg;
		pixelPtr[5] = (pattern & 0x04) ? fg : bg;
		pixelPtr += 6;
	}
}

template <class Pixel>
const byte* CharacterConverter<Pixel>::getNamePtr(int line, int scroll)
{
	// no need to test whether multi-page scrolling is enabled,
	// indexMask in the nameTable already takes care of it
	return vram.nameTable.readAreaIndex(
		((line / 8) * 32) | ((scroll & 0x20) ? 0x8000 : 0));
}
template <class Pixel>
void CharacterConverter<Pixel>::renderGraphic1(
	Pixel* pixelPtr, int line)
{
	const byte* patternArea = vram.patternTable.getReadArea(0, 256 * 8);
	patternArea += line & 7;
	const byte* colorArea = vram.colourTable.getReadArea(0, 256 / 8);

	int scroll = vdp.getHorizontalScrollHigh();
	const byte* namePtr = getNamePtr(line, scroll);
	for (unsigned n = 0; n < 32; ++n) {
		unsigned charcode = namePtr[scroll & 0x1F];
		unsigned color = colorArea[charcode / 8];
		Pixel fg = palFg[color >> 4];
		Pixel bg = palFg[color & 0x0F];

		unsigned pattern = patternArea[charcode * 8];
		pixelPtr[0] = (pattern & 0x80) ? fg : bg;
		pixelPtr[1] = (pattern & 0x40) ? fg : bg;
		pixelPtr[2] = (pattern & 0x20) ? fg : bg;
		pixelPtr[3] = (pattern & 0x10) ? fg : bg;
		pixelPtr[4] = (pattern & 0x08) ? fg : bg;
		pixelPtr[5] = (pattern & 0x04) ? fg : bg;
		pixelPtr[6] = (pattern & 0x02) ? fg : bg;
		pixelPtr[7] = (pattern & 0x01) ? fg : bg;
		pixelPtr += 8;
		if (!(++scroll & 0x1F)) namePtr = getNamePtr(line, scroll);
	}
}

template <class Pixel>
void CharacterConverter<Pixel>::renderGraphic2(
	Pixel* pixelPtr, int line)
{
	int quarter = ((line / 8) * 32) & ~0xFF;
	int baseLine = (-1 << 13) | (quarter * 8) | (line & 7);

	// pattern area is contiguous, color area not
	const byte* patternArea = vram.patternTable.getReadArea(quarter * 8, 8 * 256);
	patternArea += line & 7;

	int scroll = vdp.getHorizontalScrollHigh();
	const byte* namePtr = getNamePtr(line, scroll);

	for (unsigned n = 0; n < 32; ++n) {
		unsigned charCode8 = namePtr[scroll & 0x1F] * 8;
		unsigned pattern = patternArea[charCode8];
		unsigned index = charCode8 | baseLine;
		unsigned color = vram.colourTable.readNP(index);
		Pixel fg = palFg[color >> 4];
		Pixel bg = palFg[color & 0x0F];
		pixelPtr[0] = (pattern & 0x80) ? fg : bg;
		pixelPtr[1] = (pattern & 0x40) ? fg : bg;
		pixelPtr[2] = (pattern & 0x20) ? fg : bg;
		pixelPtr[3] = (pattern & 0x10) ? fg : bg;
		pixelPtr[4] = (pattern & 0x08) ? fg : bg;
		pixelPtr[5] = (pattern & 0x04) ? fg : bg;
		pixelPtr[6] = (pattern & 0x02) ? fg : bg;
		pixelPtr[7] = (pattern & 0x01) ? fg : bg;
		pixelPtr += 8;
		if (!(++scroll & 0x1F)) namePtr = getNamePtr(line, scroll);
	}
}

template <class Pixel>
void CharacterConverter<Pixel>::renderMultiHelper(
	Pixel* pixelPtr, int line, int mask, int patternQuarter)
{
	unsigned baseLine = mask | ((line / 4) & 7);
	unsigned scroll = vdp.getHorizontalScrollHigh();
	const byte* namePtr = getNamePtr(line, scroll);
	for (unsigned n = 0; n < 32; ++n) {
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
	}
}
template <class Pixel>
void CharacterConverter<Pixel>::renderMulti(
	Pixel* pixelPtr, int line)
{
	int mask = (-1 << 11);
	renderMultiHelper(pixelPtr, line, mask, 0);
}

template <class Pixel>
void CharacterConverter<Pixel>::renderMultiQ(
	Pixel* pixelPtr, int line)
{
	int mask = (-1 << 13);
	int patternQuarter = (line * 4) & ~0xFF;  // (line / 8) * 32
	renderMultiHelper(pixelPtr, line, mask, patternQuarter);
}

template <class Pixel>
void CharacterConverter<Pixel>::renderBogus(
	Pixel* pixelPtr, int /*line*/)
{
	Pixel fg = palFg[vdp.getForegroundColour()];
	Pixel bg = palBg[vdp.getBackgroundColour()];
	for (int n = 8; n--; ) *pixelPtr++ = bg;
	for (int c = 20; c--; ) {
		for (int n = 4; n--; ) *pixelPtr++ = fg;
		for (int n = 2; n--; ) *pixelPtr++ = bg;
	}
	for (int n = 8; n--; ) *pixelPtr++ = bg;
}

// Force template instantiation.
template class CharacterConverter<word>;
template class CharacterConverter<unsigned>;
#ifdef COMPONENT_GL
template<> class CharacterConverter<GLUtil::NoExpansion> {};
template class CharacterConverter<GLUtil::ExpandGL>;
#endif // COMPONENT_GL

} // namespace openmsx

