// $Id$

#ifndef __SAI2XSCALER_HH__
#define __SAI2XSCALER_HH__

#include "Scaler.hh"
#include "Blender.hh"

namespace openmsx {

/** 2xSaI algorithm: edge-detection which produces a rounded look.
  * Algorithm was developed by Derek Liauw Kie Fa.
  */
template <class Pixel>
class SaI2xScaler: public Scaler<Pixel>
{
public:
	SaI2xScaler(SDL_PixelFormat* format);
	void scale256(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY );
	void scale512(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY );
private:
	void scaleLine256(
		const Pixel* srcLine0, const Pixel* srcLine1,
		const Pixel* srcLine2, const Pixel* srcLine3,
		Pixel* dstUpper, Pixel* dstLower );
	void scaleLine512(
		const Pixel* srcLine0, const Pixel* srcLine1,
		const Pixel* srcLine2, const Pixel* srcLine3,
		Pixel* dstUpper, Pixel* dstLower );
	Blender<Pixel> blender;
};

} // namespace openmsx

#endif // __SAI2XSCALER_HH__
