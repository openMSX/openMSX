// $Id$

#ifndef HQ3XSCALER_HH
#define HQ3XSCALER_HH

#include "Scaler3.hh"

namespace openmsx {

/** Runs the hq3x scaler algorithm.
  */
template <class Pixel>
class HQ3xScaler: public Scaler3<Pixel>
{
public:
	HQ3xScaler(SDL_PixelFormat* format);

	virtual void scale256(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY
		);
};

} // namespace openmsx

#endif
