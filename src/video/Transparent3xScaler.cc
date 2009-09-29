// $Id$

#include "Transparent3xScaler.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "MemoryOps.hh"
#include "CliComm.hh"
#include "openmsx.hh"
#include "build-info.hh"

namespace openmsx {

template<typename Pixel>
Transparent3xScaler<Pixel>::Transparent3xScaler(
		const PixelOperations<Pixel>& pixelOps,
		CliComm& cliComm_)
	: Scaler3<Pixel>(pixelOps)
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
void Transparent3xScaler<Pixel>::scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
	MemoryOps::MemSet<Pixel, MemoryOps::STREAMING> memset;
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 3) {
		Pixel color = src.getLinePtr<Pixel>(srcY)[0];
		if (color == transparent) continue;
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		memset(dstLine0, dst.getWidth(), color);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		memset(dstLine1, dst.getWidth(), color);
		Pixel* dstLine2 = dst.getLinePtrDirect<Pixel>(dstY + 2);
		memset(dstLine2, dst.getWidth(), color);
	}
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scaleBlank2to3(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
	MemoryOps::MemSet<Pixel, MemoryOps::STREAMING> memset;
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 3) {
		Pixel color0 = src.getLinePtr<Pixel>(srcY + 0)[0];
		Pixel color1 = src.getLinePtr<Pixel>(srcY + 1)[0];
		Pixel color01 = blendColorKey(this->pixelOps,
		                              color0, color1, transparent);
		if (color0 != transparent) {
			Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
			memset(dstLine0, dst.getWidth(), color0);
		}
		if (color01 != transparent) {
			Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
			memset(dstLine1, dst.getWidth(), color01);
		}
		if (color1 != transparent) {
			Pixel* dstLine2 = dst.getLinePtrDirect<Pixel>(dstY + 2);
			memset(dstLine2, dst.getWidth(), color1);
		}
	}
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 3) {
		const Pixel* srcLine = src.getLinePtr<Pixel>(srcY, srcWidth);
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		Pixel* dstLine2 = dst.getLinePtrDirect<Pixel>(dstY + 2);
		for (unsigned x = 0; x < srcWidth; ++x) {
			if (srcLine[x] != transparent) {
				dstLine0[3 * x + 0] =
				dstLine0[3 * x + 1] =
				dstLine0[3 * x + 2] =
				dstLine1[3 * x + 0] =
				dstLine1[3 * x + 1] =
				dstLine1[3 * x + 2] =
				dstLine2[3 * x + 0] =
				dstLine2[3 * x + 1] =
				dstLine2[3 * x + 2] = srcLine[x];
			}
		}
	}
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale1x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 3) {
		const Pixel* srcLine0 = src.getLinePtr<Pixel>(srcY + 0, srcWidth);
		const Pixel* srcLine1 = src.getLinePtr<Pixel>(srcY + 1, srcWidth);
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		Pixel* dstLine2 = dst.getLinePtrDirect<Pixel>(dstY + 2);
		for (unsigned x = 0; x < srcWidth; ++x) {
			Pixel color0 = srcLine0[x];
			Pixel color1 = srcLine1[x];
			Pixel color01 = blendColorKey(this->pixelOps,
			                              color0, color1, transparent);
			if (color0 != transparent) {
				dstLine0[3 * x + 0] =
				dstLine0[3 * x + 1] =
				dstLine0[3 * x + 2] = color0;
			}
			if (color01 != transparent) {
				dstLine1[3 * x + 0] =
				dstLine1[3 * x + 1] =
				dstLine1[3 * x + 2] = color01;
			}
			if (color1 != transparent) {
				dstLine2[3 * x + 0] =
				dstLine2[3 * x + 1] =
				dstLine2[3 * x + 2] = color1;
			}
		}
	}
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale2x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 3) {
		const Pixel* srcLine = src.getLinePtr<Pixel>(srcY, srcWidth);
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		Pixel* dstLine2 = dst.getLinePtrDirect<Pixel>(dstY + 2);
		for (unsigned x = 0; x < srcWidth / 2; ++x) {
			Pixel color0 = srcLine[2 * x + 0];
			Pixel color1 = srcLine[2 * x + 1];
			Pixel color01 = blendColorKey(this->pixelOps,
			                              color0, color1, transparent);
			if (color0 != transparent) {
				dstLine0[3 * x + 0] =
				dstLine1[3 * x + 0] =
				dstLine2[3 * x + 0] = color0;
			}
			if (color01 != transparent) {
				dstLine0[3 * x + 1] =
				dstLine1[3 * x + 1] =
				dstLine2[3 * x + 1] = color01;
			}
			if (color1 != transparent) {
				dstLine0[3 * x + 2] =
				dstLine1[3 * x + 2] =
				dstLine2[3 * x + 2] = color1;
			}
		}
	}
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale2x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	dst.lock();
	Pixel transparent = dst.getKeyColor<Pixel>();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 3) {
		const Pixel* srcLine0 = src.getLinePtr<Pixel>(srcY + 0, srcWidth);
		const Pixel* srcLine1 = src.getLinePtr<Pixel>(srcY + 1, srcWidth);
		Pixel* dstLine0 = dst.getLinePtrDirect<Pixel>(dstY + 0);
		Pixel* dstLine1 = dst.getLinePtrDirect<Pixel>(dstY + 1);
		Pixel* dstLine2 = dst.getLinePtrDirect<Pixel>(dstY + 2);
		for (unsigned x = 0; x < srcWidth / 2; ++x) {
			//  0 |01| 1
			//  --+--+--
			//  02| M|13
			//  --+--+--
			//  2 |23| 3
			Pixel color0 = srcLine0[2 * x + 0];
			Pixel color1 = srcLine0[2 * x + 1];
			Pixel color2 = srcLine1[2 * x + 0];
			Pixel color3 = srcLine1[2 * x + 1];
			Pixel color01 = blendColorKey(this->pixelOps,
			                              color0, color1, transparent);
			Pixel color02 = blendColorKey(this->pixelOps,
			                              color0, color2, transparent);
			Pixel color13 = blendColorKey(this->pixelOps,
			                              color1, color3, transparent);
			Pixel color23 = blendColorKey(this->pixelOps,
			                              color2, color3, transparent);
			Pixel colorM  = blendColorKey(this->pixelOps,
			                              color01, color23, transparent);
			if (color0  != transparent) dstLine0[3 * x + 0] = color0;
			if (color01 != transparent) dstLine0[3 * x + 1] = color01;
			if (color1  != transparent) dstLine0[3 * x + 2] = color1;
			if (color02 != transparent) dstLine1[3 * x + 0] = color02;
			if (colorM  != transparent) dstLine1[3 * x + 1] = colorM;
			if (color13 != transparent) dstLine1[3 * x + 2] = color13;
			if (color2  != transparent) dstLine2[3 * x + 0] = color2;
			if (color23 != transparent) dstLine2[3 * x + 1] = color23;
			if (color3  != transparent) dstLine2[3 * x + 2] = color3;
		}
	}
}


template<typename Pixel>
void Transparent3xScaler<Pixel>::printWarning()
{
	static bool alreadyPrinted = false;
	if (alreadyPrinted) return;
	alreadyPrinted = true;

	cliComm.printWarning(
		"Transparent1xScaler not yet implemented for gfx9000 modes");
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale2x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler3<Pixel>::scale2x1to9x3(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale2x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler3<Pixel>::scale2x2to9x3(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale4x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler3<Pixel>::scale4x1to9x3(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale4x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler3<Pixel>::scale4x2to9x3(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale8x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler3<Pixel>::scale8x1to9x3(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale8x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler3<Pixel>::scale8x2to9x3(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale4x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler3<Pixel>::scale4x1to3x3(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}

template<typename Pixel>
void Transparent3xScaler<Pixel>::scale4x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	printWarning();
	Scaler3<Pixel>::scale4x2to3x3(src, srcStartY, srcEndY, srcWidth,
	                              dst, dstStartY, dstEndY);
}


// Force template instantiation.
#if HAVE_16BPP
template class Transparent3xScaler<word>;
#endif
#if HAVE_32BPP
template class Transparent3xScaler<unsigned>;
#endif

} // namespace openmsx
