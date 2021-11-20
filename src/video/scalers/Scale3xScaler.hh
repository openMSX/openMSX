#ifndef SCALE3XSCALER_HH
#define SCALE3XSCALER_HH

#include "Scaler3.hh"

namespace openmsx {

/** Runs the Scale3x scaler algorithm.
  */
template<typename Pixel>
class Scale3xScaler final : public Scaler3<Pixel>
{
public:
	explicit Scale3xScaler(const PixelOperations<Pixel>& pixelOps);

	void scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;

private:
	void scaleLine1on3Half(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2,
		unsigned srcWidth) __restrict;
	void scaleLine1on3Mid (Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2,
		unsigned srcWidth) __restrict;
};

} // namespace openmsx

#endif
