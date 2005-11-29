// $Id$

#ifndef HQ2XLITESCALER_HH
#define HQ2XLITESCALER_HH

#include "Scaler2.hh"

namespace openmsx {

template <class Pixel>
class HQ2xLiteScaler: public Scaler2<Pixel>
{
public:
	HQ2xLiteScaler(SDL_PixelFormat* format);

	virtual void scale1x1to2x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth, 
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x1to1x2(FrameSource& src, 
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth, 
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
};

} // namespace openmsx

#endif
