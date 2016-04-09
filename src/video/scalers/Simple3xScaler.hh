#ifndef SIMPLE3XSCALER_HH
#define SIMPLE3XSCALER_HH

#include "Scaler3.hh"
#include "PixelOperations.hh"
#include "Scanline.hh"
#include <memory>

namespace openmsx {

class RenderSettings;
template <class Pixel> class Blur_1on3;
template <class Pixel> class PolyLineScaler;

template <class Pixel>
class Simple3xScaler final : public Scaler3<Pixel>
{
public:
	Simple3xScaler(const PixelOperations<Pixel>& pixelOps,
	               const RenderSettings& renderSettings);
	~Simple3xScaler();

private:
	void scaleImage(FrameSource& src, const RawFrame* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scaleBlank2to3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale2x1to9x3(FrameSource& src,
		unsigned srcstarty, unsigned srcendy, unsigned srcwidth,
		ScalerOutput<Pixel>& dst, unsigned dststarty, unsigned dstendy) override;
	void scale2x2to9x3(FrameSource& src,
		unsigned srcstarty, unsigned srcendy, unsigned srcwidth,
		ScalerOutput<Pixel>& dst, unsigned dststarty, unsigned dstendy) override;
	void scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale1x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale4x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale4x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale2x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale2x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale8x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale8x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale4x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale4x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;

	void doScale1(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
		PolyLineScaler<Pixel>& scale);
	void doScale2(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
		PolyLineScaler<Pixel>& scale);

	PixelOperations<Pixel> pixelOps;
	Scanline<Pixel> scanline;

	// in 16bpp calculation of LUTs can be expensive, so keep as member
	std::unique_ptr<Blur_1on3<Pixel>> blur_1on3;

	const RenderSettings& settings;
};

} // namespace openmsx

#endif
