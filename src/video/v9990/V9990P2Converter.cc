// $Id$

#include "V9990P2Converter.hh"
#include "V9990VRAM.hh"
#include "V9990.hh"
#include "GLUtil.hh"

#include <string.h>
#include <cassert>
#include <algorithm>

using std::min;
using std::max;

namespace openmsx {

// Force template instantiation
template class V9990P2Converter<word, Renderer::ZOOM_256>;
template class V9990P2Converter<word, Renderer::ZOOM_REAL>;
template class V9990P2Converter<unsigned int, Renderer::ZOOM_256>;
template class V9990P2Converter<unsigned int, Renderer::ZOOM_REAL>;

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
template <Renderer::Zoom zoom> class V9990P2Converter<NoExpansion, zoom> {};
template class V9990P2Converter<
	ExpandFilter<GLuint>::ExpandType, Renderer::ZOOM_REAL >;
#endif // COMPONENT_GL

template <class Pixel, Renderer::Zoom zoom>
V9990P2Converter<Pixel, zoom>::V9990P2Converter(
		V9990* vdp_,
		Pixel* palette64_)
		: vdp(vdp_),
		  palette64(palette64_)
{
	vram = vdp->getVRAM();
}

template <class Pixel, Renderer::Zoom zoom>
V9990P2Converter<Pixel, zoom>::~V9990P2Converter()
{
}

template <class Pixel, Renderer::Zoom zoom>
void V9990P2Converter<Pixel, zoom>::convertLine(
		Pixel* linePtr, int displayX, int displayWidth, int displayY)
{
	int i = 0;
	
	displayX = displayX + vdp->getScrollAX();
	displayY = displayY + vdp->getScrollAY();
	
	for(i = 0; i < 256; i++) {
		Pixel pix1 = raster(displayX++, displayY, 0x7C000, 0x00000);
		Pixel pix2 = raster(displayX++, displayY, 0x7C000, 0x00000);
		
		if(zoom == Renderer::ZOOM_REAL) {
			*linePtr++ = pix1;
			*linePtr++ = pix2;
		} else {
			// TODO use blender a la BitmapConverter::blendPixels2()
			*linePtr++ = pix1;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
Pixel V9990P2Converter<Pixel, zoom>::raster(int x, int y,
		unsigned int nameTable, unsigned int patternTable)
{
	byte offset = vdp->getPaletteOffset();
	
	byte p = getPixel(x, y, nameTable, patternTable) +
	         ((offset & 0x03) << 4);
	if (!(p & 0x0F)) { return vdp->getBackDropColor(); }
	return palette64[p];
}

template <class Pixel, Renderer::Zoom zoom>
byte V9990P2Converter<Pixel, zoom>::getPixel(
		int x, int y, unsigned int nameTable, unsigned int patternTable)
{
	x &= 1023;
	y &= 1023;
	unsigned int address = nameTable + (((y/8)*128 + (x/8)) * 2);
	unsigned int pattern = (vram->readVRAM(address + 0) +
	                        vram->readVRAM(address + 1) * 256) & 0x3FFF;
	int x2 = (pattern%64) * 8 + (x%8);
	int y2 = (pattern/64) * 8 + (y%8);
	address = patternTable + y2 * 256 + x2/2;
	byte dixel = vram->readVRAM(address);
	if(!(x & 1)) dixel >>= 4;
	return dixel & 15;
}	

}

