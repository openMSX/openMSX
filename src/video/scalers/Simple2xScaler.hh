#ifndef SIMPLESCALER_HH
#define SIMPLESCALER_HH

#include "Scaler2.hh"
#include "Scanline.hh"
#include "Multiply32.hh"

namespace openmsx {

class RenderSettings;

/** Scaler which assigns the color of the original pixel to all pixels in
  * the 2x2 square. Optionally it can draw darkended scanlines (scanline has
  * the average color from the pixel above and below). It can also optionally
  * perform a horizontal blur.
  */
template<typename Pixel>
class Simple2xScaler final : public Scaler2<Pixel>
{
public:
	Simple2xScaler(
		const PixelOperations<Pixel>& pixelOps,
		RenderSettings& renderSettings);

private:
	void scaleImage(FrameSource& src, const RawFrame* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scaleBlank1to2(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale1x1to2x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale1x1to1x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;

	void drawScanline(const Pixel* in1, const Pixel* in2, Pixel* out,
	                  int factor, unsigned dstWidth);
	void blur1on2(const Pixel* pIn, Pixel* pOut, unsigned alpha,
	              size_t srcWidth);
	void blur1on1(const Pixel* pIn, Pixel* pOut, unsigned alpha,
	              size_t srcWidth);

private:
	RenderSettings& settings;
	PixelOperations<Pixel> pixelOps;

	Multiply32<Pixel> mult1;
	Multiply32<Pixel> mult2;
	Multiply32<Pixel> mult3;

	Scanline<Pixel> scanline;
};

} // namespace openmsx

#endif
