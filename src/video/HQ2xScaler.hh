// $Id$

#ifndef HQ2XSCALER_HH
#define HQ2XSCALER_HH

#include "Scaler.hh"

namespace openmsx {

/** Runs the hq2x scaler algorithm.
  */
template <class Pixel>
class HQ2xScaler: public Scaler<Pixel>
{
public:
	HQ2xScaler(SDL_PixelFormat* format);

	virtual void scale256(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale512(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
};

} // namespace openmsx

#endif
