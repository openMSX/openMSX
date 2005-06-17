// $Id$

#ifndef V9990P2CONVERTER_HH
#define V9990P2CONVERTER_HH

#include "openmsx.hh"
#include "Renderer.hh"

namespace openmsx {

class V9990;
class V9990VRAM;

template <class Pixel, Renderer::Zoom zoom>
class V9990P2Converter
{
public:
	V9990P2Converter(V9990& vdp_, Pixel* palette64);
	~V9990P2Converter();

	void convertLine(Pixel* linePtr,
	                 int displayX, int displayWidth, int displayY);

private:
	V9990& vdp;
	V9990VRAM& vram;
	Pixel* palette64;

	Pixel raster(int x, int y,
	             unsigned int nameTable, unsigned int patternTable);
	byte getPixel(int x, int y,
	              unsigned int nameTable, unsigned int patternTable);
};

} // namespace

#endif
