// $Id$

#include "Simple3xScaler.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
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
void Simple3xScaler<Pixel>::doScale1(
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	int scanlineFactor = settings.getScanlineFactor();
	unsigned y = dstStartY;
	const Pixel* srcLine = src.getLinePtr(srcStartY++, (Pixel*)0);
	Pixel* prevDstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
	scale(srcLine, prevDstLine0, 960);

	Scale_1on1<Pixel> copy;
	Pixel* dstLine1     = Scaler<Pixel>::linePtr(dst, y + 1);
	copy(prevDstLine0, dstLine1, 960);

	for (/* */; (y + 4) < dstEndY; y += 3, srcStartY += 1) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 3);
		scale(srcLine, dstLine0, 960);

		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 4);
		copy(dstLine0, dstLine1, 960);

		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		scanline.draw(prevDstLine0, dstLine0, dstLine2,
		              scanlineFactor, 960);

		prevDstLine0 = dstLine0;
	}
	// When interlace is enabled, the bottom line can fall off the screen.
	if ((y + 2) < dstEndY) {
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		scanline.draw(prevDstLine0, prevDstLine0, dstLine2,
		              scanlineFactor, 960);
	}
}


template <class Pixel>
void Simple3xScaler<Pixel>::scale192(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_2on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale256(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_1on3<Pixel>());
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale384(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_4on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale512(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale640(
		FrameSource& /*src*/, unsigned /*srcStartY*/, unsigned /*srcEndY*/,
		SDL_Surface* /*dst*/, unsigned /*dstStartY*/, unsigned /*dstEndY*/)
{
	// TODO
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale768(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_8on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale1024(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scaleBlank(Pixel color, SDL_Surface* dst,
                               unsigned startY, unsigned endY)
{
	int scanlineFactor = settings.getScanlineFactor();
	Pixel scanlineColor = scanline.darken(color, scanlineFactor);

	for (unsigned y = startY; y < endY; y += 3) {
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine0, 960, color);

		if ((y + 1) >= endY) break;
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 1);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine1, 960, color);

		if ((y + 2) >= endY) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine2, 960, scanlineColor);
	}
}

// Force template instantiation.
template class Simple3xScaler<word>;
template class Simple3xScaler<unsigned>;

} // namespace openmsx
