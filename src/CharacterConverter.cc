// $Id$

/*
TODO:
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
#include "VDP.hh"
#include "VDPVRAM.hh"

/** Fill a boolean array with a single value.
  * Optimised for byte-sized booleans,
  * but correct for every size.
  * TODO: Find a proper location for this "toolkit" function.
  */
inline static void fillBool(bool *ptr, bool value, int nr)
{
#if SIZEOF_BOOL == 1
	memset(ptr, value, nr);
#else
	for (int i = nr; i--; ) *ptr++ = value;
#endif
}

// Force template instantiation for these types.
// Without this, object file contains no method implementations.
template class CharacterConverter<byte, Renderer::ZOOM_256>;
template class CharacterConverter<byte, Renderer::ZOOM_512>;
template class CharacterConverter<word, Renderer::ZOOM_256>;
template class CharacterConverter<word, Renderer::ZOOM_512>;
template class CharacterConverter<unsigned int, Renderer::ZOOM_256>;
template class CharacterConverter<unsigned int, Renderer::ZOOM_512>;
template class CharacterConverter<unsigned int, Renderer::ZOOM_REAL>;

template <class Pixel, Renderer::Zoom zoom>
CharacterConverter<Pixel, zoom>::RenderMethod
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
	VDP *vdp, const Pixel *palFg, const Pixel *palBg)
{
	this->vdp = vdp;
	this->palFg = palFg;
	this->palBg = palBg;
	vram = vdp->getVRAM();

	anyDirtyColour = anyDirtyPattern = anyDirtyName = true;
	fillBool(dirtyColour, true, sizeof(dirtyColour));
	fillBool(dirtyPattern, true, sizeof(dirtyPattern));
	fillBool(dirtyName, true, sizeof(dirtyName));
	dirtyForeground = dirtyBackground = true;
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderText1(
	Pixel *pixelPtr, int line)
{
	bool dirtyColours = dirtyForeground || dirtyBackground;
	if (!(anyDirtyName || anyDirtyPattern || dirtyColours)) return;

	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | (line & 7)) & vdp->getPatternMask();

	// Actual display.
	int nameStart = (line / 8) * 40;
	int nameEnd = nameStart + 40;
	for (int name = nameStart; name < nameEnd; name++) {
		int charcode = vram->readNP(nameBase | name);
		if (dirtyColours || dirtyName[name] || dirtyPattern[charcode]) {
			int pattern = vram->readNP(patternBaseLine | (charcode * 8));
			for (int i = 6; i--; ) {
				if (zoom == Renderer::ZOOM_512) {
					pixelPtr[0] = pixelPtr[1] = (pattern & 0x80) ? fg : bg;
					pixelPtr += 2;
				} else {
					*pixelPtr++ = (pattern & 0x80) ? fg : bg;
				}
				pattern <<= 1;
			}
		} else {
			pixelPtr += zoom == Renderer::ZOOM_512 ? 12 : 6;
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

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	int patternQuarter = (nameStart & ~0xFF) & (vdp->getPatternMask() / 8);
	int patternBaseLine = ((-1 << 13) | (line & 7)) & vdp->getPatternMask();

	// Actual display.
	for (int name = nameStart; name < nameEnd; name++) {
		int patternNr = vram->readNP(nameBase | name) | patternQuarter;
		if (dirtyColours || dirtyName[name] || dirtyPattern[patternNr]) {
			int pattern = vram->readNP(patternBaseLine | (patternNr * 8));
			for (int i = 6; i--; ) {
				if (zoom == Renderer::ZOOM_512) {
					pixelPtr[0] = pixelPtr[1] = (pattern & 0x80) ? fg : bg;
					pixelPtr += 2;
				} else {
					*pixelPtr++ = (pattern & 0x80) ? fg : bg;
				}
				pattern <<= 1;
			}
		} else {
			pixelPtr += zoom == Renderer::ZOOM_512 ? 12 : 6;
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

	// TODO: This name masking seems unnecessarily complex.
	int nameBase = (-1 << 12) & vdp->getNameMask();
	int nameMask = ~(-1 << 12) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | (line & 7)) & vdp->getPatternMask();

	// Actual display.
	// TODO: Implement blinking.
	int nameStart = (line / 8) * 80;
	int nameEnd = nameStart + 80;
	int colourPattern = 0; // avoid warning
	for (int name = nameStart; name < nameEnd; name++) {
		// Colour table contains one bit per character.
		if ((name & 7) == 0) {
			colourPattern = vram->readNP(
				((-1 << 9) | (name >> 3)) & vdp->getColourMask() );
		} else {
			colourPattern <<= 1;
		}
		int maskedName = name & nameMask;
		int charcode = vram->readNP(nameBase | maskedName);
		if (dirtyColours || dirtyName[maskedName] || dirtyPattern[charcode]) {
			Pixel fg, bg;
			if (colourPattern & 0x80) {
				fg = blinkFg;
				bg = blinkBg;
			} else {
				fg = plainFg;
				bg = plainBg;
			}
			int pattern = vram->readNP(patternBaseLine | (charcode * 8));
			if (zoom == Renderer::ZOOM_256) {
				for (int i = 3; i--; ) {
					*pixelPtr++ = (pattern & 0xC0) ? fg : bg;
					pattern <<= 1;
				}
			} else {
				for (int i = 6; i--; ) {
					*pixelPtr++ = (pattern & 0x80) ? fg : bg;
					pattern <<= 1;
				}
			}
		}
		else {
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

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | (line & 7)) & vdp->getPatternMask();
	int colourBase = (-1 << 6) & vdp->getColourMask();

	for (int x = 0; x < 256; x += 8) {
		int charcode = vram->readNP(nameBase | name);
		if (dirtyName[name] || dirtyPattern[charcode]
		|| dirtyColour[charcode / 64]) {
			int colour = vram->readNP(colourBase | (charcode / 8));
			Pixel fg = palFg[colour >> 4];
			Pixel bg = palFg[colour & 0x0F];

			int pattern = vram->readNP(patternBaseLine | (charcode * 8));
			// TODO: Compare performance of this loop vs unrolling.
			for (int i = 8; i--; ) {
				if (zoom == Renderer::ZOOM_512) {
					pixelPtr[0] = pixelPtr[1] = (pattern & 0x80) ? fg : bg;
					pixelPtr += 2;
				} else {
					*pixelPtr++ = (pattern & 0x80) ? fg : bg;
				}
				pattern <<= 1;
			}
		} else {
			pixelPtr += zoom == Renderer::ZOOM_512 ? 16 : 8;
		}
		name++;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderGraphic2(
	Pixel *pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern || anyDirtyColour)) return;

	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;

	int quarter = nameStart & ~0xFF;
	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternQuarter = quarter & (vdp->getPatternMask() / 8);
	int patternBaseLine = ((-1 << 13) | (line & 7)) & vdp->getPatternMask();
	int colourNrBase = 0x3FF & (vdp->getColourMask() / 8);
	int colourBaseLine = ((-1 << 13) | (line & 7)) & vdp->getColourMask();
	for (int name = nameStart; name < nameEnd; name++) {
		int charCode = vram->readNP(nameBase | name);
		int colourNr = (quarter | charCode) & colourNrBase;
		int patternNr = patternQuarter | charCode;
		if (dirtyName[name] || dirtyPattern[patternNr]
		|| dirtyColour[colourNr]) {
			int pattern = vram->readNP(patternBaseLine | (patternNr * 8));
			int colour = vram->readNP(colourBaseLine | (colourNr * 8));
			Pixel fg = palFg[colour >> 4];
			Pixel bg = palFg[colour & 0x0F];
			for (int i = 8; i--; ) {
				if (zoom == Renderer::ZOOM_512) {
					pixelPtr[0] = pixelPtr[1] = (pattern & 0x80) ? fg : bg;
					pixelPtr += 2;
				} else {
					*pixelPtr++ = (pattern & 0x80) ? fg : bg;
				}
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += zoom == Renderer::ZOOM_512 ? 16 : 8;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderMulti(
	Pixel *pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern)) return;

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | ((line / 4) & 7))
		& vdp->getPatternMask();
	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	for (int name = nameStart; name < nameEnd; name++) {
		int charcode = vram->readNP(nameBase | name);
		if (dirtyName[name] || dirtyPattern[charcode]) {
			int colour = vram->readNP(patternBaseLine | (charcode * 8));
			Pixel cl = palFg[colour >> 4];
			Pixel cr = palFg[colour & 0x0F];
			if (zoom == Renderer::ZOOM_512) {
				for (int n = 8; n--; ) *pixelPtr++ = cl;
				for (int n = 8; n--; ) *pixelPtr++ = cr;
			} else {
				for (int n = 4; n--; ) *pixelPtr++ = cl;
				for (int n = 4; n--; ) *pixelPtr++ = cr;
			}
		} else {
			pixelPtr += zoom == Renderer::ZOOM_512 ? 16 : 8;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderMultiQ(
	Pixel *pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern)) return;

	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternQuarter = (nameStart & ~0xFF) & (vdp->getPatternMask() / 8);
	int patternBaseLine = ((-1 << 13) | ((line / 4) & 7))
		& vdp->getPatternMask();
	for (int name = nameStart; name < nameEnd; name++) {
		int patternNr = patternQuarter | vram->readNP(nameBase | name);
		if (dirtyName[name] || dirtyPattern[patternNr]) {
			int colour = vram->readNP(patternBaseLine | (patternNr * 8));
			Pixel cl = palFg[colour >> 4];
			Pixel cr = palFg[colour & 0x0F];
			if (zoom == Renderer::ZOOM_512) {
				for (int n = 8; n--; ) *pixelPtr++ = cl;
				for (int n = 8; n--; ) *pixelPtr++ = cr;
			} else {
				for (int n = 4; n--; ) *pixelPtr++ = cl;
				for (int n = 4; n--; ) *pixelPtr++ = cr;
			}
		} else {
			pixelPtr += zoom == Renderer::ZOOM_512 ? 16 : 8;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void CharacterConverter<Pixel, zoom>::renderBogus(
	Pixel *pixelPtr, int line)
{
	if (!(dirtyForeground || dirtyBackground)) return;

	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];
	if (zoom == Renderer::ZOOM_512) {
		for (int n = 16; n--; ) *pixelPtr++ = bg;
		for (int c = 40; c--; ) {
			for (int n = 8; n--; ) *pixelPtr++ = fg;
			for (int n = 4; n--; ) *pixelPtr++ = bg;
		}
		for (int n = 16; n--; ) *pixelPtr++ = bg;
	} else {
		for (int n = 8; n--; ) *pixelPtr++ = bg;
		for (int c = 20; c--; ) {
			for (int n = 4; n--; ) *pixelPtr++ = fg;
			for (int n = 2; n--; ) *pixelPtr++ = bg;
		}
		for (int n = 8; n--; ) *pixelPtr++ = bg;
	}
}

