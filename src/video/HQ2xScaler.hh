// $Id$

#ifndef HQ2XSCALER_HH
#define HQ2XSCALER_HH

#include "Scaler2.hh"

namespace openmsx {

/** Runs the hq2x scaler algorithm.
  */
template <class Pixel>
class HQ2xScaler: public Scaler2<Pixel>
{
public:
	HQ2xScaler(const PixelOperations<Pixel>& pixelOps);

	virtual void scale1x1to2x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x1to1x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
};

} // namespace openmsx

#endif
