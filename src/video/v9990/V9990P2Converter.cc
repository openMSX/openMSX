// $Id$

#include "V9990P2Converter.hh"
#include "V9990VRAM.hh"
#include "V9990.hh"
#include "GLUtil.hh"

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
template <> class V9990P2Converter<NoExpansion> {};
template class V9990P2Converter<ExpandFilter<GLuint>::ExpandType>;
#endif // COMPONENT_GL

template <class Pixel>
V9990P2Converter<Pixel>::V9990P2Converter(V9990& vdp_, Pixel* palette64_)
	: vdp(vdp_), vram(vdp.getVRAM()), palette64(palette64_)
{
}

template <class Pixel>
void V9990P2Converter<Pixel>::convertLine(
	Pixel* linePtr, int displayX, int displayWidth, int displayY)
{
	int displayAX = displayX + vdp.getScrollAX();
	int displayAY = displayY + vdp.getScrollAY();

	int visibleSprites[16 + 1];
	determineVisibleSprites(visibleSprites, displayY);

	int displayEnd = displayX + displayWidth;
	for (/* */; displayX < displayEnd; ++displayX) {
		*linePtr++ = raster(displayAX, displayAY,
		                    visibleSprites, displayX, displayY);
		++displayAX;
	}
}

template <class Pixel>
Pixel V9990P2Converter<Pixel>::raster(unsigned xA, unsigned yA,
		int* visibleSprites, unsigned x, unsigned y)
{
	// front sprite plane
	byte p = getSpritePixel(visibleSprites, x, y, true);

	if (!(p & 0x0F)) {
		// image plane
		byte offset = vdp.getPaletteOffset();
		p = getPixel(xA, yA) + ((offset & 0x03) << 4);

		if (!(p & 0x0F)) {
			// back sprite plane
			p = getSpritePixel(visibleSprites, x, y, false);

			if (!(p & 0x0F)) {
				// backdrop color
				p = vdp.getBackDropColor();
			}
		}
	}
	return palette64[p];
}

template <class Pixel>
byte V9990P2Converter<Pixel>::getPixel(unsigned x, unsigned y)
{
	static const unsigned patternTable = 0x00000;
	static const unsigned nameTable    = 0x7C000;

	// TODO optimization: more specific readVRAMP2 methods
	x &= 1023;
	y &= 511;
	unsigned chrAddr = nameTable + (((y / 8) * 128 + (x / 8)) * 2);
	unsigned pattern = (vram.readVRAMDirect(chrAddr + 0) +
	                    vram.readVRAMDirect(chrAddr + 1) * 256) & 0x3FFF;
	unsigned x2 = (pattern % 64) * 8 + (x % 8);
	unsigned y2 = (pattern / 64) * 8 + (y % 8);
	unsigned patAddr = patternTable + y2 * 256 + x2 / 2;
	byte dixel = vram.readVRAMBx(patAddr); // interleaved
	return (x & 1) ? (dixel & 0x0F) : (dixel >> 4);
}

template <class Pixel>
void V9990P2Converter<Pixel>::determineVisibleSprites(
	int* visibleSprites, int displayY)
{
	static const unsigned spriteTable = 0x3FE00;

	int index = 0;
	for (int sprite = 0; sprite < 125; ++sprite) {
		int spriteInfo = spriteTable + 4 * sprite;
		byte attr = vram.readVRAMDirect(spriteInfo + 3);

		if (!(attr & 0x10)) {
			byte spriteY = vram.readVRAMDirect(spriteInfo) + 1;
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
byte V9990P2Converter<Pixel>::getSpritePixel(
	int* visibleSprites, int x, int y, bool front)
{
	static const unsigned int spriteTable = 0x3FE00;
	int spritePatternTable = vdp.getSpritePatternAddress(P2);

	for (unsigned sprite = 0; visibleSprites[sprite] != -1; ++sprite) {
		unsigned attAddr = spriteTable + 4 * visibleSprites[sprite];
		int spriteX = vram.readVRAMDirect(attAddr + 2);
		byte spriteAttr = vram.readVRAMDirect(attAddr + 3);
		spriteX += 256 * (spriteAttr & 0x03);
		if (spriteX > 1008) spriteX -= 1024; // hack X coord into -16..1008

		unsigned posX = x - spriteX;
		if ((posX < 16) && (front ^ !!(spriteAttr & 0x20))) {
			byte spriteY  = vram.readVRAMDirect(attAddr + 0);
			byte spriteNo = vram.readVRAMDirect(attAddr + 1);
			spriteY = y - (spriteY + 1);
			unsigned patAddr = spritePatternTable
			     + (256 * (((spriteNo & 0xE0) >> 1) + spriteY))
			     + (  8 *  (spriteNo & 0x1F))
			     + (posX / 2);
			byte dixel = vram.readVRAMBx(patAddr); // interleaved
			if (posX & 1) {
				dixel &= 0x0F;
			} else {
				dixel >>= 4;
			}
			if (dixel) {
				return dixel | ((spriteAttr >> 2) & 0x30);
			}
		}
	}
	return 0;
}


// Force template instantiation
template class V9990P2Converter<word>;
template class V9990P2Converter<unsigned int>;

} // namespace openmsx
