// $Id$

#include <string.h>
#include <cassert>
#include <algorithm>

#include "GLUtil.hh"
#include "V9990.hh"
#include "V9990VRAM.hh"
#include "V9990P1Converter.hh"

using std::min;
using std::max;

namespace openmsx {

// Force template instantiation
template class V9990P1Converter<word, Renderer::ZOOM_256>;
template class V9990P1Converter<word, Renderer::ZOOM_REAL>;
template class V9990P1Converter<unsigned int, Renderer::ZOOM_256>;
template class V9990P1Converter<unsigned int, Renderer::ZOOM_REAL>;

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
template <Renderer::Zoom zoom> class V9990P1Converter<NoExpansion, zoom> {};
template class V9990P1Converter<
	ExpandFilter<GLuint>::ExpandType, Renderer::ZOOM_REAL >;
#endif // COMPONENT_GL

template <class Pixel, Renderer::Zoom zoom>
V9990P1Converter<Pixel, zoom>::V9990P1Converter(
		V9990* vdp_,
		SDL_PixelFormat format_,
		Pixel* palette64_)
		: vdp(vdp_),
		  format(format_),
		  palette64(palette64_)
{
	vram = vdp->getVRAM();
}

template <class Pixel, Renderer::Zoom zoom>
V9990P1Converter<Pixel, zoom>::~V9990P1Converter()
{
}

template <class Pixel, Renderer::Zoom zoom>
void V9990P1Converter<Pixel, zoom>::convertLine(
		Pixel* linePtr, int displayX, int displayWidth, int displayY)
{
	int i = 0;
	int prioX = 256; // TODO get prio X & Y from reg #27
	int prioY = 256;

	int visibleSprites[16 + 1];

	determineVisibleSprites(visibleSprites, displayY);
	
	if(displayY > prioY) prioX = 0;
	
	int displayAX = displayX + vdp->getScrollAX();
	int displayAY = displayY + vdp->getScrollAY();
	int displayBX = displayX + vdp->getScrollBX();
	int displayBY = displayY + vdp->getScrollBY();
	
	for(i = 0; (i < prioX) && (i < displayWidth); i++) {
		Pixel pix = raster(displayAX, displayAY, 0x7C000, 0x00000, // A
		                   displayBX, displayBY, 0x7E000, 0x40000, // B
						   visibleSprites, displayX, displayY);
		*linePtr++ = pix;
		if(zoom == Renderer::ZOOM_REAL)
			*linePtr++ = pix;

		displayX++;
		displayAX++;
		displayBX++;
	}
	for(; (i < 256) && (i < displayWidth); i++) {
		Pixel pix = raster(displayBX, displayBY, 0x7E000, 0x40000,  // B
		                   displayAX, displayAY, 0x7C000, 0x00000,  // A
						   visibleSprites, displayX, displayY);
		*linePtr++ = pix;
		if(zoom == Renderer::ZOOM_REAL)
			*linePtr++ = pix;

		displayX++;
		displayAX++;
		displayBX++;
	}
}

template <class Pixel, Renderer::Zoom zoom>
Pixel V9990P1Converter<Pixel, zoom>::raster(int xA, int yA,
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
		byte offset = vdp->getPaletteOffset();
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
					p = vdp->getBackDropColor();
				}
			}
		}
	}
	return palette64[p];
}

template <class Pixel, Renderer::Zoom zoom>
byte V9990P1Converter<Pixel, zoom>::getPixel(
		int x, int y, unsigned int nameTable, unsigned int patternTable)
{
	x &= 511;
	y &= 511;
	unsigned int address = nameTable + (((y/8)*64 + (x/8)) * 2);
	unsigned int pattern = vram->readVRAMP1(address) +
	                       vram->readVRAMP1(address+1) * 256;
	int x2 = (pattern % 32) * 8 + (x % 8);
	int y2 = (pattern / 32) * 8 + (y % 8);
	address = patternTable + y2 * 128 + x2 / 2;
	byte dixel = vram->readVRAMP1(address);
	return (x & 1) ? dixel & 0x0F : dixel >> 4;
}	

template <class Pixel, Renderer::Zoom zoom>
void V9990P1Converter<Pixel, zoom>::determineVisibleSprites(
		int* visibleSprites, int displayY)
{
	static const unsigned int spriteTable = 0x3FE00;
	
	int index = 0;
	for (int sprite = 0; sprite < 125; ++sprite) {
		int spriteInfo = spriteTable + 4 * sprite;
		byte attr = vram->readVRAMP1(spriteInfo+3);

		if(!(attr & 0x10)) {
			byte spriteY = vram->readVRAMP1(spriteInfo) + 1;
			if ((spriteY <= displayY) && (displayY < (spriteY + 16))) {
				visibleSprites[index++] = sprite;
				if (index >= 16) break;
			}
		}
	}
	visibleSprites[index] = -1;
}

template <class Pixel, Renderer::Zoom zoom>
byte V9990P1Converter<Pixel, zoom>::getSpritePixel(
		int* visibleSprites, int x, int y, bool front)
{
	static const unsigned int spriteTable = 0x3FE00;
	int spritePatternTable = vdp->getSpritePatternAddress(P1);

	for (int sprite = 0; (visibleSprites[sprite] != -1); ++sprite) {
		int   addr       = spriteTable + 4 * visibleSprites[sprite];
		int   spriteX    = vram->readVRAMP1(addr + 2);
		byte  spriteAttr = vram->readVRAMP1(addr + 3);
		spriteX += 256 * (spriteAttr & 0x03);
		if (spriteX > 1008) spriteX -= 1024; // hack X coord into -16..1008

		spriteX = x - spriteX;
		if ((0 <= spriteX) && (spriteX < 16) && 
		    (( front && !(spriteAttr & 0x20)) ||
		     (!front &&  (spriteAttr & 0x20)))) {
			byte  spriteY  = vram->readVRAMP1(addr + 0);
			byte  spriteNo = vram->readVRAMP1(addr + 1);
			spriteY = y - (spriteY + 1);
			addr  = spritePatternTable
			      + (128 * ((spriteNo & 0xF0) + spriteY))
			      + (  8 *  (spriteNo & 0x0F))
			      + (spriteX / 2);
			byte dixel = vram->readVRAMP1(addr);
			if (spriteX & 1) {
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

} // namespace openmsx


