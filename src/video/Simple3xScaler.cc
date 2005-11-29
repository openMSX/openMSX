// $Id$

#include "Simple3xScaler.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "RenderSettings.hh"
#include "MemoryOps.hh"

namespace openmsx {

template <class Pixel>
Simple3xScaler<Pixel>::Simple3xScaler(
		SDL_PixelFormat* format, const RenderSettings& renderSettings)
	: Scaler3<Pixel>(format)
	, pixelOps(format)
	, scanline(format)
	, settings(renderSettings)
{
}

template <typename Pixel>
template <typename ScaleOp>
void Simple3xScaler<Pixel>::doScale1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	int scanlineFactor = settings.getScanlineFactor();
	unsigned y = dstStartY;
	Pixel* dummy = 0;
	const Pixel* srcLine = src.getLinePtr(srcStartY++, srcWidth, dummy);
	Pixel* prevDstLine0 = dst.getLinePtr(y + 0, dummy);
	scale(srcLine, prevDstLine0, 960);

	Scale_1on1<Pixel> copy;
	Pixel* dstLine1     = dst.getLinePtr(y + 1, dummy);
	copy(prevDstLine0, dstLine1, 960);

	for (/* */; (y + 4) < dstEndY; y += 3, srcStartY += 1) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, srcWidth, dummy);
		Pixel* dstLine0 = dst.getLinePtr(y + 3, dummy);
		scale(srcLine, dstLine0, 960);

		Pixel* dstLine1 = dst.getLinePtr(y + 4, dummy);
		copy(dstLine0, dstLine1, 960);

		Pixel* dstLine2 = dst.getLinePtr(y + 2, dummy);
		scanline.draw(prevDstLine0, dstLine0, dstLine2,
		              scanlineFactor, 960);

		prevDstLine0 = dstLine0;
	}
	// When interlace is enabled, the bottom line can fall off the screen.
	if ((y + 2) < dstEndY) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, srcWidth, dummy);
		Pixel buf[960];
		scale(srcLine, buf, 960);

		Pixel* dstLine2 = dst.getLinePtr(y + 2, dummy);
		scanline.draw(prevDstLine0, buf, dstLine2,
		              scanlineFactor, 960);
	}
}


template <class Pixel>
void Simple3xScaler<Pixel>::scale2x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_2on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_1on3<Pixel>());
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale4x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_4on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale2x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale8x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_8on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale4x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scaleBlank(Pixel color, OutputSurface& dst,
                                       unsigned startY, unsigned endY)
{
	int scanlineFactor = settings.getScanlineFactor();
	Pixel scanlineColor = scanline.darken(color, scanlineFactor);

	for (unsigned y = startY; y < endY; y += 3) {
		Pixel* dummy = 0;
		Pixel* dstLine0 = dst.getLinePtr(y + 0, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine0, 960, color);

		if ((y + 1) >= endY) break;
		Pixel* dstLine1 = dst.getLinePtr(y + 1, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine1, 960, color);

		if ((y + 2) >= endY) break;
		Pixel* dstLine2 = dst.getLinePtr(y + 2, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine2, 960, scanlineColor);
	}
}

// Force template instantiation.
template class Simple3xScaler<word>;
template class Simple3xScaler<unsigned>;

} // namespace openmsx
