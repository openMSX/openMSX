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

	unsigned colorStart = (line / 8) * (80 / 8);
	unsigned nameStart  = (line / 8) * 80;
	for (unsigned i = 0; i < (80 / 8); ++i) {
		unsigned colorPattern = vram.colourTable.readNP(
			(colorStart + i) | (-1 << 9));
		const byte* nameArea = vram.nameTable.getReadArea(
			(nameStart + 8 * i) | (-1 << 12), 8);

		Pixel fg0 = (colorPattern & 0x80) ? blinkFg : plainFg;
		Pixel bg0 = (colorPattern & 0x80) ? blinkBg : plainBg;
		unsigned pattern0 = patternArea[nameArea[0] * 8];
		pixelPtr[ 0] = (pattern0 & 0x80) ? fg0 : bg0;
		pixelPtr[ 1] = (pattern0 & 0x40) ? fg0 : bg0;
		pixelPtr[ 2] = (pattern0 & 0x20) ? fg0 : bg0;
		pixelPtr[ 3] = (pattern0 & 0x10) ? fg0 : bg0;
		pixelPtr[ 4] = (pattern0 & 0x08) ? fg0 : bg0;
		pixelPtr[ 5] = (pattern0 & 0x04) ? fg0 : bg0;

		Pixel fg1 = (colorPattern & 0x40) ? blinkFg : plainFg;
		Pixel bg1 = (colorPattern & 0x40) ? blinkBg : plainBg;
		unsigned pattern1 = patternArea[nameArea[1] * 8];
		pixelPtr[ 6] = (pattern1 & 0x80) ? fg1 : bg1;
		pixelPtr[ 7] = (pattern1 & 0x40) ? fg1 : bg1;
		pixelPtr[ 8] = (pattern1 & 0x20) ? fg1 : bg1;
		pixelPtr[ 9] = (pattern1 & 0x10) ? fg1 : bg1;
		pixelPtr[10] = (pattern1 & 0x08) ? fg1 : bg1;
		pixelPtr[11] = (pattern1 & 0x04) ? fg1 : bg1;

		Pixel fg2 = (colorPattern & 0x20) ? blinkFg : plainFg;
		Pixel bg2 = (colorPattern & 0x20) ? blinkBg : plainBg;
		unsigned pattern2 = patternArea[nameArea[2] * 8];
		pixelPtr[12] = (pattern2 & 0x80) ? fg2 : bg2;
		pixelPtr[13] = (pattern2 & 0x40) ? fg2 : bg2;
		pixelPtr[14] = (pattern2 & 0x20) ? fg2 : bg2;
		pixelPtr[15] = (pattern2 & 0x10) ? fg2 : bg2;
		pixelPtr[16] = (pattern2 & 0x08) ? fg2 : bg2;
		pixelPtr[17] = (pattern2 & 0x04) ? fg2 : bg2;

		Pixel fg3 = (colorPattern & 0x10) ? blinkFg : plainFg;
		Pixel bg3 = (colorPattern & 0x10) ? blinkBg : plainBg;
		unsigned pattern3 = patternArea[nameArea[3] * 8];
		pixelPtr[18] = (pattern3 & 0x80) ? fg3 : bg3;
		pixelPtr[19] = (pattern3 & 0x40) ? fg3 : bg3;
		pixelPtr[20] = (pattern3 & 0x20) ? fg3 : bg3;
		pixelPtr[21] = (pattern3 & 0x10) ? fg3 : bg3;
		pixelPtr[22] = (pattern3 & 0x08) ? fg3 : bg3;
		pixelPtr[23] = (pattern3 & 0x04) ? fg3 : bg3;

		Pixel fg4 = (colorPattern & 0x08) ? blinkFg : plainFg;
		Pixel bg4 = (colorPattern & 0x08) ? blinkBg : plainBg;
		unsigned pattern4 = patternArea[nameArea[4] * 8];
		pixelPtr[24] = (pattern4 & 0x80) ? fg4 : bg4;
		pixelPtr[25] = (pattern4 & 0x40) ? fg4 : bg4;
		pixelPtr[26] = (pattern4 & 0x20) ? fg4 : bg4;
		pixelPtr[27] = (pattern4 & 0x10) ? fg4 : bg4;
		pixelPtr[28] = (pattern4 & 0x08) ? fg4 : bg4;
		pixelPtr[29] = (pattern4 & 0x04) ? fg4 : bg4;

		Pixel fg5 = (colorPattern & 0x04) ? blinkFg : plainFg;
		Pixel bg5 = (colorPattern & 0x04) ? blinkBg : plainBg;
		unsigned pattern5 = patternArea[nameArea[5] * 8];
		pixelPtr[30] = (pattern5 & 0x80) ? fg5 : bg5;
		pixelPtr[31] = (pattern5 & 0x40) ? fg5 : bg5;
		pixelPtr[32] = (pattern5 & 0x20) ? fg5 : bg5;
		pixelPtr[33] = (pattern5 & 0x10) ? fg5 : bg5;
		pixelPtr[34] = (pattern5 & 0x08) ? fg5 : bg5;
		pixelPtr[35] = (pattern5 & 0x04) ? fg5 : bg5;

		Pixel fg6 = (colorPattern & 0x02) ? blinkFg : plainFg;
		Pixel bg6 = (colorPattern & 0x02) ? blinkBg : plainBg;
		unsigned pattern6 = patternArea[nameArea[6] * 8];
		pixelPtr[36] = (pattern6 & 0x80) ? fg6 : bg6;
		pixelPtr[37] = (pattern6 & 0x40) ? fg6 : bg6;
		pixelPtr[38] = (pattern6 & 0x20) ? fg6 : bg6;
		pixelPtr[39] = (pattern6 & 0x10) ? fg6 : bg6;
		pixelPtr[40] = (pattern6 & 0x08) ? fg6 : bg6;
		pixelPtr[41] = (pattern6 & 0x04) ? fg6 : bg6;

		Pixel fg7 = (colorPattern & 0x01) ? blinkFg : plainFg;
		Pixel bg7 = (colorPattern & 0x01) ? blinkBg : plainBg;
		unsigned pattern7 = patternArea[nameArea[7] * 8];
		pixelPtr[42] = (pattern7 & 0x80) ? fg7 : bg7;
		pixelPtr[43] = (pattern7 & 0x40) ? fg7 : bg7;
		pixelPtr[44] = (pattern7 & 0x20) ? fg7 : bg7;
		pixelPtr[45] = (pattern7 & 0x10) ? fg7 : bg7;
		pixelPtr[46] = (pattern7 & 0x08) ? fg7 : bg7;
		pixelPtr[47] = (pattern7 & 0x04) ? fg7 : bg7;

		pixelPtr += 48;
	}
}

template <class Pixel>
const byte* CharacterConverter<Pixel>::getNamePtr(int line, int scroll)
{
	// no need to test whether multi-page scrolling is enabled,
	// indexMask in the nameTable already takes care of it
	return vram.nameTable.getReadArea(
		((line / 8) * 32) | ((scroll & 0x20) ? 0x8000 : 0), 32);
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

