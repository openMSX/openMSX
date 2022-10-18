#ifndef SCALE3XSCALER_HH
#define SCALE3XSCALER_HH

#include "Scaler3.hh"
#include <span>

namespace openmsx {

/** Runs the Scale3x scaler algorithm.
  */
template<std::unsigned_integral Pixel>
class Scale3xScaler final : public Scaler3<Pixel>
{
public:
	explicit Scale3xScaler(const PixelOperations<Pixel>& pixelOps);

	void scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;

private:
	void scaleLine1on3Half(std::span<Pixel> dst,
		std::span<const Pixel> src0, std::span<const Pixel> src1, std::span<const Pixel> src2);
	void scaleLine1on3Mid (std::span<Pixel> dst,
		std::span<const Pixel> src0, std::span<const Pixel> src1, std::span<const Pixel> src2);
};

} // namespace openmsx

#endif
