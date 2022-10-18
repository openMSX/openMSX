#ifndef SCALE2XSCALER_HH
#define SCALE2XSCALER_HH

#include "Scaler2.hh"
#include <cstddef>
#include <span>

namespace openmsx {

/** Runs the Scale2x scaler algorithm.
  */
template<std::unsigned_integral Pixel>
class Scale2xScaler final : public Scaler2<Pixel>
{
public:
	explicit Scale2xScaler(const PixelOperations<Pixel>& pixelOps);

	void scale1x1to2x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale1x1to1x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;

private:
	void scaleLine_1on2(std::span<Pixel> dst0, std::span<Pixel> dst1,
		std::span<const Pixel> src0, std::span<const Pixel> src1, std::span<const Pixel> src2);
	void scaleLineHalf_1on2(std::span<Pixel> dst,
		std::span<const Pixel> src0, std::span<const Pixel> src1, std::span<const Pixel> src2);

	void scaleLine_1on1(std::span<Pixel> dst0, std::span<Pixel> dst1,
		std::span<const Pixel> src0, std::span<const Pixel> src1, std::span<const Pixel> src2);
	void scaleLineHalf_1on1(std::span<Pixel> dst,
		std::span<const Pixel> src0, std::span<const Pixel> src1, std::span<const Pixel> src2);
};

} // namespace openmsx

#endif
