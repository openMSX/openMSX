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
	void scaleLine256(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
	void scaleLine512(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
private:
	Blender<Pixel> blender;
};

} // namespace openmsx

#endif // __SAI2XSCALER_HH__
