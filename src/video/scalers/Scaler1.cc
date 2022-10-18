#include "Scaler1.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "SuperImposeScalerOutput.hh"
#include "vla.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel>
Scaler1<Pixel>::Scaler1(const PixelOperations<Pixel>& pixelOps_)
	: pixelOps(pixelOps_)
{
}

/*template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::averageHalve(const Pixel* pIn0, const Pixel* pIn1, Pixel* pOut, unsigned dstWidth)
{
	// TODO SSE optimizations
	// pure C++ version
	for (auto i : xrange(dstWidth)) {
		Pixel tmp0 = blend(pIn0[2 * i + 0], pIn0[2 * i + 1]);
		Pixel tmp1 = blend(pIn1[2 * i + 0], pIn1[2 * i + 1]);
		pOut[i] = blend(tmp0, tmp1);
	}
}*/

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scaleBlank1to1(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 1) {
		auto color = src.getLineColor<Pixel>(srcY);
		dst.fillLine(dstY, color);
	}
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scaleBlank2to1(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 1) {
		auto color0 = src.getLineColor<Pixel>(srcY + 0);
		auto color1 = src.getLineColor<Pixel>(srcY + 1);
		Pixel color01 = pixelOps.template blend<1, 1>(color0, color1);
		dst.fillLine(dstY, color01);
	}
}

template<std::unsigned_integral Pixel>
static void doScale1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PolyLineScaler<Pixel>& scale)
{
	VLA_SSE_ALIGNED(Pixel, buf, srcWidth);
	unsigned dstWidth = dst.getWidth();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; ++srcY, ++dstY) {
		auto* srcLine = src.getLinePtr(srcY, srcWidth, buf.data());
		auto* dstLine = dst.acquireLine(dstY);
		scale(srcLine, dstLine, dstWidth);
		dst.releaseLine(dstY, dstLine);
	}
}

template<std::unsigned_integral Pixel>
static void doScaleDV(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PixelOperations<Pixel> ops, PolyLineScaler<Pixel>& scale)
{
	BlendLines<Pixel> blend(ops);
	unsigned dstWidth = dst.getWidth();
	VLA_SSE_ALIGNED(Pixel, buf0, std::max(srcWidth, dstWidth));
	VLA_SSE_ALIGNED(Pixel, buf1, srcWidth);
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 1) {
		auto* srcLine0 = src.getLinePtr(srcY + 0, srcWidth, buf0.data());
		auto* srcLine1 = src.getLinePtr(srcY + 1, srcWidth, buf1.data());
		auto* dstLine = dst.acquireLine(dstY);
		scale(srcLine0, dstLine,     dstWidth); // dstLine  iso buf0
		scale(srcLine1, buf0.data(), dstWidth); // buf0 iso buf1
		blend(dstLine, buf0.data(), dstLine, dstWidth); // use input as output
		dst.releaseLine(dstY, dstLine);
	}
}


template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale2x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale2x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel>> op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale1x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	// Optimized variant: pass dstLine to getLinePtr(), so we can
	// potentionally avoid the copy operation.
	assert(dst.getWidth() == srcWidth);
	Scale_1on1<Pixel> copy;
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; ++srcY, ++dstY) {
		auto* dstLine = dst.acquireLine(dstY);
		auto* srcLine = src.getLinePtr(srcY, srcWidth, dstLine);
		if (srcLine != dstLine) copy(srcLine, dstLine, srcWidth);
		dst.releaseLine(dstY, dstLine);
	}
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale1x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	// No need to scale to local buffer first, like doScaleDV does.
	VLA_SSE_ALIGNED(Pixel, buf, srcWidth);
	unsigned dstWidth = dst.getWidth();
	BlendLines<Pixel> blend(pixelOps);
	for (auto dstY : xrange(dstStartY, dstEndY)) {
		auto* dstLine = dst.acquireLine(dstY);
		auto* srcLine0 = src.getLinePtr(srcStartY++, srcWidth, dstLine);
		auto* srcLine1 = src.getLinePtr(srcStartY++, srcWidth, buf.data());
		blend(srcLine0, srcLine1, dstLine, dstWidth); // possibly srcLine0 == dstLine
		dst.releaseLine(dstY, dstLine);
	}
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale4x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale4x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel>> op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale2x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on1<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale2x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on1<Pixel>> op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale8x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on3<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale8x2to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on3<Pixel>> op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale4x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on1<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler1<Pixel>::scale4x2to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on1<Pixel>> op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template<std::unsigned_integral Pixel>
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

template<std::unsigned_integral Pixel>
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
template class Scaler1<uint16_t>;
#endif
#if HAVE_32BPP
template class Scaler1<uint32_t>;
#endif

} // namespace openmsx
