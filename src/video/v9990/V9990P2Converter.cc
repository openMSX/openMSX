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
V9990P2Converter<Pixel>::~V9990P2Converter()
{
}

template <class Pixel>
void V9990P2Converter<Pixel>::convertLine(
	Pixel* linePtr, int displayX, int /*displayWidth*/, int displayY)
{
	displayX = displayX + vdp.getScrollAX();
	displayY = displayY + vdp.getScrollAY();

	for (int i = 0; i < 256; ++i) {
		*linePtr++ = raster(displayX++, displayY, 0x7C000, 0x00000);
		*linePtr++ = raster(displayX++, displayY, 0x7C000, 0x00000);
	}
}

template <class Pixel>
Pixel V9990P2Converter<Pixel>::raster(int x, int y,
		unsigned int nameTable, unsigned int patternTable)
{
	byte offset = vdp.getPaletteOffset();

	byte p = getPixel(x, y, nameTable, patternTable) +
	         ((offset & 0x03) << 4);
	if (!(p & 0x0F)) { return vdp.getBackDropColor(); }
	return palette64[p];
}

template <class Pixel>
byte V9990P2Converter<Pixel>::getPixel(
		int x, int y, unsigned int nameTable, unsigned int patternTable)
{
	x &= 1023;
	y &= 1023;
	unsigned int address = nameTable + (((y/8)*128 + (x/8)) * 2);
	unsigned int pattern = (vram.readVRAM(address + 0) +
	                        vram.readVRAM(address + 1) * 256) & 0x3FFF;
	int x2 = (pattern%64) * 8 + (x%8);
	int y2 = (pattern/64) * 8 + (y%8);
	address = patternTable + y2 * 256 + x2/2;
	byte dixel = vram.readVRAM(address);
	if(!(x & 1)) dixel >>= 4;
	return dixel & 15;
}


// Force template instantiation
template class V9990P2Converter<word>;
template class V9990P2Converter<unsigned int>;

} // namespace openmsx
