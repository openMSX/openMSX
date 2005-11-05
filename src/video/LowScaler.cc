// $Id$

#include "LowScaler.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "DeinterlacedFrame.hh"
#include "HostCPU.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

template <typename Pixel>
LowScaler<Pixel>::LowScaler(SDL_PixelFormat* format)
	: pixelOps(format)
{
}

/*template <typename Pixel>
void LowScaler<Pixel>::averageHalve(const Pixel* pIn0, const Pixel* pIn1, Pixel* pOut)
{
	// TODO MMX/SSE optimizations
	// pure C++ version
	for (int i = 0; i < 320; ++i) {
		Pixel tmp0 = blend(pIn0[2 * i + 0], pIn0[2 * i + 1]);
		Pixel tmp1 = blend(pIn1[2 * i + 0], pIn1[2 * i + 1]);
		pOut[i] = blend(tmp0, tmp1);
	}
}*/

template <typename Pixel, typename ScaleOp>
static void doScale1(
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	while (dstStartY < dstEndY) {
		Pixel* dummy = 0;
		const Pixel* srcLine = src.getLinePtr(srcStartY++, dummy);
		Pixel* dstLine = dst.getLinePtr(dstStartY++, dummy);
		scale(srcLine, dstLine, 320);
	}
}

template <typename Pixel, typename ScaleOp>
static void doScale2(
	FrameSource& src0, FrameSource& src1, OutputSurface& dst,
	unsigned startY, unsigned endY, PixelOperations<Pixel> ops,
	ScaleOp scale)
{
	BlendLines<Pixel> blend(ops);
	for (unsigned y = startY; y < endY; ++y) {
		Pixel* dummy = 0;
		const Pixel* srcLine0 = src0.getLinePtr(y, dummy);
		const Pixel* srcLine1 = src1.getLinePtr(y, dummy);
		Pixel buf0[320], buf1[320];
		scale(srcLine0, buf0, 320);
		scale(srcLine1, buf1, 320);
		Pixel* dstLine = dst.getLinePtr(y, dummy);
		blend(buf0, buf1, dstLine, 320);
	}
}

template <typename Pixel, typename ScaleOp>
static void doScaleDV(
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	PixelOperations<Pixel> ops, ScaleOp scale)
{
	BlendLines<Pixel> blend(ops);
	while (dstStartY < dstEndY) {
		Pixel* dummy = 0;
		const Pixel* srcLine0 = src.getLinePtr(srcStartY++, dummy);
		const Pixel* srcLine1 = src.getLinePtr(srcStartY++, dummy);
		Pixel buf0[320], buf1[320];
		scale(srcLine0, buf0, 320);
		scale(srcLine1, buf1, 320);
		Pixel* dstLine = dst.getLinePtr(dstStartY++, dummy);
		blend(buf0, buf1, dstLine, 320);
	}
}


template <class Pixel>
void LowScaler<Pixel>::scale3x1to4x1(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale3x2to4x1(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_2on3<Pixel>(pixelOps));
}

template <typename Pixel>
void LowScaler<Pixel>::scale1x1to1x1(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_1on1<Pixel>());
}

template <typename Pixel>
void LowScaler<Pixel>::scale1x2to1x1(
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	// No need to scale to local buffer first, like doScaleDV does.
	// TODO: Even so, this scale mode is so rare, is it really worth having
	//       optimized code for?
	BlendLines<Pixel> blend(pixelOps);
	while (dstStartY < dstEndY) {
		Pixel* dummy = 0;
		const Pixel* srcLine0 = src.getLinePtr(srcStartY++, dummy);
		const Pixel* srcLine1 = src.getLinePtr(srcStartY++, dummy);
		Pixel* dstLine = dst.getLinePtr(dstStartY++, dummy);
		blend(srcLine0, srcLine1, dstLine, 320);
	}
}

template <class Pixel>
void LowScaler<Pixel>::scale3x1to2x1(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale3x2to2x1(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_4on3<Pixel>(pixelOps));
}

template <typename Pixel>
void LowScaler<Pixel>::scale2x1to1x1(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_2on1<Pixel>(pixelOps));
}

template <typename Pixel>
void LowScaler<Pixel>::scale2x2to1x1(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_2on1<Pixel>(pixelOps));
	// TODO: Profile specific code vs generic implementation.
}


template <class Pixel>
void LowScaler<Pixel>::scale5x1to2x1(
	FrameSource& /*src*/, unsigned /*srcStartY*/, unsigned /*srcEndY*/,
	OutputSurface& /*dst*/, unsigned /*dstStartY*/, unsigned /*dstEndY*/)
{
	// TODO
}

template <class Pixel>
void LowScaler<Pixel>::scale5x2to2x1(
	FrameSource& /*src*/, unsigned /*srcStartY*/, unsigned /*srcEndY*/,
	OutputSurface& /*dst*/, unsigned /*dstStartY*/, unsigned /*dstEndY*/)
{
	// TODO
}

template <class Pixel>
void LowScaler<Pixel>::scale3x1to1x1(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_8on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale3x2to1x1(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_8on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale4x1to1x1(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_4on1<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale4x2to1x1(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_4on1<Pixel>(pixelOps));
}

// TODO: This method doesn't have any dependency on the pixel format, so is it
//       possible to move it to a class without the Pixel template parameter?
template <class Pixel>
void LowScaler<Pixel>::scaleImage(
	FrameSource& src, unsigned lineWidth,
	unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (src.getHeight() == 240) {
		switch (lineWidth) {
		case 192:
			scale3x1to4x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 256:
			scale1x1to1x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 384:
			scale3x1to2x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 512:
			scale2x1to1x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 640:
			scale5x1to2x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 768:
			scale3x1to1x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 1024:
			scale4x1to1x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		default:
			assert(false);
			break;
		}
	} else {
		assert(src.getHeight() == 480);
		switch (lineWidth) {
		case 192:
			scale3x2to4x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 256:
			scale1x2to1x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 384:
			scale3x2to2x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 512:
			scale2x2to1x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 640:
			scale5x2to2x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 768:
			scale3x2to1x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 1024:
			scale4x2to1x1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		default:
			assert(false);
			break;
		}
	}
}

// Force template instantiation.
template class LowScaler<word>;
template class LowScaler<unsigned int>;

} // namespace openmsx
