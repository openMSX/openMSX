// $Id$

#ifndef HQ2XLITESCALER_HH
#define HQ2XLITESCALER_HH

#include "Scaler.hh"

namespace openmsx {

template <class Pixel>
class HQ2xLiteScaler: public Scaler<Pixel>
{
public:
	virtual void scale256(SDL_Surface* src, int srcY, int endSrcY,
	                      SDL_Surface* dst, int dstY);
	virtual void scale512(SDL_Surface* src, int srcY, int endSrcY,
	                      SDL_Surface* dst, int dstY);
};

} // namespace openmsx

#endif
