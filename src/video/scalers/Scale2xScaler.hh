#ifndef SCALE2XSCALER_HH
#define SCALE2XSCALER_HH

#include "Scaler2.hh"

namespace openmsx {

/** Runs the Scale2x scaler algorithm.
  */
template <class Pixel>
class Scale2xScaler : public Scaler2<Pixel>
{
public:
	explicit Scale2xScaler(const PixelOperations<Pixel>& pixelOps);

	virtual void scale1x1to2x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x1to1x2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);

private:
	void scaleLine_1on2(Pixel* dst0, Pixel* dst1,
		const Pixel* src0, const Pixel* src1, const Pixel* src2,
		unsigned long srcWidth) __restrict;
	void scaleLineHalf_1on2(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2,
		unsigned long srcWidth) __restrict;

	void scaleLine_1on1(Pixel* dst0, Pixel* dst1,
		const Pixel* src0, const Pixel* src1, const Pixel* src2,
		unsigned long srcWidth) __restrict;
	void scaleLineHalf_1on1(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2,
		unsigned long srcWidth) __restrict;
};

} // namespace openmsx

#endif
