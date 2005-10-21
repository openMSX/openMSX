// $Id$

#ifndef HQ3XLITESCALER_HH
#define HQ3XLITESCALER_HH

#include "Scaler3.hh"

namespace openmsx {

template <class Pixel>
class HQ3xLiteScaler: public Scaler3<Pixel>
{
public:
	HQ3xLiteScaler(SDL_PixelFormat* format);

	virtual void scale256(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
};

} // namespace openmsx

#endif
