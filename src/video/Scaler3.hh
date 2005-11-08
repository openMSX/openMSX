// $Id$

#ifndef SCALER3_HH
#define SCALER3_HH

#include "Scaler.hh"
#include "PixelOperations.hh"

namespace openmsx {

/** Base class for 3x scalers
  */
template <class Pixel> class Scaler3 : public Scaler<Pixel>
{
public:
	virtual void scale2x1to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale2x2to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale1x1to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x2to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale4x1to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale4x2to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale2x1to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale2x2to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale8x1to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale8x2to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale4x1to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale4x2to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scaleImage(
		FrameSource& src, unsigned lineWidth,
		unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

protected:
	Scaler3(SDL_PixelFormat* format);

private:
	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif
