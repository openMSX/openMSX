// $Id$

#ifndef __HQ2XSCALER_HH__
#define __HQ2XSCALER_HH__

#include "Scaler.hh"
#include "openmsx.hh"

namespace openmsx {

/** Runs the hq2x scaler algorithm.
  */
template <class Pixel>
class HQ2xScaler: public Scaler<Pixel>
{
public:
	virtual void scale256(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY);
	virtual void scale512(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY);
};

} // namespace openmsx

#endif // __HQ2XSCALER_HH__
