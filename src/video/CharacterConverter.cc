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

// Force template instantiation for these types.
// Without this, object file contains no method implementations.
template class CharacterConverter<byte, Renderer::ZOOM_256>;
template class CharacterConverter<byte, Renderer::ZOOM_REAL>;
template class CharacterConverter<word, Renderer::ZOOM_256>;
template class CharacterConverter<word, Renderer::ZOOM_REAL>;
template class CharacterConverter<unsigned int, Renderer::ZOOM_256>;
template class CharacterConverter<unsigned int, Renderer::ZOOM_REAL>;

#ifdef COMPONENT_GL
// On some systems, "GLuint" is not equivalent to "unsigned int",
// so CharacterConverter must be instantiated separately for those systems.
// But on systems where it is equivalent, it's an error to expand
// the same template twice.
// The following piece of template metaprogramming expands
// CharacterConverter<GLuint, Renderer::ZOOM_REAL> to an empty class if
// "GLuint" is equivalent to "unsigned int"; otherwise it is expanded to
// the actual BitmapConverter implementation.
class NoExpansion {};
// ExpandFilter::ExpandType = (Type == unsigned int ? NoExpansion : Type)
template <class Type> class ExpandFilter {
	typedef Type ExpandType;
};
template <> class ExpandFilter<unsigned int> {
	typedef NoExpansion ExpandType;
};
template <Renderer::Zoom zoom> class CharacterConverter<NoExpansion, zoom> {};
template class CharacterConverter<
	ExpandFilter<GLuint>::ExpandType, Renderer::ZOOM_REAL >;
#endif // COMPONENT_GL


