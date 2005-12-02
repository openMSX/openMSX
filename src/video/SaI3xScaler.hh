// $Id$

#ifndef SAI3XSCALER_HH
#define SAI3XSCALER_HH

#include "Scaler3.hh"
#include "PixelOperations.hh"

namespace openmsx {

/** 2xSaI algorithm: edge-detection which produces a rounded look.
  * Algorithm was developed by Derek Liauw Kie Fa.
  */
template <typename Pixel>
class SaI3xScaler: public Scaler3<Pixel>
{
public:
	SaI3xScaler(SDL_PixelFormat* format);
	virtual void scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

private:
	inline Pixel blend(Pixel p1, Pixel p2);

	template <unsigned NX, unsigned NY>
	void scaleFixed(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	void scaleAny(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif
