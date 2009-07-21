// $Id$

#ifndef TRANSPARENTSCALER_HH
#define TRANSPARENTSCALER_HH

#include "Scaler2.hh"

namespace openmsx {

template <class Pixel>
class TransparentScaler : public Scaler2<Pixel>
{
public:
	TransparentScaler(const PixelOperations<Pixel>& pixelOps);

	virtual void scaleBlank1to2(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x1to2x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x1to1x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
};

} // namespace openmsx

#endif
