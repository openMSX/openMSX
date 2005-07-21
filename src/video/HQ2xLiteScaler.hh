// $Id$

#ifndef HQ2XLITESCALER_HH
#define HQ2XLITESCALER_HH

#include "Scaler.hh"

namespace openmsx {

template <class Pixel>
class HQ2xLiteScaler: public Scaler<Pixel>
{
public:
	virtual void scale256(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale512(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
};

} // namespace openmsx

#endif
