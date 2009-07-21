// $Id$

#include "TransparentScaler.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "MemoryOps.hh"
#include "build-info.hh"

namespace openmsx {

// This scaler should be selected when superimposing. Any pixels in the
// source which match OutputSurface.getKeyColour() will not be copied.
// This is used by the Laserdisc MSX. The VDP image is superimposed on
// the Laserdisc output; applying the blurring/scanlines effects on
// these videos would look awful so these effects are ignored.
template <class Pixel>
TransparentScaler<Pixel>::TransparentScaler(
		const PixelOperations<Pixel>& pixelOps)
	: Scaler2<Pixel>(pixelOps)
{
}

template <class Pixel>
void TransparentScaler<Pixel>::scaleBlank1to2(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColour();

	unsigned srcY = srcStartY, dstY = dstStartY;
	MemoryOps::MemSet<Pixel, MemoryOps::STREAMING> memset;
	for (/**/; dstY < dstEndY; srcY += 1, dstY += 2) {
		Pixel color = src.getLinePtr<Pixel>(srcY)[0];
		if (color == transparent) continue;

		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		memset(dstLine0, dst.getWidth(), color);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		memset(dstLine1, dst.getWidth(), color);
	}
}

template <class Pixel>
static inline void scale1on2(
	const Pixel* pIn, Pixel* pOut, unsigned srcWidth, Pixel transparent)
{
	for (unsigned x = 0; x < srcWidth; ++x) {
		if (transparent != pIn[x]) {
			pOut[2 * x] = pOut[2 * x + 1] = pIn[x];
		}
	}
}

template <class Pixel>
static inline void scale1on1(
	const Pixel* pIn, Pixel* pOut, unsigned srcWidth, Pixel transparent)
{
	for (unsigned x = 0; x < srcWidth; ++x) {
		if (transparent != pIn[x]) {
			pOut[x] = pIn[x];
		}
	}
}

template <class Pixel>
void TransparentScaler<Pixel>::scale1x1to2x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColour();

	unsigned srcY = srcStartY, dstY = dstStartY;
	for (/**/; dstY < dstEndY; srcY += 1, dstY += 2) {
		const Pixel* srcLine = src.getLinePtr<Pixel>(srcY, srcWidth);
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		scale1on2(srcLine, dstLine0, srcWidth, transparent);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		scale1on2(srcLine, dstLine1, srcWidth, transparent);
	}
}

template <class Pixel>
void TransparentScaler<Pixel>::scale1x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColour();

	unsigned srcY = srcStartY, dstY = dstStartY;
	for (/**/; dstY < dstEndY; srcY += 1, dstY += 2) {
		const Pixel* srcLine = src.getLinePtr<Pixel>(srcY, srcWidth);
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		scale1on1(srcLine, dstLine0, srcWidth, transparent);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		scale1on1(srcLine, dstLine1, srcWidth, transparent);
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
