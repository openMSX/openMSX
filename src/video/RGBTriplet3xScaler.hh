// $Id$

#ifndef RGBTRIPLET3XSCALER_HH
#define RGBTRIPLET3XSCALER_HH

#include "Scaler3.hh"

namespace openmsx {

/** Runs a simple 3x scaler algorithm.
  */
template <class Pixel>
class RGBTriplet3xScaler: public Scaler3<Pixel>
{
public:
	RGBTriplet3xScaler(SDL_PixelFormat* format);

	virtual void scale256(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY
		);
};

} // namespace openmsx

#endif
