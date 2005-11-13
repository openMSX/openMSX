// $Id$

#include "Scaler3.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "openmsx.hh"

namespace openmsx {

template <class Pixel>
Scaler3<Pixel>::Scaler3(SDL_PixelFormat* format)
	: pixelOps(format)
{
}

template <typename Pixel, typename ScaleOp>
static void doScale1(
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	Scale_1on1<Pixel> copy;
	for (unsigned dstY = dstStartY; dstY < dstEndY; dstY += 3, ++srcStartY) {
		Pixel* dummy = 0;
		const Pixel* srcLine = src.getLinePtr(srcStartY, dummy);
		Pixel* dstLine0 = dst.getLinePtr(dstY + 0, dummy);
		scale(srcLine, dstLine0, 960);

		if ((dstY + 1) == dstEndY) break;
		Pixel* dstLine1 = dst.getLinePtr(dstY + 1, dummy);
		if (IsTagged<ScaleOp, Streaming>::result) {
			scale(srcLine, dstLine1, 960);
		} else {
			copy(dstLine0, dstLine1, 960);
		}

		if ((dstY + 2) == dstEndY) break;
		Pixel* dstLine2 = dst.getLinePtr(dstY + 2, dummy);
		if (IsTagged<ScaleOp, Streaming>::result) {
			scale(srcLine, dstLine2, 960);
		} else {
			copy(dstLine0, dstLine2, 960);
		}
	}
}

template <typename Pixel, typename ScaleOp>
static void doScaleDV(
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	PixelOperations<Pixel> ops, ScaleOp scale)
{
	BlendLines<Pixel> blend(ops);
	for (unsigned dstY = dstStartY; dstY < dstEndY; dstY += 3) {
		Pixel* dummy = 0;
		const Pixel* srcLine0 = src.getLinePtr(srcStartY++, dummy);
		Pixel* dstLine0 = dst.getLinePtr(dstY + 0, dummy);
		scale(srcLine0, dstLine0, 960);

		// TODO if ((dstY + 2) == endDstY) { .. }
		const Pixel* srcLine1 = src.getLinePtr(srcStartY++, dummy);
		Pixel* dstLine2 = dst.getLinePtr(dstY + 2, dummy);
		scale(srcLine1, dstLine2, 960);

		// TODO if ((dstY + 1) == endDstY) { .. }
		Pixel* dstLine1 = dst.getLinePtr(dstY + 1, dummy);
		blend(dstLine0, dstLine2, dstLine1, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale2x1to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_2on9<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale2x2to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_2on9<Pixel>(pixelOps));
}

// TODO: See comment for Scaler2::scale256.
template <class Pixel>
void Scaler3<Pixel>::scale1x1to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_1on3<Pixel>());
}

template <class Pixel>
void Scaler3<Pixel>::scale1x2to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_1on3<Pixel>());
}

template <class Pixel>
void Scaler3<Pixel>::scale4x1to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_4on9<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale4x2to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_4on9<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale2x1to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale2x2to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale8x1to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_8on9<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale8x2to9x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_8on9<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale4x1to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void Scaler3<Pixel>::scale4x2to3x3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScaleDV<Pixel>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	                 pixelOps, Scale_4on3<Pixel>(pixelOps));
}

// TODO: This method doesn't have any dependency on the pixel format, so is it
//       possible to move it to a class without the Pixel template parameter?
template <class Pixel>
void Scaler3<Pixel>::scaleImage(
	FrameSource& src, unsigned lineWidth,
	unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (src.getHeight() == 240) {
		switch (lineWidth) {
		case 192:
			scale2x1to9x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 256:
			scale1x1to3x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 384:
			scale4x1to9x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 512:
			scale2x1to3x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 640:
			assert(false);
			break;
		case 768:
			scale8x1to9x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 1024:
			scale4x1to3x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		default:
			assert(false);
			break;
		}
	} else {
		assert(src.getHeight() == 480);
		switch (lineWidth) {
		case 192:
			scale2x2to9x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 256:
			scale1x2to3x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 384:
			scale4x2to9x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 512:
			scale2x2to3x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 640:
			assert(false);
			break;
		case 768:
			scale8x2to9x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		case 1024:
			scale4x2to3x3(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
			break;
		default:
			assert(false);
			break;
		}
	}
}


// Force template instantiation.
template class Scaler3<word>;
template class Scaler3<unsigned>;

} // namespace openmsx
