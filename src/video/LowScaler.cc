// $Id$

#include "LowScaler.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
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


template <class Pixel>
void LowScaler<Pixel>::scale192(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale192(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY, pixelOps,
	                Scale_2on3<Pixel>(pixelOps));
}

template <typename Pixel>
void LowScaler<Pixel>::scale256(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_1on1<Pixel>());
}

template <typename Pixel>
void LowScaler<Pixel>::scale256(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
                                unsigned startY, unsigned endY)
{
	// no need to scale to local buffer first
	BlendLines<Pixel> blend(pixelOps);
	for (unsigned y = startY; y < endY; ++y) {
		Pixel* dummy = 0;
		const Pixel* srcLine0 = src0.getLinePtr(y, dummy);
		const Pixel* srcLine1 = src1.getLinePtr(y, dummy);
		Pixel* dstLine = dst.getLinePtr(y, dummy);
		blend(srcLine0, srcLine1, dstLine, 320);
	}
}

template <class Pixel>
void LowScaler<Pixel>::scale384(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale384(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY, pixelOps,
	                Scale_4on3<Pixel>(pixelOps));
}

template <typename Pixel>
void LowScaler<Pixel>::scale512(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_2on1<Pixel>(pixelOps));
}

template <typename Pixel>
void LowScaler<Pixel>::scale512(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
                                unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY, pixelOps,
	                Scale_2on1<Pixel>(pixelOps));
	/* TODO profile this vs generic implementation
	for (unsigned y = startY; y < endY; ++y) {
		Pixel* dummy = 0;
		const Pixel* srcLine0 = src0.getLinePtr(y, dummy);
		const Pixel* srcLine1 = src1.getLinePtr(y, dummy);
		Pixel* dstLine = dst.getLinePtr(y, dummy);
		averageHalve(srcLine0, srcLine1, dstLine);
	}*/
}


template <class Pixel>
void LowScaler<Pixel>::scale640(
	FrameSource& /*src*/, unsigned /*srcStartY*/, unsigned /*srcEndY*/,
	OutputSurface& /*dst*/, unsigned /*dstStartY*/, unsigned /*dstEndY*/)
{
	// TODO
}

template <class Pixel>
void LowScaler<Pixel>::scale640(FrameSource& /*src0*/, FrameSource& /*src1*/, OutputSurface& /*dst*/,
                             unsigned /*startY*/, unsigned /*endY*/)
{
	// TODO
}

template <class Pixel>
void LowScaler<Pixel>::scale768(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_8on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale768(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY, pixelOps,
	                Scale_8on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale1024(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_4on1<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale1024(FrameSource& src0, FrameSource& src1, OutputSurface& dst,
                              unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY, pixelOps,
	                Scale_4on1<Pixel>(pixelOps));
}

// TODO: This method doesn't have any dependency on the pixel format, so is it
//       possible to move it to a class without the Pixel template parameter?
template <class Pixel>
void LowScaler<Pixel>::scaleImage(
	FrameSource& src, unsigned lineWidth,
	unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	switch (lineWidth) {
	case 192:
		scale192(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
		break;
	case 256:
		scale256(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
		break;
	case 384:
		scale384(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
		break;
	case 512:
		scale512(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
		break;
	case 640:
		scale640(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
		break;
	case 768:
		scale768(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
		break;
	case 1024:
		scale1024(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
		break;
	default:
		assert(false);
		break;
	}
}

template <class Pixel>
void LowScaler<Pixel>::scaleImage(
	FrameSource& frameEven, FrameSource& frameOdd, unsigned lineWidth,
	OutputSurface& dst, unsigned srcStartY, unsigned srcEndY)
{
	switch (lineWidth) {
	case 192:
		scale192(frameEven, frameOdd, dst, srcStartY, srcEndY);
		break;
	case 256:
		scale256(frameEven, frameOdd, dst, srcStartY, srcEndY);
		break;
	case 384:
		scale384(frameEven, frameOdd, dst, srcStartY, srcEndY);
		break;
	case 512:
		scale512(frameEven, frameOdd, dst, srcStartY, srcEndY);
		break;
	case 640:
		scale640(frameEven, frameOdd, dst, srcStartY, srcEndY);
		break;
	case 768:
		scale768(frameEven, frameOdd, dst, srcStartY, srcEndY);
		break;
	case 1024:
		scale1024(frameEven, frameOdd, dst, srcStartY, srcEndY);
		break;
	default:
		assert(false);
		break;
	}
}

// Force template instantiation.
template class LowScaler<word>;
template class LowScaler<unsigned int>;

} // namespace openmsx
