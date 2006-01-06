// $Id$

#include "Scaler2.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "MemoryOps.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

template <class Pixel>
Scaler2<Pixel>::Scaler2(const PixelOperations<Pixel>& pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <class Pixel>
void Scaler2<Pixel>::scaleBlank1to2(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 2) {
		Pixel* dummy = 0;
		Pixel color = src.getLinePtr(srcY, dummy)[0];
		Pixel* dstLine0 = dst.getLinePtr(dstY + 0, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine0, dst.getWidth(), color);
		Pixel* dstLine1 = dst.getLinePtr(dstY + 1, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine1, dst.getWidth(), color);
	}
}

template <class Pixel>
void Scaler2<Pixel>::scaleBlank1to1(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 1) {
		Pixel* dummy = 0;
		Pixel color = src.getLinePtr(srcY, dummy)[0];
		Pixel* dstLine = dst.getLinePtr(dstY, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine, dst.getWidth(), color);
	}
}

template <typename Pixel, typename ScaleOp>
static void doScale1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	Scale_1on1<Pixel> copy;
	for (unsigned y = dstStartY; y < dstEndY; y += 2, ++srcStartY) {
		Pixel* dummy = 0;
		const Pixel* srcLine = src.getLinePtr(srcStartY, srcWidth, dummy);
		Pixel* dstLine1 = dst.getLinePtr(y + 0, dummy);
		scale(srcLine, dstLine1, dst.getWidth());
		Pixel* dstLine2 = dst.getLinePtr(y + 1, dummy);
		if (IsTagged<ScaleOp, Streaming>::result) {
			scale(srcLine, dstLine2, dst.getWidth());
		} else {
			copy(dstLine1, dstLine2, dst.getWidth());
		}
	}
}

template <typename Pixel, typename ScaleOp>
static void doScale2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; ++dstY, ++srcY) {
		Pixel* dummy = 0;
		const Pixel* srcLine = src.getLinePtr(srcY, srcWidth, dummy);
		Pixel*       dstLine = dst.getLinePtr(dstY, dummy);
		scale(srcLine, dstLine, dst.getWidth());
	}
}


template <class Pixel>
void Scaler2<Pixel>::scale1x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_1on3<Pixel>());
}

template <class Pixel>
void Scaler2<Pixel>::scale1x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_1on3<Pixel>());
}

// TODO: In theory it's nice to have this as a fallback method, but in practice
//       all subclasses override this method, so should we keep it or not?
//       And if we keep it, should it be commented out like this until we
//       need it to reduce the executable size?
//       See also Scaler3::scale256.
// TODO: Why won't it compile anymore without this method enabled?
template <class Pixel>
void Scaler2<Pixel>::scale1x1to2x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_1on2<Pixel>());
}

template <class Pixel>
void Scaler2<Pixel>::scale1x1to2x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_1on2<Pixel>());
}

template <class Pixel>
void Scaler2<Pixel>::scale2x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler2<Pixel>::scale2x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler2<Pixel>::scale1x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_1on1<Pixel>());
}

template <class Pixel>
void Scaler2<Pixel>::scale1x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_1on1<Pixel>());
}

template <class Pixel>
void Scaler2<Pixel>::scale4x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler2<Pixel>::scale4x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler2<Pixel>::scale2x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_2on1<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler2<Pixel>::scale2x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2<Pixel>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	                Scale_2on1<Pixel>(pixelOps));
}

// TODO: This method doesn't have any dependency on the pixel format, so is it
//       possible to move it to a class without the Pixel template parameter?
template <class Pixel>
void Scaler2<Pixel>::scaleImage(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (src.getHeight() == 240) {
		switch (srcWidth) {
		case 1:
			scaleBlank1to2(src, srcStartY, srcEndY,
			              dst, dstStartY, dstEndY);
			break;
		case 213:
			scale1x1to3x2(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 320:
			scale1x1to2x2(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 426:
			scale2x1to3x2(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 640:
			scale1x1to1x2(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 853:
			scale4x1to3x2(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 1280:
			scale2x1to1x2(src, srcStartY, srcEndY, srcWidth,
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
			scaleBlank1to1(src, srcStartY, srcEndY,
			              dst, dstStartY, dstEndY);
			break;
		case 213:
			scale1x1to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 320:
			scale1x1to2x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 426:
			scale2x1to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 640:
			scale1x1to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 853:
			scale4x1to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 1280:
			scale2x1to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		default:
			assert(false);
			break;
		}
	}
}

// Force template instantiation.
template class Scaler2<word>;
template class Scaler2<unsigned>;

} // namespace openmsx
