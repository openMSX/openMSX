#include "Scaler3.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "SuperImposeScalerOutput.hh"
#include "unreachable.hh"
#include "vla.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel>
Scaler3<Pixel>::Scaler3(const PixelOperations<Pixel>& pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 1, dstY += 3) {
		auto color = src.getLineColor<Pixel>(srcY);
		for (auto i : xrange(3)) {
			dst.fillLine(dstY + i, color);
		}
	}
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scaleBlank2to3(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 3) {
		auto color0 = src.getLineColor<Pixel>(srcY + 0);
		auto color1 = src.getLineColor<Pixel>(srcY + 1);
		Pixel color01 = pixelOps.template blend<1, 1>(color0, color1);
		dst.fillLine(dstY + 0, color0);
		dst.fillLine(dstY + 1, color01);
		dst.fillLine(dstY + 2, color1);
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
	unsigned dstWidth = dst.getWidth();
	for (unsigned dstY = dstStartY; dstY < dstEndY; dstY += 3, ++srcStartY) {
		auto srcLine = src.getLine(srcStartY, buf);
		auto* dstLine0 = dst.acquireLine(dstY + 0);
		scale(srcLine, std::span{dstLine0, dstWidth});

		auto* dstLine1 = dst.acquireLine(dstY + 1);
		copy(std::span{dstLine0, dstWidth}, std::span{dstLine1, dstWidth});

		auto* dstLine2 = dst.acquireLine(dstY + 2);
		copy(std::span{dstLine0, dstWidth}, std::span{dstLine2, dstWidth});

		dst.releaseLine(dstY + 0, dstLine0);
		dst.releaseLine(dstY + 1, dstLine1);
		dst.releaseLine(dstY + 2, dstLine2);
	}
}

template<std::unsigned_integral Pixel>
static void doScaleDV(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PixelOperations<Pixel> ops, PolyLineScaler<Pixel>& scale)
{
	VLA_SSE_ALIGNED(Pixel, buf, srcWidth);
	BlendLines<Pixel> blend(ops);
	unsigned dstWidth = dst.getWidth();
	for (unsigned srcY = srcStartY, dstY = dstStartY; dstY < dstEndY;
	     srcY += 2, dstY += 3) {
		auto srcLine0 = src.getLine(srcY + 0, buf);
		auto* dstLine0 = dst.acquireLine(dstY + 0);
		scale(srcLine0, std::span{dstLine0, dstWidth});

		auto srcLine1 = src.getLine(srcY + 1, buf);
		auto* dstLine2 = dst.acquireLine(dstY + 2);
		scale(srcLine1, std::span{dstLine2, dstWidth});

		auto* dstLine1 = dst.acquireLine(dstY + 1);
		blend(dstLine0, dstLine2, dstLine1, dstWidth);

		dst.releaseLine(dstY + 0, dstLine0);
		dst.releaseLine(dstY + 1, dstLine1);
		dst.releaseLine(dstY + 2, dstLine2);
	}
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale2x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on9<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale2x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on9<Pixel>> op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on3<Pixel>> op;
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale1x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on3<Pixel>> op;
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale4x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on9<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale4x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on9<Pixel>> op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale2x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale2x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel>> op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale8x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on9<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale8x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on9<Pixel>> op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale4x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel>> op(pixelOps);
	doScale1<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scale4x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel>> op(pixelOps);
	doScaleDV<Pixel>(src, srcStartY, srcEndY, srcWidth,
	                 dst, dstStartY, dstEndY, pixelOps, op);
}

template<std::unsigned_integral Pixel>
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

template<std::unsigned_integral Pixel>
void Scaler3<Pixel>::scaleImage(FrameSource& src, const RawFrame* superImpose,
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
template class Scaler3<uint16_t>;
#endif
#if HAVE_32BPP
template class Scaler3<uint32_t>;
#endif

} // namespace openmsx
