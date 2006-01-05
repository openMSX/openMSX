// $Id$

#include "LowScaler.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "MemoryOps.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

template <typename Pixel>
LowScaler<Pixel>::LowScaler(const PixelOperations<Pixel>& pixelOps_)
	: pixelOps(pixelOps_)
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

template <class Pixel>
void LowScaler<Pixel>::scaleBlank1to1(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 1) {
		Pixel* dummy = 0;
		Pixel color = src.getLinePtr(srcY, dummy)[0];
		Pixel* dstLine = dst.getLinePtr(dstY, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine, 320, color);
	}
}

template <class Pixel>
void LowScaler<Pixel>::scaleBlank2to1(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 1) {
		Pixel* dummy = 0;
		Pixel color0 = src.getLinePtr(srcY + 0, dummy)[0];
		Pixel color1 = src.getLinePtr(srcY + 1, dummy)[0];
		Pixel color01 = pixelOps.template blend<1, 1>(color0, color1);
		Pixel* dstLine = dst.getLinePtr(dstY, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine, 320, color01);
	}
}

template <typename Pixel, typename ScaleOp>
static void doScale1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; ++srcY, ++dstY) {
		Pixel* dummy = 0;
		const Pixel* srcLine = src.getLinePtr(srcY, srcWidth, dummy);
		Pixel* dstLine = dst.getLinePtr(dstY, dummy);
		scale(srcLine, dstLine, 320);
	}
}

template <typename Pixel, typename ScaleOp>
static void doScaleDV(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	PixelOperations<Pixel> ops, ScaleOp scale)
{
	BlendLines<Pixel> blend(ops);
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 1) {
		Pixel* dummy = 0;
		const Pixel* srcLine0 = src.getLinePtr(srcY + 0, srcWidth, dummy);
		const Pixel* srcLine1 = src.getLinePtr(srcY + 1, srcWidth, dummy);
		Pixel buf0[320], buf1[320];
		scale(srcLine0, buf0, 320);
		scale(srcLine1, buf1, 320);
		Pixel* dstLine = dst.getLinePtr(dstY, dummy);
		blend(buf0, buf1, dstLine, 320);
	}
}


template <class Pixel>
void LowScaler<Pixel>::scale2x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale2x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_2on3<Pixel>(pixelOps));
}

template <typename Pixel>
void LowScaler<Pixel>::scale1x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_1on1<Pixel>());
}

template <typename Pixel>
void LowScaler<Pixel>::scale1x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	// No need to scale to local buffer first, like doScaleDV does.
	// TODO: Even so, this scale mode is so rare, is it really worth having
	//       optimized code for?
	BlendLines<Pixel> blend(pixelOps);
	while (dstStartY < dstEndY) {
		Pixel* dummy = 0;
		const Pixel* srcLine0 = src.getLinePtr(srcStartY++, srcWidth, dummy);
		const Pixel* srcLine1 = src.getLinePtr(srcStartY++, srcWidth, dummy);
		Pixel* dstLine = dst.getLinePtr(dstStartY++, dummy);
		blend(srcLine0, srcLine1, dstLine, 320);
	}
}

template <class Pixel>
void LowScaler<Pixel>::scale4x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale4x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_4on3<Pixel>(pixelOps));
}

template <typename Pixel>
void LowScaler<Pixel>::scale2x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_2on1<Pixel>(pixelOps));
}

template <typename Pixel>
void LowScaler<Pixel>::scale2x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_2on1<Pixel>(pixelOps));
	// TODO: Profile specific code vs generic implementation.
}

template <class Pixel>
void LowScaler<Pixel>::scale8x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_8on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale8x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_8on3<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale4x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_4on1<Pixel>(pixelOps));
}

template <class Pixel>
void LowScaler<Pixel>::scale4x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_4on1<Pixel>(pixelOps));
}

// TODO: This method doesn't have any dependency on the pixel format, so is it
//       possible to move it to a class without the Pixel template parameter?
template <class Pixel>
void LowScaler<Pixel>::scaleImage(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (src.getHeight() == 240) {
		switch (srcWidth) {
		case 1:
			scaleBlank1to1(src, srcStartY, srcEndY,
			               dst, dstStartY, dstEndY);
			break;
		case 213:
			scale2x1to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 320:
			scale1x1to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 426:
			scale4x1to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 640:
			scale2x1to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 853:
			scale8x1to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 1280:
			scale4x1to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		default:
			assert(false);
			break;
		}
	} else {
		assert(src.getHeight() == 480);
		switch (srcWidth) {
		case 1:
			scaleBlank2to1(src, srcStartY, srcEndY,
			               dst, dstStartY, dstEndY);
			break;
		case 213:
			scale2x2to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 320:
			scale1x2to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 426:
			scale4x2to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 640:
			scale2x2to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 853:
			scale8x2to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 1280:
			scale4x2to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
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
