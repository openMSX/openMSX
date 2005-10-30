// $Id$

#include "Scaler3.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include <algorithm>

namespace openmsx {

template <class Pixel>
Scaler3<Pixel>::Scaler3(SDL_PixelFormat* format)
	: pixelOps(format)
{
}

template <typename Pixel, typename ScaleOp>
static void doScale1(
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	Scale_1on1<Pixel> copy;
	for (unsigned dstY = dstStartY; dstY < dstEndY; dstY += 3, ++srcStartY) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, dstY + 0);
		scale(srcLine, dstLine0, 960);

		if ((dstY + 1) == dstEndY) break;
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY + 1);
		if (IsTagged<ScaleOp, Streaming>::result) {
			scale(srcLine, dstLine1, 960);
		} else {
			copy(dstLine0, dstLine1, 960);
		}

		if ((dstY + 2) == dstEndY) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, dstY + 2);
		if (IsTagged<ScaleOp, Streaming>::result) {
			scale(srcLine, dstLine2, 960);
		} else {
			copy(dstLine0, dstLine2, 960);
		}
	}
}

template <typename Pixel, typename ScaleOp>
static void doScale2(
	FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	unsigned startY, unsigned endY, PixelOperations<Pixel> ops,
	ScaleOp scale)
{
	BlendLines<Pixel> blend(ops);
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 3 * y + 0);
		scale(srcLine0, dstLine0, 960);

		// TODO if ((dstY + 2) == endDstY) { .. }
		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, 3 * y + 2);
		scale(srcLine1, dstLine2, 960);

		// TODO if ((dstY + 1) == endDstY) { .. }
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 3 * y + 1);
		blend(dstLine0, dstLine2, dstLine1, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale192(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_2on9<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale192(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY, pixelOps,
	                Scale_2on9<Pixel>(pixelOps));
}

// TODO: See comment for Scaler2::scale256.
template <class Pixel>
void Scaler3<Pixel>::scale256(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_1on3<Pixel>());
}

template <class Pixel>
void Scaler3<Pixel>::scale256(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY, pixelOps,
	                Scale_1on3<Pixel>());
}

template <class Pixel>
void Scaler3<Pixel>::scale384(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_4on9<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale384(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY, pixelOps,
	                Scale_4on9<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale512(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale512(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY, pixelOps,
	                Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale640(
		FrameSource& /*src*/, unsigned /*srcStartY*/, unsigned /*srcEndY*/,
		SDL_Surface* /*dst*/, unsigned /*dstStartY*/, unsigned /*dstEndY*/)
{
	// TODO
}

template <class Pixel>
void Scaler3<Pixel>::scale640(FrameSource& /*src0*/, FrameSource& /*src1*/, SDL_Surface* /*dst*/,
                             unsigned /*startY*/, unsigned /*endY*/)
{
	// TODO
}

template <class Pixel>
void Scaler3<Pixel>::scale768(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_8on9<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale768(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY, pixelOps,
	                Scale_8on9<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale1024(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale1024(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                              unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY, pixelOps,
	                Scale_4on3<Pixel>(pixelOps));
}

// TODO: This method doesn't have any dependency on the pixel format, so is it
//       possible to move it to a class without the Pixel template parameter?
template <class Pixel>
void Scaler3<Pixel>::scaleImage(
	FrameSource& src, unsigned lineWidth,
	unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY
) {
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
void Scaler3<Pixel>::scaleImage(
	FrameSource& frameEven, FrameSource& frameOdd, unsigned lineWidth,
	SDL_Surface* dst, unsigned srcStartY, unsigned srcEndY
) {
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
template class Scaler3<word>;
template class Scaler3<unsigned>;

} // namespace openmsx
