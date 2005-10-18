// $Id$

#ifndef HQ3XSCALER_HH
#define HQ3XSCALER_HH

#include "Scaler.hh"

namespace openmsx {

/** Runs the hq3x scaler algorithm.
  */
template <class Pixel>
class HQ3xScaler: public Scaler<Pixel>
{
public:
	HQ3xScaler(SDL_PixelFormat* format);

	virtual void scaleBlank(Pixel color, SDL_Surface* dst,
	                        unsigned startY, unsigned endY, bool lower);
	virtual void scale256(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale512(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
};

} // namespace openmsx

#endif
