// $Id$

#ifndef HQ3XLITESCALER_HH
#define HQ3XLITESCALER_HH

#include "Scaler3.hh"

namespace openmsx {

template <class Pixel>
class HQ3xLiteScaler: public Scaler3<Pixel>
{
public:
	HQ3xLiteScaler(const PixelOperations<Pixel>& pixelOps);

	virtual void scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
};

} // namespace openmsx

#endif
