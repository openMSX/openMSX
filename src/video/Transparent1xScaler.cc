// $Id$

#include "Transparent1xScaler.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "MemoryOps.hh"
#include "CliComm.hh"
#include "build-info.hh"
#include <cassert>

namespace openmsx {

template<typename Pixel>
Transparent1xScaler<Pixel>::Transparent1xScaler(
		const PixelOperations<Pixel>& pixelOps,
		CliComm& cliComm_)
	: LowScaler<Pixel>(pixelOps)
	, cliComm(cliComm_)
{
}

template<typename Pixel>
static inline Pixel blendColorKey(const PixelOperations<Pixel>& pixelOps,
                                  Pixel p1, Pixel p2, Pixel colorKey)
{
	if (p1 == colorKey) return p2;
	if (p2 == colorKey) return p1;
	return pixelOps.template blend<1, 1>(p1, p2);

}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scaleBlank1to1(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor();
	MemoryOps::MemSet<Pixel, MemoryOps::STREAMING> memset;
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; ++srcY, ++dstY) {
		Pixel color = src.getLinePtr<Pixel>(srcY)[0];
		if (color == transparent) continue;
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY);
		memset(dstLine0, dst.getWidth(), color);
	}
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scaleBlank2to1(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor();
	MemoryOps::MemSet<Pixel, MemoryOps::STREAMING> memset;
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 1) {
		Pixel color0 = src.getLinePtr<Pixel>(srcY + 0)[0];
		Pixel color1 = src.getLinePtr<Pixel>(srcY + 1)[0];
		Pixel color01 = blendColorKey(this->pixelOps,
		                              color0, color1, transparent);
		if (color01 == transparent) continue;
		Pixel* dstLine = dst.getLinePtrDirect<Pixel>(dstY);
		memset(dstLine, dst.getWidth(), color01);
	}
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale1x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; ++srcY, ++dstY) {
		const Pixel* srcLine = src.getLinePtr<Pixel>(srcY, srcWidth);
		Pixel* dstLine = dst.getLinePtrDirect<Pixel>(dstY);
		for (unsigned x = 0; x < srcWidth; ++x) {
			if (srcLine[x] != transparent) {
				dstLine[x] = srcLine[x];
			}
		}
	}
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale1x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 1) {
		const Pixel* srcLine0 = src.getLinePtr<Pixel>(srcY + 0, srcWidth);
		const Pixel* srcLine1 = src.getLinePtr<Pixel>(srcY + 1, srcWidth);
		Pixel* dstLine = dst.getLinePtrDirect<Pixel>(dstY);
		for (unsigned x = 0; x < srcWidth; ++x) {
			Pixel color01 = blendColorKey(this->pixelOps,
				srcLine0[x], srcLine1[x], transparent);
			if (color01 != transparent) {
				dstLine[x] = color01;
			}
		}
	}
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale2x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; ++srcY, ++dstY) {
		const Pixel* srcLine = src.getLinePtr<Pixel>(srcY, srcWidth);
		Pixel* dstLine = dst.getLinePtrDirect<Pixel>(dstY);
		for (unsigned x = 0; x < srcWidth; x += 2) {
			Pixel color01 = blendColorKey(this->pixelOps,
				srcLine[x + 0], srcLine[x + 1], transparent);
			if (color01 != transparent) {
				dstLine[x] = color01;
			}
		}
	}
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale2x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 1) {
		const Pixel* srcLine0 = src.getLinePtr<Pixel>(srcY + 0, srcWidth);
		const Pixel* srcLine1 = src.getLinePtr<Pixel>(srcY + 1, srcWidth);
		Pixel* dstLine = dst.getLinePtrDirect<Pixel>(dstY);
		for (unsigned x = 0; x < srcWidth; x += 2) {
			Pixel color01 = blendColorKey(this->pixelOps,
				srcLine0[x + 0], srcLine0[x + 1], transparent);
			Pixel color23 = blendColorKey(this->pixelOps,
				srcLine1[x + 0], srcLine1[x + 1], transparent);
			Pixel color0123 = blendColorKey(this->pixelOps,
				color01, color23, transparent);
			if (color0123 != transparent) {
				dstLine[x] = color0123;
			}
		}
	}
}


template<typename Pixel>
void Transparent1xScaler<Pixel>::printWarning()
{
	static bool alreadyPrinted = false;
	if (alreadyPrinted) return;
	alreadyPrinted = true;

	cliComm.printWarning(
		"Transparent1xScaler not yet implemented for gfx9000 modes");
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale2x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	LowScaler<Pixel>::scale2x1to3x1(src, srcStartY, srcEndY, srcWidth,
	                                dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale2x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	LowScaler<Pixel>::scale2x2to3x1(src, srcStartY, srcEndY, srcWidth,
	                                dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale4x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	LowScaler<Pixel>::scale4x1to3x1(src, srcStartY, srcEndY, srcWidth,
	                                dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale4x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	LowScaler<Pixel>::scale4x2to3x1(src, srcStartY, srcEndY, srcWidth,
	                                dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale8x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	LowScaler<Pixel>::scale8x1to3x1(src, srcStartY, srcEndY, srcWidth,
	                                dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale8x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	LowScaler<Pixel>::scale8x2to3x1(src, srcStartY, srcEndY, srcWidth,
	                                dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale4x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	LowScaler<Pixel>::scale4x1to1x1(src, srcStartY, srcEndY, srcWidth,
	                                dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent1xScaler<Pixel>::scale4x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	LowScaler<Pixel>::scale4x2to1x1(src, srcStartY, srcEndY, srcWidth,
	                                dst, dstStartY, dstEndY);
}


// Force template instantiation.
#if HAVE_16BPP
template class Transparent1xScaler<word>;
#endif
#if HAVE_32BPP
template class Transparent1xScaler<unsigned>;
#endif

} // namespace openmsx
