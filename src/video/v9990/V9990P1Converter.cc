// $Id$

#include <cassert>
#include <algorithm>

#include "GLUtil.hh"
#include "V9990.hh"
#include "V9990VRAM.hh"
#include "V9990P1Converter.hh"

namespace openmsx {

#ifdef COMPONENT_GL
// On some systems, "GLuint" is not equivalent to "unsigned int",
// so BitmapConverter must be instantiated separately for those systems.
// But on systems where it is equivalent, it's an error to expand
// the same template twice.
// The following piece of template metaprogramming expands
// V9990BitmapConverter<GLuint, Renderer::ZOOM_REAL> to an empty class if
// "GLuint" is equivalent to "unsigned int"; otherwise it is expanded to
// the actual V9990BitmapConverter implementation.
//
// See BitmapConverter.cc
class NoExpansion {};
// ExpandFilter::ExpandType = (Type == unsigned int ? NoExpansion : Type)
template <class Type> class ExpandFilter {
	typedef Type ExpandType;
};
template <> class ExpandFilter<unsigned int> {
	typedef NoExpansion ExpandType;
};
template <> class V9990P1Converter<NoExpansion> {};
template class V9990P1Converter<ExpandFilter<GLuint>::ExpandType>;
#endif // COMPONENT_GL

template <class Pixel>
V9990P1Converter<Pixel>::V9990P1Converter(V9990& vdp_, Pixel* palette64_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, palette64(palette64_)
{
}

template <class Pixel>
V9990P1Converter<Pixel>::~V9990P1Converter()
{
}

template <class Pixel>
void V9990P1Converter<Pixel>::convertLine(
	Pixel* linePtr, int displayX, int displayWidth, int displayY)
{
	int prioX = 256; // TODO get prio X & Y from reg #27
	int prioY = 256;

	int visibleSprites[16 + 1];
	determineVisibleSprites(visibleSprites, displayY);

	if (displayY > prioY) prioX = 0;

	int displayAX = displayX + vdp.getScrollAX();
	int displayAY = displayY + vdp.getScrollAY();
	int displayBX = displayX + vdp.getScrollBX();
	int displayBY = displayY + vdp.getScrollBY();

	int displayEnd = displayX + displayWidth;
	int end = std::min(prioX, displayEnd);
	for (/* */; displayX < end; ++displayX) {
		Pixel pix = raster(displayAX, displayAY, 0x7C000, 0x00000, // A
		                   displayBX, displayBY, 0x7E000, 0x40000, // B
		                   visibleSprites, displayX, displayY);
		*linePtr++ = pix;
		*linePtr++ = pix;

		displayAX++;
		displayBX++;
	}
	for (/* */; displayX < displayEnd; ++displayX) {
		Pixel pix = raster(displayBX, displayBY, 0x7E000, 0x40000, // B
		                   displayAX, displayAY, 0x7C000, 0x00000, // A
		                   visibleSprites, displayX, displayY);
		*linePtr++ = pix;
		*linePtr++ = pix;

		displayAX++;
		displayBX++;
	}
}

template <class Pixel>
Pixel V9990P1Converter<Pixel>::raster(int xA, int yA,
	unsigned int nameTableA, unsigned int patternTableA,
	int xB, int yB,
	unsigned int nameTableB, unsigned int patternTableB,
	int* visibleSprites,
	unsigned int x, unsigned int y)
{
	// Front sprite plane
	byte p = getSpritePixel(visibleSprites, x, y, true);

	if (!(p & 0x0F)) {
		// Front image plane
		byte offset = vdp.getPaletteOffset();
		p = getPixel(xA, yA, nameTableA, patternTableA) +
		    ((offset & 0x03) << 4);

		if (!(p & 0x0F)) {
			// Back sprite plane
			p = getSpritePixel(visibleSprites, x, y, false);

			if (!(p & 0x0F)) {
				// Back image plane
				p = getPixel(xB, yB, nameTableB, patternTableB) +
				    ((offset & 0x0C) << 2);

				if (!(p & 0x0F)) {
					// Backdrop color
					p = vdp.getBackDropColor();
				}
			}
		}
	}
	return palette64[p];
}

template <class Pixel>
byte V9990P1Converter<Pixel>::getPixel(
	int x, int y, unsigned int nameTable, unsigned int patternTable)
{
	x &= 511;
	y &= 511;
	unsigned int address = nameTable + (((y / 8) * 64 + (x / 8)) * 2);
	unsigned int pattern = (vram.readVRAMP1(address + 0) +
	                        vram.readVRAMP1(address + 1) * 256) & 0x1FFF;
	int x2 = (pattern % 32) * 8 + (x % 8);
	int y2 = (pattern / 32) * 8 + (y % 8);
	address = patternTable + y2 * 128 + x2 / 2;
	byte dixel = vram.readVRAMP1(address);
	if (!(x & 1)) dixel >>= 4;
	return dixel & 0x0F;
}

template <class Pixel>
void V9990P1Converter<Pixel>::determineVisibleSprites(
	int* visibleSprites, int displayY)
{
	static const unsigned int spriteTable = 0x3FE00;

	int index = 0;
	for (int sprite = 0; sprite < 125; ++sprite) {
		int spriteInfo = spriteTable + 4 * sprite;
		byte attr = vram.readVRAMP1(spriteInfo + 3);

		if (!(attr & 0x10)) {
			byte spriteY = vram.readVRAMP1(spriteInfo) + 1;
			byte posY = displayY - spriteY;
			if (posY < 16) {
				visibleSprites[index++] = sprite;
				if (index == 16) break;
			}
		}
	}
	visibleSprites[index] = -1;
}

template <class Pixel>
byte V9990P1Converter<Pixel>::getSpritePixel(
	int* visibleSprites, int x, int y, bool front)
{
	static const unsigned int spriteTable = 0x3FE00;
	int spritePatternTable = vdp.getSpritePatternAddress(P1);

	for (int sprite = 0; visibleSprites[sprite] != -1; ++sprite) {
		int   addr       = spriteTable + 4 * visibleSprites[sprite];
		int   spriteX    = vram.readVRAMP1(addr + 2);
		byte  spriteAttr = vram.readVRAMP1(addr + 3);
		spriteX += 256 * (spriteAttr & 0x03);
		if (spriteX > 1008) spriteX -= 1024; // hack X coord into -16..1008

		unsigned posX = x - spriteX;
		if ((posX < 16) && (front ^ !!(spriteAttr & 0x20))) {
			byte spriteY  = vram.readVRAMP1(addr + 0);
			byte spriteNo = vram.readVRAMP1(addr + 1);
			spriteY = y - (spriteY + 1);
			addr = spritePatternTable
			     + (128 * ((spriteNo & 0xF0) + spriteY))
			     + (  8 *  (spriteNo & 0x0F))
			     + (posX / 2);
			byte dixel = vram.readVRAMP1(addr);
			if (!(posX & 1)) dixel >>= 4;
			dixel &= 0x0F;
			if (dixel) {
				return dixel | ((spriteAttr >> 2) & 0x30);
			}
		}
	}
	return 0;
}


// Force template instantiation
template class V9990P1Converter<word>;
template class V9990P1Converter<unsigned>;

} // namespace openmsx
