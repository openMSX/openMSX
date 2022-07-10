#ifndef SCALER3_HH
#define SCALER3_HH

#include "Scaler.hh"
#include "PixelOperations.hh"

namespace openmsx {

/** Base class for 3x scalers
  */
template<std::unsigned_integral Pixel> class Scaler3 : public Scaler<Pixel>
{
public:
	void scaleImage(FrameSource& src, const RawFrame* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;

protected:
	explicit Scaler3(const PixelOperations<Pixel>& pixelOps);

	void dispatchScale(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scaleBlank2to3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale2x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale2x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale4x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale4x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale2x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale2x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale8x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale8x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale4x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale4x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY);

protected:
	const PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif
