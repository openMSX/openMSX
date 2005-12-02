// $Id$

#ifndef RGBTRIPLET3XSCALER_HH
#define RGBTRIPLET3XSCALER_HH

#include "Scaler3.hh"
#include "PixelOperations.hh"
#include "Scanline.hh"

namespace openmsx {

class RenderSettings;

/** TODO
  */
template <class Pixel>
class RGBTriplet3xScaler : public Scaler3<Pixel>
{
public:
	RGBTriplet3xScaler(SDL_PixelFormat* format,
	                   const RenderSettings& renderSettings);

	virtual void scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale2x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale4x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale2x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale8x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale4x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

private:
	void recalcBlur();
	inline void calcSpil(unsigned x, unsigned& r, unsigned& s);

	/**
	 * Calculates the RGB triplets.
	 * @param in Buffer of input pixels
	 * @param out Buffer of output pixels, should be 3x as long as input
	 * @param inwidth Width of the input buffer (in pixels)
	 */
	void rgbify(const Pixel* in, Pixel* out, unsigned inwidth);

	template <typename ScaleOp>
	void doScale1(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
		ScaleOp scale);

	int c1, c2;
	PixelOperations<Pixel> pixelOps;
	Scanline<Pixel> scanline;
	const RenderSettings& settings;
};

} // namespace openmsx

#endif
