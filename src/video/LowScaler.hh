// $Id$

#ifndef LOWSCALER_HH
#define LOWSCALER_HH

#include "Scaler.hh"
#include "PixelOperations.hh"

namespace openmsx {

template <typename Pixel>
class LowScaler : public Scaler<Pixel>
{
public:
	LowScaler(SDL_PixelFormat* format);

	virtual void scale3x1to4x1(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale192(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
	                      unsigned startY, unsigned endY);
	virtual void scale1x1to1x1(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale256(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
	                      unsigned startY, unsigned endY);
	virtual void scale3x1to2x1(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale384(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
	                      unsigned startY, unsigned endY);
	virtual void scale2x1to1x1(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale512(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
	                      unsigned startY, unsigned endY);
	virtual void scale5x1to2x1(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale640(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
	                      unsigned startY, unsigned endY);
	virtual void scale3x1to1x1(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale768(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
	                      unsigned startY, unsigned endY);
	virtual void scale4x1to1x1(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1024(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
	                       unsigned startY, unsigned endY);

	virtual void scaleImage(
		FrameSource& src, unsigned lineWidth,
		unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scaleImage(
		FrameSource& frameEven, FrameSource& frameOdd, unsigned lineWidth,
		OutputSurface& dst, unsigned srcStartY, unsigned srcEndY);

private:
	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif
