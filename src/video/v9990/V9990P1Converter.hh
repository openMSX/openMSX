// $Id$

#ifndef __V9990P1CONVERTER_HH__
#define __V9990P1CONVERTER_HH__

#include <SDL.h>
#include "openmsx.hh"
#include "Renderer.hh"
#include "V9990ModeEnum.hh"

namespace openmsx {

class V9990;
class V9990VRAM;

template <class Pixel, Renderer::Zoom zoom>
class V9990P1Converter
{
public:
	
	V9990P1Converter( V9990* vdp_, SDL_PixelFormat format_, Pixel* palette64_);
	~V9990P1Converter( );

	void convertLine(Pixel* linePtr,
	                 int displayX, int displayWidth, int displayY);

private:
	V9990* vdp;
	SDL_PixelFormat format;
	Pixel* palette64;
	V9990VRAM* vram;

	Pixel raster(int xA, int yA,
	             unsigned int nameTableA, unsigned int patternTableA,
				 int xB, int yB,
	             unsigned int nameTableB, unsigned int patternTableB);
	Pixel getPixel(int x, int y,
	               unsigned int nameTable, unsigned int patternTable);
};

} // namespace

#endif // __V9990PATTERNCONVERTER_HH__
