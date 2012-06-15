// $Id$

#include "Scaler1.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "RawFrame.hh"
#include "SuperImposeScalerOutput.hh"
#include "openmsx.hh"
#include "vla.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include <cassert>

namespace openmsx {

template <typename Pixel>
Scaler1<Pixel>::Scaler1(const PixelOperations<Pixel>& pixelOps_)
	: pixelOps(pixelOps_)
{
}

/*template <typename Pixel>
void Scaler1<Pixel>::averageHalve(const Pixel* pIn0, const Pixel* pIn1, Pixel* pOut, unsigned dstWidth)
{
	// TODO MMX/SSE optimizations
	// pure C++ version
	for (int i = 0; i < dstWidth; ++i) {
		Pixel tmp0 = blend(pIn0[2 * i + 0], pIn0[2 * i + 1]);
		Pixel tmp1 = blend(pIn1[2 * i + 0], pIn1[2 * i + 1]);
		pOut[i] = blend(tmp0, tmp1);
	}
}*/

template <class Pixel>
void Scaler1<Pixel>::scaleBlank1to1(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	/*if (PLATFORM_GP2X) {
		// note: src.getLinePtr() internally does a src.lock(). In a
		//       profile the Lock function is relatively high. Though
		//       if we put the blank line color info in some other
		//       data structure (no need to lock()), the FillRect
		//       function takes the place of Lock. So it seems we are
		//       simply waiting for the previous blit command to
		//       finish.
		dst.unlock();
		SDL_Rect dstRect;
		dstRect.x = 0;
		dstRect.w = dst.getWidth();
		for (unsigned srcY = srcStartY, dstY = dstStartY; dstY < dstEndY; ) {
			dstRect.y = dstY;
			Pixel color = src.getLinePtr<Pixel>(srcY)[0];
			unsigned start = srcY;
			do {
				srcY += 1;
			} while ((src.getLinePtr<Pixel>(srcY)[0] == color) &&
				 (srcY < srcEndY));
			unsigned height = srcY - start;
			dstY += height;
			dstRect.h = height;
			SDL_FillRect(dst.getSDLWorkSurface(), &dstRect, color);
		}
	} else {*/
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 1) {
		Pixel color = src.getLinePtr<Pixel>(srcY)[0];
		dst.fillLine(dstY, color);
	}
}

template <class Pixel>
void Scaler1<Pixel>::scaleBlank2to1(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 1) {
		Pixel color0 = src.getLinePtr<Pixel>(srcY + 0)[0];
		Pixel color1 = src.getLinePtr<Pixel>(srcY + 1)[0];
		Pixel color01 = pixelOps.template blend<1, 1>(color0, color1);
		dst.fillLine(dstY, color01);
	}
}

template <typename Pixel>
static void doScale1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PolyLineScaler<Pixel>& scale)
{
	unsigned dstWidth = dst.getWidth();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; ++srcY, ++dstY) {
		const Pixel* srcLine = src.getLinePtr<Pixel>(srcY, srcWidth);
		Pixel* dstLine = dst.acquireLine(dstY);
		scale(srcLine, dstLine, dstWidth);
		dst.releaseLine(dstY, dstLine);
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
	VLA_ALIGNED(Pixel, buf0, dstWidth, 16);
	VLA_ALIGNED(Pixel, buf1, dstWidth, 16);
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 1) {
		const Pixel* srcLine0 = src.getLinePtr<Pixel>(srcY + 0, srcWidth);
		const Pixel* srcLine1 = src.getLinePtr<Pixel>(srcY + 1, srcWidth);
		scale(srcLine0, buf0, dstWidth);
		scale(srcLine1, buf1, dstWidth);
		Pixel* dstLine = dst.acquireLine(dstY);
		blend(buf0, buf1, dstLine, dstWidth);
		dst.releaseLine(dstY, dstLine);
	}
}


