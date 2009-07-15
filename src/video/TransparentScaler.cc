// $Id$

#include "TransparentScaler.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "RenderSettings.hh"
#include "MemoryOps.hh"
#include "HostCPU.hh"
#include "openmsx.hh"
#include "vla.hh"
#include "build-info.hh"
#include <cassert>

namespace openmsx {

// class TransparentScaler

// This scaler should be selected when superimposing. Any pixels in the
// source which match OutputSurface.getKeyColour() will not be copied.
// This is used by the Laserdisc MSX. The VDP image is superimposed on
// the Laserdisc output; applying the blurring/scanlines effects on
// these videos would look awful so these effects are ignored.
template <class Pixel>
TransparentScaler<Pixel>::TransparentScaler(
		const PixelOperations<Pixel>& pixelOps,
		RenderSettings& renderSettings)
	: Scaler2<Pixel>(pixelOps)
	, settings(renderSettings)
	, mult1(pixelOps.format)
	, mult2(pixelOps.format)
	, mult3(pixelOps.format)
{
}

template <class Pixel>
void TransparentScaler<Pixel>::scaleBlank1to2(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	transparent = dst.getKeyColour();

	unsigned stopDstY = (dstEndY == dst.getHeight())
	                  ? dstEndY : dstEndY - 2;
	unsigned srcY = srcStartY, dstY = dstStartY;
	MemoryOps::MemSet<Pixel, MemoryOps::STREAMING> memset;
	for (/* */; dstY < stopDstY; srcY += 1, dstY += 2) {
		Pixel color0 = src.getLinePtr<Pixel>(srcY)[0];
		if (color0 == transparent)
			continue;

		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		memset(dstLine0, dst.getWidth(), color0);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		memset(dstLine1, dst.getWidth(), color0);
	}
	if (dstY != dst.getHeight()) {
		unsigned nextLineWidth = src.getLineWidth(srcY + 1);
		assert(src.getLineWidth(srcY) == 1);
		assert(nextLineWidth != 1);
		this->scaleImage(src, srcY, srcEndY, nextLineWidth,
		                 dst, dstY, dstEndY);
	}
}

template <class Pixel>
void TransparentScaler<Pixel>::blur1on2(const Pixel* pIn, Pixel* pOut, unsigned /*alpha*/,
                                   unsigned long srcWidth)
{
	for (unsigned x = 0; x < srcWidth; x++) {
		if (transparent != pIn[x])
			pOut[x*2] = pOut[x*2+1] = pIn[x];
	}
}

template <class Pixel>
void TransparentScaler<Pixel>::blur1on1(const Pixel* pIn, Pixel* pOut, unsigned /*alpha*/,
                                   unsigned long srcWidth)
{
	for (unsigned x = 0; x < srcWidth; x++) {
		if (transparent != pIn[x])
			pOut[x] = pIn[x];
	}
}

template <class Pixel>
void TransparentScaler<Pixel>::scale1x1to2x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	transparent = dst.getKeyColour();
	int blur = settings.getBlurFactor();

	unsigned dstY = dstStartY;
	const Pixel* srcLine = src.getLinePtr<Pixel>(srcStartY++, srcWidth);
	Pixel* prevDstLine0 = dst.getLinePtrDirect<Pixel>(dstY++);
	blur1on2(srcLine, prevDstLine0, blur, srcWidth);

	while (dstY < dstEndY - 1) {
		srcLine = src.getLinePtr<Pixel>(srcStartY++, srcWidth);
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		blur1on2(srcLine, dstLine0, blur, srcWidth);

		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY);
		blur1on2(srcLine, dstLine1, blur, srcWidth);

		prevDstLine0 = dstLine0;
		dstY += 2;
	}
}

template <class Pixel>
void TransparentScaler<Pixel>::scale1x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	transparent = dst.getKeyColour();
	int blur = settings.getBlurFactor();

	unsigned dstY = dstStartY;
	const Pixel* srcLine = src.getLinePtr<Pixel>(srcStartY++, srcWidth);
	Pixel* prevDstLine0 = dst.getLinePtrDirect<Pixel>(dstY++);
	blur1on1(srcLine, prevDstLine0, blur, srcWidth);

	while (dstY < dstEndY - 1) {
		srcLine = src.getLinePtr<Pixel>(srcStartY++, srcWidth);
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		blur1on1(srcLine, dstLine0, blur, srcWidth);

		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		blur1on1(srcLine, dstLine1, blur, srcWidth);

		prevDstLine0 = dstLine0;
		dstY += 2;
	}
}

// Force template instantiation.
#if HAVE_16BPP
template class TransparentScaler<word>;
#endif
#if HAVE_32BPP
template class TransparentScaler<unsigned>;
#endif

} // namespace openmsx
