#ifndef SAI2XSCALER_HH
#define SAI2XSCALER_HH

#include "Scaler2.hh"
#include "PixelOperations.hh"

namespace openmsx {

/** 2xSaI algorithm: edge-detection which produces a rounded look.
  * Algorithm was developed by Derek Liauw Kie Fa.
  */
template<typename Pixel>
class SaI2xScaler final : public Scaler2<Pixel>
{
public:
	explicit SaI2xScaler(const PixelOperations<Pixel>& pixelOps);
	void scaleBlank1to2(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale1x1to2x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale1x1to1x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;

private:
	void scaleLine1on2(
		const Pixel* srcLine0, const Pixel* srcLine1,
		const Pixel* srcLine2, const Pixel* srcLine3,
		Pixel* dstUpper, Pixel* dstLower, unsigned srcWidth);
	void scaleLine1on1(
		const Pixel* srcLine0, const Pixel* srcLine1,
		const Pixel* srcLine2, const Pixel* srcLine3,
		Pixel* dstUpper, Pixel* dstLower, unsigned srcWidth);

	[[nodiscard]] inline Pixel blend(Pixel p1, Pixel p2) const;

private:
	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif
