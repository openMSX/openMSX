#include "Scaler2.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "SuperImposeScalerOutput.hh"
#include "unreachable.hh"
#include "vla.hh"
#include "build-info.hh"
#include <cassert>
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel>
Scaler2<Pixel>::Scaler2(const PixelOperations<Pixel>& pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scaleBlank1to2(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 2) {
		auto color = src.getLineColor<Pixel>(srcY);
		dst.fillLine(dstY + 0, color);
		dst.fillLine(dstY + 1, color);
	}
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scaleBlank1to1(
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
static void doScale1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PolyLineScaler<Pixel>& scale)
{
	VLA_SSE_ALIGNED(Pixel, buf, srcWidth);
	Scale_1on1<Pixel> copy;
	for (unsigned y = dstStartY; y < dstEndY; y += 2, ++srcStartY) {
		auto srcLine = src.getLine(srcStartY, buf);
		auto dstLine0 = dst.acquireLine(y + 0);
		scale(srcLine, dstLine0);
		auto dstLine1 = dst.acquireLine(y + 1);
		copy(dstLine0, dstLine1);
		dst.releaseLine(y + 0, dstLine0);
		dst.releaseLine(y + 1, dstLine1);
	}
}

template<std::unsigned_integral Pixel>
static void doScale2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PolyLineScaler<Pixel>& scale)
{
	VLA_SSE_ALIGNED(Pixel, buf, srcWidth);
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; ++dstY, ++srcY) {
		auto srcLine = src.getLine(srcY, buf);
		auto dstLine = dst.acquireLine(dstY);
		scale(srcLine, dstLine);
		dst.releaseLine(dstY, dstLine);
	}
}


template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale1x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on3<Pixel>> op;
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale1x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on3<Pixel>> op;
	doScale2<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

// TODO: In theory it's nice to have this as a fallback method, but in practice
//       all subclasses override this method, so should we keep it or not?
//       And if we keep it, should it be commented out like this until we
//       need it to reduce the executable size?
//       See also Scaler3::scale256.
// TODO: Why won't it compile anymore without this method enabled?
template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale1x1to2x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on2<Pixel>> op;
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale1x1to2x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on2<Pixel>> op;
	doScale2<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale2x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale2x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel>> op(pixelOps);
	doScale2<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale1x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	// Optimized variant: possibly avoid copy.
	assert(dst.getWidth() == srcWidth);
	Scale_1on1<Pixel> copy;
	for (unsigned y = dstStartY; y < dstEndY; y += 2, ++srcStartY) {
		auto dstLine0 = dst.acquireLine(y + 0);
		auto srcLine = src.getLine(srcStartY, dstLine0);
		if (srcLine.data() != dstLine0.data()) copy(srcLine, dstLine0);
		auto dstLine1 = dst.acquireLine(y + 1);
		copy(dstLine0, dstLine1);
		dst.releaseLine(y + 0, dstLine0);
		dst.releaseLine(y + 1, dstLine1);
	}
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale1x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	// Optimized variant: possibly avoid copy.
	assert(dst.getWidth() == srcWidth);
	Scale_1on1<Pixel> copy;
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; ++dstY, ++srcY) {
		auto dstLine = dst.acquireLine(dstY);
		auto srcLine = src.getLine(srcY, dstLine);
		if (srcLine.data() != dstLine.data()) copy(srcLine, dstLine);
		dst.releaseLine(dstY, dstLine);
	}
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale4x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale4x1to3x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel>> op(pixelOps);
	doScale2<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale2x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on1<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scale2x1to1x1(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on1<Pixel>> op(pixelOps);
	doScale2<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::dispatchScale(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
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
			UNREACHABLE;
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
			UNREACHABLE;
		}
	}
}

template<std::unsigned_integral Pixel>
void Scaler2<Pixel>::scaleImage(FrameSource& src, const RawFrame* superImpose,
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
template class Scaler2<uint16_t>;
#endif
#if HAVE_32BPP
template class Scaler2<uint32_t>;
#endif

} // namespace openmsx
