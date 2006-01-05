// $Id$

#ifndef SCALE3XSCALER_HH
#define SCALE3XSCALER_HH

#include "Scaler3.hh"

namespace openmsx {

/** Runs the Scale3x scaler algorithm.
  */
template <class Pixel>
class Scale3xScaler: public Scaler3<Pixel>
{
public:
	Scale3xScaler(const PixelOperations<Pixel>& pixelOps);

	virtual void scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

private:
	void scaleLine256Half(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2);
	void scaleLine256Mid(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2);
	void scaleLine512Half(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2);
};

} // namespace openmsx

#endif
