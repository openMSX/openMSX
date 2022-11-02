#include "Simple3xScaler.hh"
#include "SuperImposedVideoFrame.hh"
#include "LineScalers.hh"
#include "RawFrame.hh"
#include "ScalerOutput.hh"
#include "RenderSettings.hh"
#include "narrow.hh"
#include "vla.hh"
#include <cstdint>
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx {

template<std::unsigned_integral Pixel>
Simple3xScaler<Pixel>::Simple3xScaler(
		const PixelOperations<Pixel>& pixelOps_,
		const RenderSettings& settings_)
	: Scaler3<Pixel>(pixelOps_)
	, pixelOps(pixelOps_)
	, scanline(pixelOps_)
	, blur_1on3(pixelOps_)
	, settings(settings_)
{
}

template<std::unsigned_integral Pixel>
Simple3xScaler<Pixel>::~Simple3xScaler() = default;

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::doScale1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PolyLineScaler<Pixel>& scale)
{
	VLA_SSE_ALIGNED(Pixel, buf, srcWidth);
	int scanlineFactor = settings.getScanlineFactor();
	unsigned dstWidth = dst.getWidth();
	unsigned y = dstStartY;
	auto srcLine = src.getLine(srcStartY++, buf);
	auto dstLine0 = dst.acquireLine(y + 0);
	scale(srcLine, dstLine0);

	Scale_1on1<Pixel> copy;
	auto dstLine1 = dst.acquireLine(y + 1);
	copy(dstLine0, dstLine1);

	for (/* */; (y + 4) < dstEndY; y += 3, srcStartY += 1) {
		srcLine = src.getLine(srcStartY, buf);
		auto dstLine3 = dst.acquireLine(y + 3);
		scale(srcLine, dstLine3);

		auto dstLine4 = dst.acquireLine(y + 4);
		copy(dstLine3, dstLine4);

		auto dstLine2 = dst.acquireLine(y + 2);
		scanline.draw(dstLine0, dstLine3,
		              dstLine2, scanlineFactor);

		dst.releaseLine(y + 0, dstLine0);
		dst.releaseLine(y + 1, dstLine1);
		dst.releaseLine(y + 2, dstLine2);
		dstLine0 = dstLine3;
		dstLine1 = dstLine4;
	}
	srcLine = src.getLine(srcStartY, buf);
	VLA_SSE_ALIGNED(Pixel, buf2, dstWidth);
	scale(srcLine, buf2);

	auto dstLine2 = dst.acquireLine(y + 2);
	scanline.draw(dstLine0, buf2, dstLine2, scanlineFactor);
	dst.releaseLine(y + 0, dstLine0);
	dst.releaseLine(y + 1, dstLine1);
	dst.releaseLine(y + 2, dstLine2);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::doScale2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PolyLineScaler<Pixel>& scale)
{
	VLA_SSE_ALIGNED(Pixel, buf, srcWidth);
	int scanlineFactor = settings.getScanlineFactor();
	for (unsigned srcY = srcStartY, dstY = dstStartY; dstY < dstEndY;
	     srcY += 2, dstY += 3) {
		auto srcLine0 = src.getLine(srcY + 0, buf);
		auto dstLine0 = dst.acquireLine(dstY + 0);
		scale(srcLine0, dstLine0);

		auto srcLine1 = src.getLine(srcY + 1, buf);
		auto dstLine2 = dst.acquireLine(dstY + 2);
		scale(srcLine1, dstLine2);

		auto dstLine1 = dst.acquireLine(dstY + 1);
		scanline.draw(dstLine0, dstLine2, dstLine1,
		              scanlineFactor);

		dst.releaseLine(dstY + 0, dstLine0);
		dst.releaseLine(dstY + 1, dstLine1);
		dst.releaseLine(dstY + 2, dstLine2);
	}
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale2x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on9<Pixel>> op(pixelOps);
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale2x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on9<Pixel>> op(pixelOps);
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (unsigned blur = settings.getBlurFactor() / 3) {
		blur_1on3.setBlur(blur);
		PolyScaleRef<Pixel, Blur_1on3<Pixel>> op(blur_1on3);
		doScale1(src, srcStartY, srcEndY, srcWidth,
		         dst, dstStartY, dstEndY, op);
	} else {
		// No blurring: this is an optimization but it's also needed
		// for correctness (otherwise there's an overflow in 0.16 fixed
		// point arithmetic).
		PolyScale<Pixel, Scale_1on3<Pixel>> op;
		doScale1(src, srcStartY, srcEndY, srcWidth,
		         dst, dstStartY, dstEndY, op);
	}
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale1x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on3<Pixel>> op;
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale4x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on9<Pixel>> op(pixelOps);
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale4x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on9<Pixel>> op(pixelOps);
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale2x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel>> op(pixelOps);
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale2x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel>> op(pixelOps);
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale8x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on9<Pixel>> op(pixelOps);
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale8x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on9<Pixel>> op(pixelOps);
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale4x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel>> op(pixelOps);
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scale4x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel>> op(pixelOps);
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	int scanlineFactor = settings.getScanlineFactor();

	unsigned dstHeight = dst.getHeight();
	unsigned stopDstY = (dstEndY == dstHeight)
	                  ? dstEndY : dstEndY - 3;
	unsigned srcY = srcStartY, dstY = dstStartY;
	for (/* */; dstY < stopDstY; srcY += 1, dstY += 3) {
		auto color0 = src.getLineColor<Pixel>(srcY);
		Pixel color1 = scanline.darken(color0, scanlineFactor);
		dst.fillLine(dstY + 0, color0);
		dst.fillLine(dstY + 1, color0);
		dst.fillLine(dstY + 2, color1);
	}
	if (dstY != dstHeight) {
		unsigned nextLineWidth = src.getLineWidth(srcY + 1);
		assert(src.getLineWidth(srcY) == 1);
		assert(nextLineWidth != 1);
		this->dispatchScale(src, srcY, srcEndY, nextLineWidth,
		                    dst, dstY, dstEndY);
	}
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scaleBlank2to3(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	int scanlineFactor = settings.getScanlineFactor();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 3) {
		auto color0 = src.getLineColor<Pixel>(srcY + 0);
		auto color1 = src.getLineColor<Pixel>(srcY + 1);
		Pixel color01 = scanline.darken(color0, color1, scanlineFactor);
		dst.fillLine(dstY + 0, color0);
		dst.fillLine(dstY + 1, color01);
		dst.fillLine(dstY + 2, color1);
	}
}


