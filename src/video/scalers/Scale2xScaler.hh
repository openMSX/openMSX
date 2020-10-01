#ifndef SCALE2XSCALER_HH
#define SCALE2XSCALER_HH

#include "Scaler2.hh"
#include <cstddef>

namespace openmsx {

/** Runs the Scale2x scaler algorithm.
  */
template<typename Pixel>
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
	void scaleLine_1on2(Pixel* dst0, Pixel* dst1,
		const Pixel* src0, const Pixel* src1, const Pixel* src2,
		size_t srcWidth) __restrict;
	void scaleLineHalf_1on2(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2,
		size_t srcWidth) __restrict;

	void scaleLine_1on1(Pixel* dst0, Pixel* dst1,
		const Pixel* src0, const Pixel* src1, const Pixel* src2,
		size_t srcWidth) __restrict;
	void scaleLineHalf_1on1(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2,
		size_t srcWidth) __restrict;
};

} // namespace openmsx

#endif
