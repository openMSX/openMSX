// $Id$

#ifndef SIMPLE3XSCALER_HH
#define SIMPLE3XSCALER_HH

#include "Scaler3.hh"
#include "PixelOperations.hh"
#include "Scanline.hh"

namespace openmsx {

class RenderSettings;

template <class Pixel>
class Simple3xScaler : public Scaler3<Pixel>
{
public:
	Simple3xScaler(SDL_PixelFormat* format,
	               const RenderSettings& renderSettings);

	virtual void scaleBlank(
                Pixel color, OutputSurface& dst,
                unsigned startY, unsigned endY);

	virtual void scale2x1to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale1x1to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale4x1to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale2x1to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale8x1to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale4x1to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

private:
	template <typename ScaleOp>
	void doScale1(FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	              OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	              ScaleOp scale);
	PixelOperations<Pixel> pixelOps;
	Scanline<Pixel> scanline;
	const RenderSettings& settings;
};

} // namespace openmsx

#endif