// class Blur_1on3

template<std::unsigned_integral Pixel>
Blur_1on3<Pixel>::Blur_1on3(const PixelOperations<Pixel>& pixelOps)
	: mult0(pixelOps)
	, mult1(pixelOps)
	, mult2(pixelOps)
	, mult3(pixelOps)
{
}

#ifdef __SSE2__
template<std::unsigned_integral Pixel>
void Blur_1on3<Pixel>::blur_SSE(const Pixel* in_, Pixel* out_, size_t srcWidth)
{
	if constexpr (sizeof(Pixel) != 4) {
		assert(false); return; // only 32-bpp
	}

	assert((srcWidth % 4) == 0);
	assert(srcWidth >= 8);
	assert((size_t(in_ ) % 16) == 0);
	assert((size_t(out_) % 16) == 0);

	unsigned alpha = blur * 256;
	auto c0 = narrow_cast<int16_t>(alpha / 2);
	auto c1 = narrow_cast<int16_t>(alpha + c0);
	auto c2 = narrow_cast<int16_t>(0x10000 - c1);
	auto c3 = narrow_cast<int16_t>(0x10000 - alpha);
	__m128i C0C1 = _mm_set_epi16(c1, c1, c1, c1, c0, c0, c0, c0);
	__m128i C1C0 = _mm_shuffle_epi32(C0C1, 0x4E);
	__m128i C2C3 = _mm_set_epi16(c3, c3, c3, c3, c2, c2, c2, c2);
	__m128i C3C2 = _mm_shuffle_epi32(C2C3, 0x4E);

	size_t tmp = srcWidth - 4;
	const auto* in  = reinterpret_cast<const char*>(in_  +     tmp);
	      auto* out = reinterpret_cast<      char*>(out_ + 3 * tmp);
	auto x = -ptrdiff_t(tmp * sizeof(Pixel));

	__m128i ZERO = _mm_setzero_si128();

	// Prepare first iteration (duplicate left border pixel)
	__m128i abcd = _mm_load_si128(reinterpret_cast<const __m128i*>(in + x));
	__m128i a_b_ = _mm_unpacklo_epi8(abcd, ZERO);
	__m128i a_a_ = _mm_unpacklo_epi64(a_b_, a_b_);
	__m128i a0a1 = _mm_mulhi_epu16(a_a_, C0C1);
	__m128i d1d0 = _mm_shuffle_epi32(a0a1, 0x4E); // left border

	// At the start of each iteration the following vars are live:
	//   abcd, a_b_, a_a_, a0a1, d1d0
	// Each iteration reads 4 and produces 12 pixels.
	do {
		// p01
		__m128i a2a3  = _mm_mulhi_epu16(a_a_, C2C3);
		__m128i b_b_  = _mm_unpackhi_epi64(a_b_, a_b_);
		__m128i b1b0  = _mm_mulhi_epu16(b_b_, C1C0);
		__m128i xxb0  = _mm_unpackhi_epi64(ZERO, b1b0);
		__m128i p01   = _mm_add_epi16(_mm_add_epi16(d1d0, a2a3), xxb0);
		// p23
		__m128i xxa1  = _mm_unpackhi_epi64(ZERO, a0a1);
		__m128i b3b2  = _mm_mulhi_epu16(b_b_, C3C2);
		__m128i a2b2  = shuffle<0xE4>(a2a3, b3b2);
		__m128i b1xx  = _mm_unpacklo_epi64(b1b0, ZERO);
		__m128i p23   = _mm_add_epi16(_mm_add_epi16(xxa1, a2b2), b1xx);
		__m128i p0123 = _mm_packus_epi16(p01, p23);
		_mm_store_si128(reinterpret_cast<__m128i*>(out + 3 * x + 0),
		                p0123);

		// p45
		__m128i a0xx  = _mm_unpacklo_epi64(a0a1, ZERO);
		__m128i c_d_  = _mm_unpackhi_epi8(abcd, ZERO);
		__m128i c_c_  = _mm_unpacklo_epi64(c_d_, c_d_);
		__m128i c0c1  = _mm_mulhi_epu16(c_c_, C0C1);
		__m128i p45   = _mm_add_epi16(_mm_add_epi16(a0xx, b3b2), c0c1);
		// p67
		__m128i c2c3  = _mm_mulhi_epu16(c_c_, C2C3);
		__m128i d_d_  = _mm_unpackhi_epi64(c_d_, c_d_);
		        d1d0  = _mm_mulhi_epu16(d_d_, C1C0);
		__m128i xxd0  = _mm_unpackhi_epi64(ZERO, d1d0);
		__m128i p67   = _mm_add_epi16(_mm_add_epi16(b1b0, c2c3), xxd0);
		__m128i p4567 = _mm_packus_epi16(p45, p67);
		_mm_store_si128(reinterpret_cast<__m128i*>(out + 3 * x + 16),
		                p4567);

		// p89
		__m128i xxc1  = _mm_unpackhi_epi64(ZERO, c0c1);
		__m128i d3d2  = _mm_mulhi_epu16(d_d_, C3C2);
		__m128i c2d2  = shuffle<0xE4>(c2c3, d3d2);
		__m128i d1xx  = _mm_unpacklo_epi64(d1d0, ZERO);
		__m128i p89   = _mm_add_epi16(_mm_add_epi16(xxc1, c2d2), d1xx);
		// pab
		__m128i c0xx  = _mm_unpacklo_epi64(c0c1, ZERO);
		        abcd  = _mm_load_si128(reinterpret_cast<const __m128i*>(in + x + 16));
		        a_b_  = _mm_unpacklo_epi8(abcd, ZERO);
		        a_a_  = _mm_unpacklo_epi64(a_b_, a_b_);
		        a0a1  = _mm_mulhi_epu16(a_a_, C0C1);
		__m128i pab   = _mm_add_epi16(_mm_add_epi16(c0xx, d3d2), a0a1);
		__m128i p89ab = _mm_packus_epi16(p89, pab);
		_mm_store_si128(reinterpret_cast<__m128i*>(out + 3 * x + 32),
		                p89ab);

		x += 16;
	} while (x < 0);

	// Last iteration (duplicate right border pixel)
	// p01
	__m128i a2a3  = _mm_mulhi_epu16(a_a_, C2C3);
	__m128i b_b_  = _mm_unpackhi_epi64(a_b_, a_b_);
	__m128i b1b0  = _mm_mulhi_epu16(b_b_, C1C0);
	__m128i xxb0  = _mm_unpackhi_epi64(ZERO, b1b0);
	__m128i p01   = _mm_add_epi16(_mm_add_epi16(d1d0, a2a3), xxb0);
	// p23
	__m128i xxa1  = _mm_unpackhi_epi64(ZERO, a0a1);
	__m128i b3b2  = _mm_mulhi_epu16(b_b_, C3C2);
	__m128i a2b2  = shuffle<0xE4>(a2a3, b3b2);
	__m128i b1xx  = _mm_unpacklo_epi64(b1b0, ZERO);
	__m128i p23   = _mm_add_epi16(_mm_add_epi16(xxa1, a2b2), b1xx);
	__m128i p0123 = _mm_packus_epi16(p01, p23);
	_mm_store_si128(reinterpret_cast<__m128i*>(out + 0),
	                p0123);

	// p45
	__m128i a0xx  = _mm_unpacklo_epi64(a0a1, ZERO);
	__m128i c_d_  = _mm_unpackhi_epi8(abcd, ZERO);
	__m128i c_c_  = _mm_unpacklo_epi64(c_d_, c_d_);
	__m128i c0c1  = _mm_mulhi_epu16(c_c_, C0C1);
	__m128i p45   = _mm_add_epi16(_mm_add_epi16(a0xx, b3b2), c0c1);
	// p67
	__m128i c2c3  = _mm_mulhi_epu16(c_c_, C2C3);
	__m128i d_d_  = _mm_unpackhi_epi64(c_d_, c_d_);
		d1d0  = _mm_mulhi_epu16(d_d_, C1C0);
	__m128i xxd0  = _mm_unpackhi_epi64(ZERO, d1d0);
	__m128i p67   = _mm_add_epi16(_mm_add_epi16(b1b0, c2c3), xxd0);
	__m128i p4567 = _mm_packus_epi16(p45, p67);
	_mm_store_si128(reinterpret_cast<__m128i*>(out + 16),
	                p4567);

	// p89
	__m128i xxc1  = _mm_unpackhi_epi64(ZERO, c0c1);
	__m128i d3d2  = _mm_mulhi_epu16(d_d_, C3C2);
	__m128i c2d2  = shuffle<0xE4>(c2c3, d3d2);
	__m128i d1xx  = _mm_unpacklo_epi64(d1d0, ZERO);
	__m128i p89   = _mm_add_epi16(_mm_add_epi16(xxc1, c2d2), d1xx);
	// pab
	__m128i c0xx  = _mm_unpacklo_epi64(c0c1, ZERO);
	        a0a1  = _mm_shuffle_epi32(d1d0, 0x4E); // right border
	__m128i pab   = _mm_add_epi16(_mm_add_epi16(c0xx, d3d2), a0a1);
	__m128i p89ab = _mm_packus_epi16(p89, pab);
	_mm_store_si128(reinterpret_cast<__m128i*>(out + 32),
	                p89ab);
}
#endif

template<std::unsigned_integral Pixel>
void Blur_1on3<Pixel>::operator()(std::span<const Pixel> in, std::span<Pixel> out)
{
	/* The following code is equivalent to this loop. It is 2x unrolled
	 * and common subexpressions have been eliminated. The last iteration
	 * is also moved outside the for loop.
	 *
	 *  unsigned c0 = blur / 2;
	 *  unsigned c1 = c0 + blur;
	 *  unsigned c2 = 256 - c1;
	 *  unsigned c3 = 256 - 2 * c0;
	 *  Pixel prev, curr, next;
	 *  prev = curr = next = in[0];
	 *  size_t srcWidth = dstWidth / 3;
	 *  for (auto x : xrange(srcWidth)) {
	 *      if (x != (srcWidth - 1)) next = in[x + 1];
	 *      out[3 * x + 0] = mul(c1, prev) + mul(c2, curr);
	 *      out[3 * x + 1] = mul(c0, prev) + mul(c3, curr) + mul(c0, next);
	 *      out[3 * x + 2] =                 mul(c2, curr) + mul(c1, next);
	 *      prev = curr;
	 *      curr = next;
	 *  }
	 */
#ifdef __SSE2__
	if constexpr (sizeof(Pixel) == 4) {
		blur_SSE(in.data(), out.data(), in.size());
		return;
	}
#endif

	// C++ routine, both 16bpp and 32bpp
	unsigned c0 = blur / 2;
	unsigned c1 = blur + c0;
	unsigned c2 = 256 - c1;
	unsigned c3 = 256 - 2 * c0;
	mult0.setFactor32(c0);
	mult1.setFactor32(c1);
	mult2.setFactor32(c2);
	mult3.setFactor32(c3);

	Pixel p0 = in[0];
	Pixel p1;
	uint32_t f0 = mult0.mul32(p0);
	uint32_t f1 = mult1.mul32(p0);
	uint32_t g0 = f0;
	uint32_t g1 = f1;

	size_t srcWidth = in.size();
	size_t x = 0;
	for (/**/; x < (srcWidth - 2); x += 2) {
		uint32_t g2 = mult2.mul32(p0);
		out[3 * x + 0] = mult0.conv32(g2 + f1);
		p1 = in[x + 1];
		uint32_t t0 = mult0.mul32(p1);
		out[3 * x + 1] = mult0.conv32(f0 + mult3.mul32(p0) + t0);
		f0 = t0;
		f1 = mult1.mul32(p1);
		out[3 * x + 2] = mult0.conv32(g2 + f1);

		uint32_t f2 = mult2.mul32(p1);
		out[3 * x + 3] = mult0.conv32(f2 + g1);
		p0 = in[x + 2];
		uint32_t t1 = mult0.mul32(p0);
		out[3 * x + 4] = mult0.conv32(g0 + mult3.mul32(p1) + t1);
		g0 = t1;
		g1 = mult1.mul32(p0);
		out[3 * x + 5] = mult0.conv32(g1 + f2);
	}
	uint32_t g2 = mult2.mul32(p0);
	out[3 * x + 0] = mult0.conv32(g2 + f1);
	p1 = in[x + 1];
	uint32_t t0 = mult0.mul32(p1);
	out[3 * x + 1] = mult0.conv32(f0 + mult3.mul32(p0) + t0);
	f0 = t0;
	f1 = mult1.mul32(p1);
	out[3 * x + 2] = mult0.conv32(g2 + f1);

	uint32_t f2 = mult2.mul32(p1);
	out[3 * x + 3] = mult0.conv32(f2 + g1);
	out[3 * x + 4] = mult0.conv32(g0 + mult3.mul32(p1) + f0);
	out[3 * x + 5] = p1;
}

template<std::unsigned_integral Pixel>
void Simple3xScaler<Pixel>::scaleImage(
	FrameSource& src, const RawFrame* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (superImpose) {
		SuperImposedVideoFrame<Pixel> sf(src, *superImpose, pixelOps);
		srcWidth = sf.getLineWidth(srcStartY);
		this->dispatchScale(sf,  srcStartY, srcEndY, srcWidth,
		                    dst, dstStartY, dstEndY);
	} else {
		this->dispatchScale(src, srcStartY, srcEndY, srcWidth,
		                    dst, dstStartY, dstEndY);
	}
}

// Force template instantiation.
#if HAVE_16BPP
template class Simple3xScaler<uint16_t>;
#endif
#if HAVE_32BPP
template class Simple3xScaler<uint32_t>;
#endif

} // namespace openmsx
