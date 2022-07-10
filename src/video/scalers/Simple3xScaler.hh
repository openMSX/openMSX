#ifndef SIMPLE3XSCALER_HH
#define SIMPLE3XSCALER_HH

#include "Scaler3.hh"
#include "Multiply32.hh"
#include "PixelOperations.hh"
#include "Scanline.hh"

namespace openmsx {

class RenderSettings;
template<std::unsigned_integral Pixel> class PolyLineScaler;

template<std::unsigned_integral Pixel> class Blur_1on3
{
public:
	explicit Blur_1on3(const PixelOperations<Pixel>& pixelOps);
	inline void setBlur(unsigned blur_) { blur = blur_; }
	void operator()(const Pixel* in, Pixel* out, size_t dstWidth);
private:
	Multiply32<Pixel> mult0;
	Multiply32<Pixel> mult1;
	Multiply32<Pixel> mult2;
	Multiply32<Pixel> mult3;
	unsigned blur;
#ifdef __SSE2__
	void blur_SSE(const Pixel* in_, Pixel* out_, size_t srcWidth);
#endif
};

template<std::unsigned_integral Pixel>
class Simple3xScaler final : public Scaler3<Pixel>
{
public:
	Simple3xScaler(const PixelOperations<Pixel>& pixelOps,
	               const RenderSettings& settings);
	~Simple3xScaler() override;

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
		unsigned srcStartY, unsigned srcEndY, unsigned srcwidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
	void scale2x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcwidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY) override;
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

private:
	PixelOperations<Pixel> pixelOps;
	Scanline<Pixel> scanline;

	// in 16bpp calculation of LUTs can be expensive, so keep as member
	Blur_1on3<Pixel> blur_1on3;

	const RenderSettings& settings;
};

} // namespace openmsx

#endif