template <class Pixel, Renderer::Zoom zoom>
typename CharacterConverter<Pixel, zoom>::RenderMethod
	CharacterConverter<Pixel, zoom>::modeToRenderMethod[] = {
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

template <class Pixel, Renderer::Zoom zoom>
CharacterConverter<Pixel, zoom>::CharacterConverter(
	VDP *vdp, const Pixel *palFg, const Pixel *palBg, Blender<Pixel> blender)
	: blender(blender)
{
	this->vdp = vdp;
	this->palFg = palFg;
	this->palBg = palBg;
	vram = vdp->getVRAM();

	anyDirtyColour = anyDirtyPattern = anyDirtyName = true;
	dirtyColour.set();
	dirtyPattern.set();
	dirtyName.set();
	dirtyForeground = dirtyBackground = true;
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::setDisplayMode(DisplayMode mode)
{
	assert(mode.getBase() < 0x0C);
	renderMethod = modeToRenderMethod[mode.getBase()];
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderText1(
	Pixel *pixelPtr, int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];

	int patternBaseLine = (-1 << 11) | ((line + vdp->getVerticalScroll()) & 7);

	// Actual display.
	// Note: Because line width is not a power of two, reading an entire line
	//       from a VRAM pointer returned by readArea will not wrap the index
	//       correctly. Therefore we read one character at a time.
	int nameStart = (line / 8) * 40;
	int nameEnd = nameStart + 40;
	for (int name = nameStart; name < nameEnd; name++) {
		int charcode = vram->nameTable.readNP((name + 0xC00) | (-1 << 12));
		if (dirtyColours || dirtyName[name] || dirtyPattern[charcode]) {
			int pattern = vram->patternTable.readNP(
				patternBaseLine | (charcode * 8) );
			for (int i = 6; i--; ) {
				*pixelPtr++ = (pattern & 0x80) ? fg : bg;
				pattern <<= 1;
			}
		} else {
			pixelPtr += 6;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderText1Q(
	Pixel *pixelPtr, int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];

	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	int patternQuarter = nameStart & ~0xFF;
	int patternBaseLine = (-1 << 13) | ((line + vdp->getVerticalScroll()) & 7);	// TODO check vertical scroll
	const byte *namePtr = vram->nameTable.readArea(nameStart | (-1 << 10));

	// Actual display.
	for (int name = nameStart; name < nameEnd; name++) {
		int patternNr = patternQuarter | *namePtr++;
		if (dirtyColours || dirtyName[name] || dirtyPattern[patternNr]) {
			int pattern = vram->patternTable.readNP(
				patternBaseLine | (patternNr * 8) );
			for (int i = 6; i--; ) {
				*pixelPtr++ = (pattern & 0x80) ? fg : bg;
				pattern <<= 1;
			}
		} else {
			pixelPtr += 6;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderText2(
	Pixel *pixelPtr, int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel plainFg = palFg[vdp->getForegroundColour()];
	Pixel plainBg = palBg[vdp->getBackgroundColour()];
	Pixel blinkFg, blinkBg;
 	if (vdp->getBlinkState()) {
		int fg = vdp->getBlinkForegroundColour();
		blinkFg = palBg[fg ? fg : vdp->getBlinkBackgroundColour()];
		blinkBg = palBg[vdp->getBlinkBackgroundColour()];
	} else {
		blinkFg = plainFg;
		blinkBg = plainBg;
	}

	int patternBaseLine = (-1 << 11) | ((line + vdp->getVerticalScroll()) & 7);

	// Actual display.
	// TODO: Implement blinking.
	int nameStart = (line / 8) * 80;
	int nameEnd = nameStart + 80;
	int colourPattern = 0; // avoid warning
	for (int name = nameStart; name < nameEnd; name++) {
		// Colour table contains one bit per character.
		if ((name & 7) == 0) {
			colourPattern = vram->colourTable.readNP((name >> 3) | (-1 << 9));
		} else {
			colourPattern <<= 1;
		}
		int charcode = vram->nameTable.readNP(name | (-1 << 12));
		if (dirtyColours || dirtyName[name] || dirtyPattern[charcode]) {
			Pixel fg, bg;
			if (colourPattern & 0x80) {
				fg = blinkFg;
				bg = blinkBg;
			} else {
				fg = plainFg;
				bg = plainBg;
			}
			int pattern = vram->patternTable.readNP(
				patternBaseLine | (charcode * 8) );
			if (zoom == Renderer::ZOOM_256) {
				Pixel mix = blender.blend(fg, bg);
				for (int i = 3; i--; ) {
					*pixelPtr++ =
						  (pattern & 0x80)
						? ((pattern & 0x40) ? fg : mix)
						: ((pattern & 0x40) ? mix : bg);
					pattern <<= 2;
				}
			} else {
				for (int i = 6; i--; ) {
					*pixelPtr++ = (pattern & 0x80) ? fg : bg;
					pattern <<= 1;
				}
			}
		} else {
			pixelPtr += zoom == Renderer::ZOOM_256 ? 3 : 6;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderGraphic1(
	Pixel *pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern || anyDirtyColour)) return;

	int name = (line / 8) * 32;

	const byte *namePtr = vram->nameTable.readArea(name | (-1 << 10));
	int patternBaseLine = (-1 << 11) | (line & 7);

	for (int n = 32; n--; name++) {
		int charcode = *namePtr++;
		if (dirtyName[name] || dirtyPattern[charcode]
		|| dirtyColour[charcode / 64]) {
			int colour = vram->colourTable.readNP((charcode / 8) | (-1 << 6));
			Pixel fg = palFg[colour >> 4];
			Pixel bg = palFg[colour & 0x0F];

			int pattern = vram->patternTable.readNP(
				patternBaseLine | (charcode * 8) );
			// TODO: Compare performance of this loop vs unrolling.
			for (int i = 8; i--; ) {
				*pixelPtr++ = (pattern & 0x80) ? fg : bg;
				pattern <<= 1;
			}
		} else {
			pixelPtr += 8;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderGraphic2(
	Pixel *pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern || anyDirtyColour)) return;

	int name = (line / 8) * 32;
	const byte *namePtr = vram->nameTable.readArea(name | (-1 << 10));

	int quarter = name & ~0xFF;
	int baseLine = (-1 << 13) | (quarter << 3) | (line & 7);

	for (int n = 32; n--; name++) {
		int charCode = *namePtr++;
		if (dirtyName[name] || dirtyPattern[quarter | charCode]
		|| dirtyColour[quarter | charCode]) {
			int index = (charCode * 8) | baseLine;
			int pattern = vram->patternTable.readNP(index);
			int colour = vram->colourTable.readNP(index);
			Pixel fg = palFg[colour >> 4];
			Pixel bg = palFg[colour & 0x0F];
			for (int i = 8; i--; ) {
				*pixelPtr++ = (pattern & 0x80) ? fg : bg;
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 8;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderMultiHelper(
	Pixel *pixelPtr, int line, int mask, int patternQuarter)
{
	if (!(anyDirtyName || anyDirtyPattern)) return;

	int baseLine = mask | ((line / 4) & 7);
	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	const byte *namePtr = vram->nameTable.readArea(nameStart | (-1 << 10));
	for (int name = nameStart; name < nameEnd; name++) {
		int patternNr = patternQuarter | *namePtr++;
		if (dirtyName[name] || dirtyPattern[patternNr]) {
			int colour = vram->patternTable.readNP((patternNr * 8) | baseLine);
			Pixel cl = palFg[colour >> 4];
			Pixel cr = palFg[colour & 0x0F];
			for (int n = 4; n--; ) *pixelPtr++ = cl;
			for (int n = 4; n--; ) *pixelPtr++ = cr;
		} else {
			pixelPtr += 8;
		}
	}
}
template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderMulti(
	Pixel *pixelPtr, int line)
{
	int mask = (-1 << 11);
	renderMultiHelper(pixelPtr, line, mask, 0);
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderMultiQ(
	Pixel *pixelPtr, int line)
{
	int mask = (-1 << 13);
	int patternQuarter = (line * 4) & ~0xFF;  // (line / 8) * 32
	renderMultiHelper(pixelPtr, line, mask, patternQuarter);
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderBogus(
	Pixel *pixelPtr, int line)
{
	if (!(dirtyForeground || dirtyBackground)) return;

	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];
	for (int n = 8; n--; ) *pixelPtr++ = bg;
	for (int c = 20; c--; ) {
		for (int n = 4; n--; ) *pixelPtr++ = fg;
		for (int n = 2; n--; ) *pixelPtr++ = bg;
	}
	for (int n = 8; n--; ) *pixelPtr++ = bg;
}

} // namespace openmsx

