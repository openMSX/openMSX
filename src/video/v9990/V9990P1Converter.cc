// $Id$

#include "V9990P1Converter.hh"
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

	if(displayY > prioY) prioX = 0;
	
	int displayAX = displayX + vdp->getScrollAX();
	int displayAY = displayY + vdp->getScrollAY();
	int displayBX = displayX + vdp->getScrollBX();
	int displayBY = displayY + vdp->getScrollBY();
	
	/* TODO: displayX += scrollX; displayY += scrollY */
	
	for(i = 0; (i < prioX) && (i < displayWidth); i++) {
		Pixel pix = raster(displayAX, displayAY, 0x7C000, 0x00000,  // A
		                   displayBX, displayBY, 0x7E000, 0x40000); // B
		*linePtr++ = pix;
		if(zoom == Renderer::ZOOM_REAL)
			*linePtr++ = pix;

		displayAX++;
		displayBX++;
	}
	for(; (i < 256) && (i < displayWidth); i++) {
		Pixel pix = raster(displayBX, displayBY, 0x7E000, 0x40000,  // B
		                   displayAX, displayAY, 0x7C000, 0x00000); // A
		*linePtr++ = pix;
		if(zoom == Renderer::ZOOM_REAL)
			*linePtr++ = pix;

		displayAX++;
		displayBX++;
	}
}

template <class Pixel, Renderer::Zoom zoom>
Pixel V9990P1Converter<Pixel, zoom>::raster(int xA, int yA,
		unsigned int nameTableA, unsigned int patternTableA,
		int xB, int yB,
		unsigned int nameTableB, unsigned int patternTableB)
{
	Pixel      p = getPixel(xA, yB, nameTableA, patternTableA);
	if(p == 0) p = getPixel(xA, yB, nameTableB, patternTableB);
	if(p == 0) p = vdp->getBackDropColor();
	return p;
}

template <class Pixel, Renderer::Zoom zoom>
Pixel V9990P1Converter<Pixel, zoom>::getPixel(
		int x, int y, unsigned int nameTable, unsigned int patternTable)
{
	unsigned int address = nameTable + (((y/8)*64 + (x/8)) * 2);
	unsigned int pattern = vram->readVRAM(address) +
	                       vram->readVRAM(address+1) * 256;
	int x2 = (pattern%32) * 8 + (x%8);
	int y2 = (pattern/32) * 8 + (y%8);
	address = patternTable + y2 * 128 + x2/2;
	byte dixel = vram->readVRAM(address);
	if(!(x & 1)) dixel >>= 4;
	return palette64[(vdp->getPaletteOffset() & 0x30) + (dixel & 15)];
}	

}

