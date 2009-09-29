// $Id$

#include "Transparent2xScaler.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "MemoryOps.hh"
#include "CliComm.hh"
#include "build-info.hh"

namespace openmsx {

// This scaler should be selected when superimposing. Any pixels in the
// source which match OutputSurface::getKeyColor() will not be copied.
// This is used by the Laserdisc MSX. The VDP image is superimposed on
// the Laserdisc output; applying the blurring/scanlines effects on
// these videos would look awful so these effects are ignored.
template<typename Pixel>
Transparent2xScaler<Pixel>::Transparent2xScaler(
		const PixelOperations<Pixel>& pixelOps,
		CliComm& cliComm_)
	: Scaler2<Pixel>(pixelOps)
	, cliComm(cliComm_)
{
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scaleBlank1to2(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
	MemoryOps::MemSet<Pixel, MemoryOps::STREAMING> memset;
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 2) {
		Pixel color = src.getLinePtr<Pixel>(srcY)[0];
		if (color == transparent) continue;
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		memset(dstLine0, dst.getWidth(), color);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		memset(dstLine1, dst.getWidth(), color);
	}
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scaleBlank1to1(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
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
void Transparent2xScaler<Pixel>::scale1x1to2x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 2) {
		const Pixel* srcLine = src.getLinePtr<Pixel>(srcY, srcWidth);
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		for (unsigned x = 0; x < srcWidth; ++x) {
			if (srcLine[x] != transparent) {
				dstLine0[2 * x + 0] =
				dstLine0[2 * x + 1] =
				dstLine1[2 * x + 0] =
				dstLine1[2 * x + 1] = srcLine[x];
			}
		}
	}
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scale1x1to2x1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; ++srcY, ++dstY) {
		const Pixel* srcLine = src.getLinePtr<Pixel>(srcY, srcWidth);
		Pixel* dstLine = dst.getLinePtrDirect<Pixel>(dstY);
		for (unsigned x = 0; x < srcWidth; ++x) {
			if (srcLine[x] != transparent) {
				dstLine[2 * x + 0] =
				dstLine[2 * x + 1] = srcLine[x];
			}
		}
	}
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scale1x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 2) {
		const Pixel* srcLine = src.getLinePtr<Pixel>(srcY, srcWidth);
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		for (unsigned x = 0; x < srcWidth; ++x) {
			if (srcLine[x] != transparent) {
				dstLine0[x] =
				dstLine1[x] = srcLine[x];
			}
		}
	}
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scale1x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
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
void Transparent2xScaler<Pixel>::printWarning()
{
	static bool alreadyPrinted = false;
	if (alreadyPrinted) return;
	alreadyPrinted = true;

	cliComm.printWarning(
		"Transparent2xScaler not yet implemented for gfx9000 modes");
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scale1x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler2<Pixel>::scale1x1to3x2(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scale1x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler2<Pixel>::scale1x1to3x1(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scale2x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler2<Pixel>::scale2x1to3x2(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scale2x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler2<Pixel>::scale2x1to3x1(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scale4x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler2<Pixel>::scale4x1to3x2(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scale4x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler2<Pixel>::scale4x1to3x1(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scale2x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler2<Pixel>::scale2x1to1x2(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent2xScaler<Pixel>::scale2x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler2<Pixel>::scale2x1to1x1(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}


// Force template instantiation.
#if HAVE_16BPP
template class Transparent2xScaler<word>;
#endif
#if HAVE_32BPP
template class Transparent2xScaler<unsigned>;
#endif

} // namespace openmsx
