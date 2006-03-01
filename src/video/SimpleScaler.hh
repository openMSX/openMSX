// $Id$

#ifndef SIMPLESCALER_HH
#define SIMPLESCALER_HH

#include "Scaler2.hh"
#include "Scanline.hh"
#include "Multiply32.hh"

namespace openmsx {

class RenderSettings;

/** Scaler which assigns the color of the original pixel to all pixels in
  * the 2x2 square. Optionally it can draw darkended scanlines (scanline has
  * the averga color from the pixel above and below). It can also optionally
  * perform a horizontal blur.
  */
template <class Pixel>
class SimpleScaler: public Scaler2<Pixel>
{
public:
	SimpleScaler(
		const PixelOperations<Pixel>& pixelOps,
		RenderSettings& renderSettings
		);

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
	void drawScanline(const Pixel* in1, const Pixel* in2, Pixel* out,
	                  int factor, unsigned dstWidth);
	void blur1on2(const Pixel* pIn, Pixel* pOut, unsigned alpha,
	              unsigned srcWidth);
	void blur1on1(const Pixel* pIn, Pixel* pOut, unsigned alpha,
	              unsigned srcWidth);

	RenderSettings& settings;

	Multiply32<Pixel> mult1;
	Multiply32<Pixel> mult2;
	Multiply32<Pixel> mult3;

	Scanline<Pixel> scanline;
};

} // namespace openmsx

#endif
