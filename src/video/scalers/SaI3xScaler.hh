#ifndef SAI3XSCALER_HH
#define SAI3XSCALER_HH

#include "Scaler3.hh"
#include "PixelOperations.hh"

namespace openmsx {

/** 2xSaI algorithm: edge-detection which produces a rounded look.
  * Algorithm was developed by Derek Liauw Kie Fa.
  */
template<std::unsigned_integral Pixel>
class SaI3xScaler final : public Scaler3<Pixel>
{
public:
	explicit SaI3xScaler(const PixelOperations<Pixel>& pixelOps);
	void scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;

private:
	[[nodiscard]] inline Pixel blend(Pixel p1, Pixel p2) const;

	template<unsigned NX, unsigned NY>
	void scaleFixed(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	void scaleAny(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
		__restrict;

	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif
