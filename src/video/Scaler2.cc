// $Id$

#include "Scaler2.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include <algorithm>

namespace openmsx {

template <class Pixel>
Scaler2<Pixel>::Scaler2(SDL_PixelFormat* format)
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
	for (unsigned y = dstStartY; y < dstEndY; y += 2, ++srcStartY) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale(srcLine, dstLine1, 640);
		if ((y + 1) == dstEndY) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		if (IsTagged<ScaleOp, Streaming>::result) {
			scale(srcLine, dstLine2, 640);
		} else {
			copy(dstLine1, dstLine2, 640);
		}
	}
}

template <typename Pixel, typename ScaleOp>
static void doScale2(
	FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	unsigned startY, unsigned endY, ScaleOp scale)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		scale(srcLine0, dstLine0, 640);

		// TODO if ((dstY + 1) == endDstY) break;
		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		scale(srcLine1, dstLine1, 640);
	}
}


template <class Pixel>
void Scaler2<Pixel>::scale192(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_1on3<Pixel>());
}

template <class Pixel>
void Scaler2<Pixel>::scale192(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY,
	                Scale_1on3<Pixel>());
}

// TODO: In theory it's nice to have this as a fallback method, but in practice
//       all subclasses override this method, so should we keep it or not?
//       And if we keep it, should it be commented out like this until we
//       need it to reduce the executable size?
//       See also Scaler3::scale256.
/*template <class Pixel>
void Scaler2<Pixel>::scale256(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_1on2<Pixel>());
}*/

template <class Pixel>
void Scaler2<Pixel>::scale256(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY,
	                Scale_1on2<Pixel>());
}

template <class Pixel>
void Scaler2<Pixel>::scale384(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler2<Pixel>::scale384(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY,
	                Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler2<Pixel>::scale512(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_1on1<Pixel>());
}

template <class Pixel>
void Scaler2<Pixel>::scale512(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY,
	                Scale_1on1<Pixel>());
}

template <class Pixel>
void Scaler2<Pixel>::scale640(
	FrameSource& /*src*/, unsigned /*srcStartY*/, unsigned /*srcEndY*/,
	SDL_Surface* /*dst*/, unsigned /*dstStartY*/, unsigned /*dstEndY*/)
{
	// TODO
}

template <class Pixel>
void Scaler2<Pixel>::scale640(FrameSource& /*src0*/, FrameSource& /*src1*/, SDL_Surface* /*dst*/,
                             unsigned /*startY*/, unsigned /*endY*/)
{
	// TODO
}

template <class Pixel>
void Scaler2<Pixel>::scale768(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler2<Pixel>::scale768(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY,
	                Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler2<Pixel>::scale1024(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_2on1<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler2<Pixel>::scale1024(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                              unsigned startY, unsigned endY)
{
	doScale2<Pixel>(src0, src1, dst, startY, endY,
	                Scale_2on1<Pixel>(pixelOps));
}


// Force template instantiation.
template class Scaler2<word>;
template class Scaler2<unsigned>;

} // namespace openmsx
