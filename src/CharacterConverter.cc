// $Id$

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
template class CharacterConverter<byte>;
template class CharacterConverter<word>;
template class CharacterConverter<unsigned int>;

template <class Pixel> CharacterConverter<Pixel>::RenderMethod
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

template <class Pixel> CharacterConverter<Pixel>::CharacterConverter(
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

template <class Pixel> void CharacterConverter<Pixel>::renderText1(
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
		int charcode = vram->getVRAM(nameBase | name);
		if (dirtyColours || dirtyName[name] || dirtyPattern[charcode]) {
			int pattern = vram->getVRAM(patternBaseLine | (charcode * 8));
			for (int i = 6; i--; ) {
				pixelPtr[0] = pixelPtr[1] = ((pattern & 0x80) ? fg : bg);
				pixelPtr += 2;
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 12;
		}
	}
}

template <class Pixel> void CharacterConverter<Pixel>::renderText1Q(
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
		int patternNr = vram->getVRAM(nameBase | name) | patternQuarter;
		if (dirtyColours || dirtyName[name] || dirtyPattern[patternNr]) {
			int pattern = vram->getVRAM(patternBaseLine | (patternNr * 8));
			for (int i = 6; i--; ) {
				pixelPtr[0] = pixelPtr[1] = ((pattern & 0x80) ? fg : bg);
				pixelPtr += 2;
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 12;
		}
	}
}

template <class Pixel> void CharacterConverter<Pixel>::renderText2(
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
			colourPattern = vram->getVRAM(
				((-1 << 9) | (name >> 3)) & vdp->getColourMask() );
		} else {
			colourPattern <<= 1;
		}
		int maskedName = name & nameMask;
		int charcode = vram->getVRAM(nameBase | maskedName);
		if (dirtyColours || dirtyName[maskedName] || dirtyPattern[charcode]) {
			Pixel fg, bg;
			if (colourPattern & 0x80) {
				fg = blinkFg;
				bg = blinkBg;
			} else {
				fg = plainFg;
				bg = plainBg;
			}
			int pattern = vram->getVRAM(patternBaseLine | (charcode * 8));
			for (int i = 6; i--; ) {
				*pixelPtr++ = (pattern & 0x80) ? fg : bg;
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 6;
		}
	}
}

template <class Pixel> void CharacterConverter<Pixel>::renderGraphic1(
	Pixel *pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern || anyDirtyColour)) return;

	int name = (line / 8) * 32;

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | (line & 7)) & vdp->getPatternMask();
	int colourBase = (-1 << 6) & vdp->getColourMask();

	for (int x = 0; x < 256; x += 8) {
		int charcode = vram->getVRAM(nameBase | name);
		if (dirtyName[name] || dirtyPattern[charcode]
		|| dirtyColour[charcode / 64]) {
			int colour = vram->getVRAM(colourBase | (charcode / 8));
			Pixel fg = palFg[colour >> 4];
			Pixel bg = palFg[colour & 0x0F];

			int pattern = vram->getVRAM(patternBaseLine | (charcode * 8));
			// TODO: Compare performance of this loop vs unrolling.
			for (int i = 8; i--; ) {
				pixelPtr[0] = pixelPtr[1] = ((pattern & 0x80) ? fg : bg);
				pixelPtr += 2;
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 16;
		}
		name++;
	}
}

template <class Pixel> void CharacterConverter<Pixel>::renderGraphic2(
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
		int charCode = vram->getVRAM(nameBase | name);
		int colourNr = (quarter | charCode) & colourNrBase;
		int patternNr = patternQuarter | charCode;
		if (dirtyName[name] || dirtyPattern[patternNr]
		|| dirtyColour[colourNr]) {
			int pattern = vram->getVRAM(patternBaseLine | (patternNr * 8));
			int colour = vram->getVRAM(colourBaseLine | (colourNr * 8));
			Pixel fg = palFg[colour >> 4];
			Pixel bg = palFg[colour & 0x0F];
			for (int i = 8; i--; ) {
				pixelPtr[0] = pixelPtr[1] = ((pattern & 0x80) ? fg : bg);
				pixelPtr += 2;
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 16;
		}
	}
}

template <class Pixel> void CharacterConverter<Pixel>::renderMulti(
	Pixel *pixelPtr, int line)
{
	if (!(anyDirtyName || anyDirtyPattern)) return;

	int nameBase = (-1 << 10) & vdp->getNameMask();
	int patternBaseLine = ((-1 << 11) | ((line / 4) & 7))
		& vdp->getPatternMask();
	int nameStart = (line / 8) * 32;
	int nameEnd = nameStart + 32;
	for (int name = nameStart; name < nameEnd; name++) {
		int charcode = vram->getVRAM(nameBase | name);
		if (dirtyName[name] || dirtyPattern[charcode]) {
			int colour = vram->getVRAM(patternBaseLine | (charcode * 8));
			Pixel cl = palFg[colour >> 4];
			Pixel cr = palFg[colour & 0x0F];
			for (int n = 8; n--; ) *pixelPtr++ = cl;
			for (int n = 8; n--; ) *pixelPtr++ = cr;
		}
		else {
			pixelPtr += 16;
		}
	}
}

template <class Pixel> void CharacterConverter<Pixel>::renderMultiQ(
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
		int patternNr = patternQuarter | vram->getVRAM(nameBase | name);
		if (dirtyName[name] || dirtyPattern[patternNr]) {
			int colour = vram->getVRAM(patternBaseLine | (patternNr * 8));
			Pixel cl = palFg[colour >> 4];
			Pixel cr = palFg[colour & 0x0F];
			for (int n = 8; n--; ) *pixelPtr++ = cl;
			for (int n = 8; n--; ) *pixelPtr++ = cr;
		}
		else {
			pixelPtr += 16;
		}
	}
}

template <class Pixel> void CharacterConverter<Pixel>::renderBogus(
	Pixel *pixelPtr, int line)
{
	if (!(dirtyForeground || dirtyBackground)) return;

	Pixel fg = palFg[vdp->getForegroundColour()];
	Pixel bg = palBg[vdp->getBackgroundColour()];
	for (int n = 16; n--; ) *pixelPtr++ = bg;
	for (int c = 40; c--; ) {
		for (int n = 8; n--; ) *pixelPtr++ = fg;
		for (int n = 4; n--; ) *pixelPtr++ = bg;
	}
	for (int n = 16; n--; ) *pixelPtr++ = bg;
}

