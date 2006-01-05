// $Id$

#ifndef SCALE2XSCALER_HH
#define SCALE2XSCALER_HH

#include "Scaler2.hh"

namespace openmsx {

/** Runs the Scale2x scaler algorithm.
  */
template <class Pixel>
class Scale2xScaler: public Scaler2<Pixel>
{
public:
	Scale2xScaler(const PixelOperations<Pixel>& pixelOps);

	virtual void scale1x1to2x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x1to1x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

private:
	void scaleLine256Half(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2);
	void scaleLine512Half(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2);
};

} // namespace openmsx

#endif