template <class Pixel>
void Scaler1<Pixel>::scale2x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel> > op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Scaler1<Pixel>::scale2x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel> > op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template <typename Pixel>
void Scaler1<Pixel>::scale1x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	/*if (PLATFORM_GP2X) {
		if (RawFrame* raw = dynamic_cast<RawFrame*>(&src)) {
			raw->unlock();
			dst.unlock();
			unsigned height = dstEndY - dstStartY;
			SDL_Rect srcRect, dstRect;
			srcRect.x = 0;        srcRect.y = srcStartY;
			srcRect.w = srcWidth; srcRect.h = height;
			dstRect.x = 0;        dstRect.y = dstStartY;
			SDL_BlitSurface(raw->getSDLSurface(), &srcRect,
			                dst.getSDLWorkSurface(),  &dstRect);
			return;
		}
	}*/
	PolyScale<Pixel, Scale_1on1<Pixel> > op;
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <typename Pixel>
void Scaler1<Pixel>::scale1x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	// No need to scale to local buffer first, like doScaleDV does.
	// TODO: Even so, this scale mode is so rare, is it really worth having
	//       optimized code for?
	unsigned dstWidth = dst.getWidth();
	BlendLines<Pixel> blend(pixelOps);
	for (unsigned dstY = dstStartY; dstY < dstEndY; ++dstY) {
		const Pixel* srcLine0 = src.getLinePtr<Pixel>(srcStartY++, srcWidth);
		const Pixel* srcLine1 = src.getLinePtr<Pixel>(srcStartY++, srcWidth);
		Pixel* dstLine = dst.acquireLine(dstY);
		blend(srcLine0, srcLine1, dstLine, dstWidth);
		dst.releaseLine(dstY, dstLine);
	}
}

template <class Pixel>
void Scaler1<Pixel>::scale4x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel> > op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Scaler1<Pixel>::scale4x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel> > op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template <typename Pixel>
void Scaler1<Pixel>::scale2x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on1<Pixel> > op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <typename Pixel>
void Scaler1<Pixel>::scale2x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on1<Pixel> > op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template <class Pixel>
void Scaler1<Pixel>::scale8x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on3<Pixel> > op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Scaler1<Pixel>::scale8x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on3<Pixel> > op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template <class Pixel>
void Scaler1<Pixel>::scale4x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on1<Pixel> > op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Scaler1<Pixel>::scale4x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on1<Pixel> > op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template <class Pixel>
void Scaler1<Pixel>::dispatchScale(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (src.getHeight() == 240) {
		switch (srcWidth) {
		case 1:
			scaleBlank1to1(src, srcStartY, srcEndY,
			               dst, dstStartY, dstEndY);
			break;
		case 213:
			scale2x1to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 320:
			scale1x1to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 426:
			scale4x1to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 640:
			scale2x1to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 853:
			scale8x1to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 1280:
			scale4x1to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		default:
			UNREACHABLE;
		}
	} else {
		assert(src.getHeight() == 480);
		switch (srcWidth) {
		case 1:
			scaleBlank2to1(src, srcStartY, srcEndY,
			               dst, dstStartY, dstEndY);
			break;
		case 213:
			scale2x2to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 320:
			scale1x2to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 426:
			scale4x2to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 640:
			scale2x2to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 853:
			scale8x2to3x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		case 1280:
			scale4x2to1x1(src, srcStartY, srcEndY, srcWidth,
			              dst, dstStartY, dstEndY);
			break;
		default:
			UNREACHABLE;
		}
	}
}

template <class Pixel>
void Scaler1<Pixel>::scaleImage(FrameSource& src, const RawFrame* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (superImpose) {
		SuperImposeScalerOutput<Pixel> dst2(
			dst, *superImpose, pixelOps);
		dispatchScale(src, srcStartY, srcEndY, srcWidth,
		              dst2, dstStartY, dstEndY);
	} else {
		dispatchScale(src, srcStartY, srcEndY, srcWidth,
		              dst, dstStartY, dstEndY);
	}
}

// Force template instantiation.
#if HAVE_16BPP
template class Scaler1<word>;
#endif
#if HAVE_32BPP
template class Scaler1<unsigned>;
#endif

} // namespace openmsx
