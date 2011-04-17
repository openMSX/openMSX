// $Id$

#include "Scaler3.hh"
#include "LineScalers.hh"
#include "RawFrame.hh"
#include "OutputSurface.hh"
#include "DirectScalerOutput.hh"
#include "openmsx.hh"
#include "unreachable.hh"
#include "build-info.hh"

namespace openmsx {

template <class Pixel>
Scaler3<Pixel>::Scaler3(const PixelOperations<Pixel>& pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <class Pixel>
void Scaler3<Pixel>::scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 3) {
		Pixel color = src.getLinePtr<Pixel>(srcY)[0];
		for (int i = 0; i < 3; ++i) {
			dst.fillLine(dstY + i, color);
		}
	}
}

template <class Pixel>
void Scaler3<Pixel>::scaleBlank2to3(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 3) {
		Pixel color0 = src.getLinePtr<Pixel>(srcY + 0)[0];
		Pixel color1 = src.getLinePtr<Pixel>(srcY + 1)[0];
		Pixel color01 = pixelOps.template blend<1, 1>(color0, color1);
		dst.fillLine(dstY + 0, color0);
		dst.fillLine(dstY + 1, color01);
		dst.fillLine(dstY + 2, color1);
	}
}

template <typename Pixel>
static void doScale1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PolyLineScaler<Pixel>& scale)
{
	Scale_1on1<Pixel> copy;
	bool isStreaming = scale.isStreaming();
	unsigned dstWidth = dst.getWidth();
	for (unsigned dstY = dstStartY; dstY < dstEndY; dstY += 3, ++srcStartY) {
		const Pixel* srcLine = src.getLinePtr<Pixel>(srcStartY, srcWidth);
		Pixel* dstLine0 = dst.acquireLine(dstY + 0);
		scale(srcLine, dstLine0, dstWidth);

		Pixel* dstLine1 = dst.acquireLine(dstY + 1);
		if (isStreaming) {
			scale(srcLine, dstLine1, dstWidth);
		} else {
			copy(dstLine0, dstLine1, dstWidth);
		}

		Pixel* dstLine2 = dst.acquireLine(dstY + 2);
		if (isStreaming) {
			scale(srcLine, dstLine2, dstWidth);
		} else {
			copy(dstLine0, dstLine2, dstWidth);
		}

		dst.releaseLine(dstY + 0, dstLine0);
		dst.releaseLine(dstY + 1, dstLine1);
		dst.releaseLine(dstY + 2, dstLine2);
	}
}

template <typename Pixel>
static void doScaleDV(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PixelOperations<Pixel> ops, PolyLineScaler<Pixel>& scale)
{
	BlendLines<Pixel> blend(ops);
	unsigned dstWidth = dst.getWidth();
	for (unsigned srcY = srcStartY, dstY = dstStartY; dstY < dstEndY;
	     srcY += 2, dstY += 3) {
		const Pixel* srcLine0 = src.getLinePtr<Pixel>(srcY + 0, srcWidth);
		Pixel* dstLine0 = dst.acquireLine(dstY + 0);
		scale(srcLine0, dstLine0, dstWidth);

		const Pixel* srcLine1 = src.getLinePtr<Pixel>(srcY + 1, srcWidth);
		Pixel* dstLine2 = dst.acquireLine(dstY + 2);
		scale(srcLine1, dstLine2, dstWidth);

		Pixel* dstLine1 = dst.acquireLine(dstY + 1);
		blend(dstLine0, dstLine2, dstLine1, dstWidth);

		dst.releaseLine(dstY + 0, dstLine0);
		dst.releaseLine(dstY + 1, dstLine1);
		dst.releaseLine(dstY + 2, dstLine2);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale2x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on9<Pixel> > op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Scaler3<Pixel>::scale2x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on9<Pixel> > op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template <class Pixel>
void Scaler3<Pixel>::scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on3<Pixel> > op;
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Scaler3<Pixel>::scale1x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on3<Pixel> > op;
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template <class Pixel>
void Scaler3<Pixel>::scale4x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on9<Pixel> > op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Scaler3<Pixel>::scale4x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on9<Pixel> > op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template <class Pixel>
void Scaler3<Pixel>::scale2x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel> > op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Scaler3<Pixel>::scale2x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel> > op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template <class Pixel>
void Scaler3<Pixel>::scale8x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on9<Pixel> > op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Scaler3<Pixel>::scale8x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on9<Pixel> > op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template <class Pixel>
void Scaler3<Pixel>::scale4x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel> > op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Scaler3<Pixel>::scale4x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel> > op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template <class Pixel>
void Scaler3<Pixel>::dispatchScale(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (src.getHeight() == 240) {
		switch (srcWidth) {
		case 1:
			scaleBlank1to3(src, srcStartY, srcEndY,
			              dst, dstStartY, dstEndY);
			break;
		case 213:
			scale2x1to9x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 320:
			scale1x1to3x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 426:
			scale4x1to9x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 640:
			scale2x1to3x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 853:
			scale8x1to9x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 1280:
			scale4x1to3x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		default:
			UNREACHABLE;
		}
	} else {
		assert(src.getHeight() == 480);
		switch (srcWidth) {
		case 1:
			scaleBlank2to3(src, srcStartY, srcEndY,
			              dst, dstStartY, dstEndY);
			break;
		case 213:
			scale2x2to9x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 320:
			scale1x2to3x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 426:
			scale4x2to9x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 640:
			scale2x2to3x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 853:
			scale8x2to9x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 1280:
			scale4x2to3x3(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		default:
			UNREACHABLE;
		}
	}
}

template <class Pixel>
void Scaler3<Pixel>::scaleImage(FrameSource& src, const RawFrame* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& out, unsigned dstStartY, unsigned dstEndY)
{
	DirectScalerOutput<Pixel> dst(out);
	dispatchScale(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY);

	// TODO move superimpose to ScalerOutput pipeline
	if (superImpose) {
		AlphaBlendLines<Pixel> alphaBlend(pixelOps);
		for (unsigned y = dstStartY; y < dstEndY; ++y) {
			Pixel* dstLine = out.getLinePtrDirect<Pixel>(y);
			const Pixel* srcLine = superImpose->getLinePtr960_720<Pixel>(y);
			alphaBlend(dstLine, srcLine, dstLine, 960);
			superImpose->freeLineBuffers();
		}
	}
}

// Force template instantiation.
#if HAVE_16BPP
template class Scaler3<word>;
#endif
#if HAVE_32BPP
template class Scaler3<unsigned>;
#endif

} // namespace openmsx
