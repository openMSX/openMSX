// $Id$

#ifndef TRANSPARENTSCALER_HH
#define TRANSPARENTSCALER_HH

#include "Scaler2.hh"
#include "Scanline.hh"
#include "Multiply32.hh"

namespace openmsx {

class RenderSettings;

template <class Pixel>
class TransparentScaler : public Scaler2<Pixel>
{
public:
	TransparentScaler(
		const PixelOperations<Pixel>& pixelOps,
		RenderSettings& renderSettings);

	void setTransparent(Pixel transparent_) {
		transparent = transparent_;
	}

	virtual void scaleBlank1to2(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x1to2x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x1to1x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

private:
	void blur1on2(const Pixel* pIn, Pixel* pOut, unsigned alpha,
	              unsigned long srcWidth);
	void blur1on1(const Pixel* pIn, Pixel* pOut, unsigned alpha,
	              unsigned long srcWidth);

	RenderSettings& settings;

	Multiply32<Pixel> mult1;
	Multiply32<Pixel> mult2;
	Multiply32<Pixel> mult3;

	Pixel transparent;
};

} // namespace openmsx

#endif
