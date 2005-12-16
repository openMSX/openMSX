// $Id$

/*
TODO:
- Dirty checking is currently broken in some places.
  Clean this up after new VRAM Window implemenation is in place.
- Is it possible to combine dirtyPattern and dirtyColour into a single
  dirty array?
  Pitfalls:
  * in SCREEN1, a colour change invalidates 8 consequetive characters
  * A12 and A11 of patternMask and colourMask may be different
    also, colourMask has A10..A6 as well
    in most realistic cases however the two will be of equal size
- Clean up renderGraphics2, it is currently very hard to understand
  with all the masks and quarters etc.
- Try using a generic inlined pattern-to-pixels converter.
- Fix character mode dirty checking to work with incremental rendering.
  Approach 1:
  * use two dirty arrays, one for this frame, one for next frame
  * on every change, mark dirty in both arrays
    (checking line is useless because of vertical scroll on screen splits)
  * in frameStart, swap arrays
  Approach 2:
  * cache characters as 16x16 blocks and blit them to the screen
  * on a name change, do nothing
  * on a pattern or colour change, mark the block as dirty
  * if a to-be-blitted block is dirty, recalculate it
  I'll implement approach 1 on account of being very similar to the
  existing code. Some time I'll implement approach 2 as well and see
  if it is an improvement (in clarity and performance).
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
	anyDirtyColour = anyDirtyPattern = anyDirtyName = true;
	dirtyColour.set();
	dirtyPattern.set();
	dirtyName.set();
	dirtyForeground = dirtyBackground = true;
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
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel fg = palFg[vdp.getForegroundColour()];
	Pixel bg = palBg[vdp.getBackgroundColour()];

	int patternBaseLine = (-1 << 11) | ((line + vdp.getVerticalScroll()) & 7);

	// Actual display.
	// Note: Because line width is not a power of two, reading an entire line
	//       from a VRAM pointer returned by readArea will not wrap the index
	//       correctly. Therefore we read one character at a time.
	int nameStart = (line / 8) * 40;
	int nameEnd = nameStart + 40;
	for (int name = nameStart; name < nameEnd; name++) {
		int charcode = vram.nameTable.readNP((name + 0xC00) | (-1 << 12));
		if (dirtyColours || dirtyName[name] || dirtyPattern[charcode]) {
			int pattern = vram.patternTable.readNP(
				patternBaseLine | (charcode * 8));
			pixelPtr[0] = (pattern & 0x80) ? fg : bg;
			pixelPtr[1] = (pattern & 0x40) ? fg : bg;
			pixelPtr[2] = (pattern & 0x20) ? fg : bg;
			pixelPtr[3] = (pattern & 0x10) ? fg : bg;
			pixelPtr[4] = (pattern & 0x08) ? fg : bg;
			pixelPtr[5] = (pattern & 0x04) ? fg : bg;
		}
		pixelPtr += 6;
	}
}

template <class Pixel>
void CharacterConverter<Pixel>::renderText1Q(
	Pixel* pixelPtr, int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel fg = palFg[vdp.getForegroundColour()];
	Pixel bg = palBg[vdp.getBackgroundColour()];

	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	int patternQuarter = nameStart & ~0xFF;
	int patternBaseLine = (-1 << 13) | ((line + vdp.getVerticalScroll()) & 7);	// TODO check vertical scroll
	const byte* namePtr = vram.nameTable.readArea(nameStart | (-1 << 10));

	// Actual display.
	for (int name = nameStart; name < nameEnd; name++) {
		int patternNr = patternQuarter | *namePtr++;
		if (dirtyColours || dirtyName[name] || dirtyPattern[patternNr]) {
			int pattern = vram.patternTable.readNP(
				patternBaseLine | (patternNr * 8) );
			pixelPtr[0] = (pattern & 0x80) ? fg : bg;
			pixelPtr[1] = (pattern & 0x40) ? fg : bg;
			pixelPtr[2] = (pattern & 0x20) ? fg : bg;
			pixelPtr[3] = (pattern & 0x10) ? fg : bg;
			pixelPtr[4] = (pattern & 0x08) ? fg : bg;
			pixelPtr[5] = (pattern & 0x04) ? fg : bg;
		}
		pixelPtr += 6;
	}
}

template <class Pixel>
void CharacterConverter<Pixel>::renderText2(
	Pixel* pixelPtr, int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

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

	int patternBaseLine = (-1 << 11) | ((line + vdp.getVerticalScroll()) & 7);

	// Actual display.
	// TODO: Implement blinking.
	int nameStart = (line / 8) * 80;
	int nameEnd = nameStart + 80;
	int colourPattern = 0; // avoid warning
	for (int name = nameStart; name < nameEnd; name++) {
		// Colour table contains one bit per character.
		if ((name & 7) == 0) {
			colourPattern = vram.colourTable.readNP((name >> 3) | (-1 << 9));
		} else {
			colourPattern <<= 1;
		}
		int charcode = vram.nameTable.readNP(name | (-1 << 12));
		if (dirtyColours || dirtyName[name] || dirtyPattern[charcode]) {
			Pixel fg, bg;
			if (colourPattern & 0x80) {
				fg = blinkFg;
				bg = blinkBg;
			} else {
				fg = plainFg;
				bg = plainBg;
			}
			int pattern = vram.patternTable.readNP(
				patternBaseLine | (charcode * 8) );
			pixelPtr[0] = (pattern & 0x80) ? fg : bg;
			pixelPtr[1] = (pattern & 0x40) ? fg : bg;
			pixelPtr[2] = (pattern & 0x20) ? fg : bg;
			pixelPtr[3] = (pattern & 0x10) ? fg : bg;
			pixelPtr[4] = (pattern & 0x08) ? fg : bg;
			pixelPtr[5] = (pattern & 0x04) ? fg : bg;
		}
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
	if (!(anyDirtyName || anyDirtyPattern || anyDirtyColour)) return;

	int patternBaseLine = (-1 << 11) | (line & 7);
	int scroll = vdp.getHorizontalScrollHigh();
	const byte* namePtr = getNamePtr(line, scroll);
	for (int n = 0; n < 32; ++n) {
		int charcode = namePtr[scroll & 0x1F];
		if (/*dirtyName[name] ||*/ dirtyPattern[charcode]
		|| dirtyColour[charcode / 64]) {
			int colour = vram.colourTable.readNP((charcode / 8) | (-1 << 6));
			Pixel fg = palFg[colour >> 4];
			Pixel bg = palFg[colour & 0x0F];

			int pattern = vram.patternTable.readNP(
				patternBaseLine | (charcode * 8));
			pixelPtr[0] = (pattern & 0x80) ? fg : bg;
			pixelPtr[1] = (pattern & 0x40) ? fg : bg;
			pixelPtr[2] = (pattern & 0x20) ? fg : bg;
			pixelPtr[3] = (pattern & 0x10) ? fg : bg;
			pixelPtr[4] = (pattern & 0x08) ? fg : bg;
			pixelPtr[5] = (pattern & 0x04) ? fg : bg;
			pixelPtr[6] = (pattern & 0x02) ? fg : bg;
			pixelPtr[7] = (pattern & 0x01) ? fg : bg;
		}
		pixelPtr += 8;
		if (!(++scroll & 0x1F)) namePtr = getNamePtr(line, scroll);
	}
}

template <class Pixel>
void CharacterConverter<Pixel>::renderGraphic2(
	Pixel* pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern || anyDirtyColour)) return;

	int quarter = ((line / 8) * 32) & ~0xFF;
	int baseLine = (-1 << 13) | (quarter << 3) | (line & 7);
	int scroll = vdp.getHorizontalScrollHigh();
	const byte* namePtr = getNamePtr(line, scroll);
	for (int n = 0; n < 32; ++n) {
		int charCode = namePtr[scroll & 0x1F];
		if (/*dirtyName[name] ||*/ dirtyPattern[quarter | charCode]
		|| dirtyColour[quarter | charCode]) {
			int index = (charCode * 8) | baseLine;
			int pattern = vram.patternTable.readNP(index);
			int colour = vram.colourTable.readNP(index);
			Pixel fg = palFg[colour >> 4];
			Pixel bg = palFg[colour & 0x0F];
			pixelPtr[0] = (pattern & 0x80) ? fg : bg;
			pixelPtr[1] = (pattern & 0x40) ? fg : bg;
			pixelPtr[2] = (pattern & 0x20) ? fg : bg;
			pixelPtr[3] = (pattern & 0x10) ? fg : bg;
			pixelPtr[4] = (pattern & 0x08) ? fg : bg;
			pixelPtr[5] = (pattern & 0x04) ? fg : bg;
			pixelPtr[6] = (pattern & 0x02) ? fg : bg;
			pixelPtr[7] = (pattern & 0x01) ? fg : bg;
		}
		pixelPtr += 8;
		if (!(++scroll & 0x1F)) namePtr = getNamePtr(line, scroll);
	}
}

template <class Pixel>
void CharacterConverter<Pixel>::renderMultiHelper(
	Pixel* pixelPtr, int line, int mask, int patternQuarter)
{
	if (!(anyDirtyName || anyDirtyPattern)) return;

	int baseLine = mask | ((line / 4) & 7);
	int scroll = vdp.getHorizontalScrollHigh();
	const byte* namePtr = getNamePtr(line, scroll);
	for (int n = 0; n < 32; ++n) {
		int patternNr = patternQuarter | namePtr[scroll & 0x1F];
		if (/*dirtyName[name] ||*/ dirtyPattern[patternNr]) {
			int colour = vram.patternTable.readNP((patternNr * 8) | baseLine);
			Pixel cl = palFg[colour >> 4];
			Pixel cr = palFg[colour & 0x0F];
			pixelPtr[0] = cl; pixelPtr[1] = cl;
			pixelPtr[2] = cl; pixelPtr[3] = cl;
			pixelPtr[4] = cr; pixelPtr[5] = cr;
			pixelPtr[6] = cr; pixelPtr[7] = cr;
		}
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
	if (!(dirtyForeground || dirtyBackground)) return;

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
// The type "GLuint" can be either be equivalent to "word", "unsigned" or still
// some completely other type. In the later case we need to expand the
// CharacterConverter<GLuint> template, in the two former cases it is an error
// to expand it again (it is already instantiated above).
// The following piece of template metaprogramming takes care of this.

class NoExpansion {};
// ExpandFilter<T>::type = (T == unsigned || T == word) ? NoExpansion : T
template<class T> class ExpandFilter  { typedef T           type; };
template<> class ExpandFilter<word>     { typedef NoExpansion type; };
template<> class ExpandFilter<unsigned> { typedef NoExpansion type; };

template<> class CharacterConverter<NoExpansion> {};
template class CharacterConverter<ExpandFilter<GLuint>::type>;

#endif // COMPONENT_GL

} // namespace openmsx

