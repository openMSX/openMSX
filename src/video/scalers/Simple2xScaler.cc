#include "Simple2xScaler.hh"
#include "SuperImposedVideoFrame.hh"
#include "LineScalers.hh"
#include "RawFrame.hh"
#include "ScalerOutput.hh"
#include "RenderSettings.hh"
#include "vla.hh"
#include <cassert>
#include <cstddef>
#include <cstdint>
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx {

// class Simple2xScaler

template<std::unsigned_integral Pixel>
Simple2xScaler<Pixel>::Simple2xScaler(
		const PixelOperations<Pixel>& pixelOps_,
		RenderSettings& renderSettings)
	: Scaler2<Pixel>(pixelOps_)
	, settings(renderSettings)
	, pixelOps(pixelOps_)
	, mult1(pixelOps)
	, mult2(pixelOps)
	, mult3(pixelOps)
	, scanline(pixelOps)
{
}

template<std::unsigned_integral Pixel>
void Simple2xScaler<Pixel>::scaleBlank1to2(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	int scanlineFactor = settings.getScanlineFactor();

	unsigned dstHeight = dst.getHeight();
	unsigned stopDstY = (dstEndY == dstHeight)
	                  ? dstEndY : dstEndY - 2;
	unsigned srcY = srcStartY, dstY = dstStartY;
	for (/* */; dstY < stopDstY; srcY += 1, dstY += 2) {
		auto color0 = src.getLineColor<Pixel>(srcY);
		dst.fillLine(dstY + 0, color0);
		Pixel color1 = scanline.darken(color0, scanlineFactor);
		dst.fillLine(dstY + 1, color1);
	}
	if (dstY != dstHeight) {
		unsigned nextLineWidth = src.getLineWidth(srcY + 1);
		assert(src.getLineWidth(srcY) == 1);
		assert(nextLineWidth != 1);
		this->dispatchScale(src, srcY, srcEndY, nextLineWidth,
		                    dst, dstY, dstEndY);
	}
}

#ifdef __SSE2__

// Combines upper-half of 'x' with lower half of 'y'.
static inline __m128i shuffle(__m128i x, __m128i y)
{
	// mm_shuffle_pd() actually shuffles 64-bit floating point values, we
	// need to shuffle integers. Though floats and ints are stored in the
	// same xmmN registers. So this instruction does the right thing.
	// However (some?) x86 CPUs keep the float and integer interpretations
	// of these registers in different physical locations in the chip and
	// there is some overhead on switching between these interpretations.
	// So the casts in the statement below don't generate any instructions,
	// but they still can cause overhead on (some?) CPUs.
	return _mm_castpd_si128(_mm_shuffle_pd(
		_mm_castsi128_pd(x), _mm_castsi128_pd(y), 1));
}

// 32bpp
static void blur1on2_SSE2(
	const uint32_t* __restrict in_, uint32_t* __restrict out_,
	unsigned c1_, unsigned c2_, size_t width)
{
	width *= sizeof(uint32_t); // in bytes
	assert(width >= (2 * sizeof(__m128i)));
	assert((reinterpret_cast<uintptr_t>(in_ ) % sizeof(__m128i)) == 0);
	assert((reinterpret_cast<uintptr_t>(out_) % sizeof(__m128i)) == 0);

	ptrdiff_t x = -ptrdiff_t(width - sizeof(__m128i));
	const auto* in  = reinterpret_cast<const char*>(in_ ) -     x;
	      auto* out = reinterpret_cast<      char*>(out_) - 2 * x;

	// Setup first iteration
	__m128i c1 = _mm_set1_epi16(c1_);
	__m128i c2 = _mm_set1_epi16(c2_);
	__m128i zero = _mm_setzero_si128();

	__m128i abcd = *reinterpret_cast<const __m128i*>(in);
	__m128i a0b0 = _mm_unpacklo_epi8(abcd, zero);
	__m128i d0a0 = _mm_shuffle_epi32(a0b0, 0x44);
	__m128i d1a1 = _mm_mullo_epi16(c1, d0a0);

	// Each iteration reads 4 pixels and generates 8 pixels
	do {
		// At the start of each iteration these variables are live:
		//   abcd, a0b0, d1a1
		__m128i c0d0 = _mm_unpackhi_epi8(abcd, zero);
		__m128i b0c0 = shuffle(a0b0, c0d0);
		__m128i a2b2 = _mm_mullo_epi16(c2, a0b0);
		__m128i b1c1 = _mm_mullo_epi16(c1, b0c0);
		__m128i daab = _mm_srli_epi16(_mm_add_epi16(d1a1, a2b2), 8);
		__m128i abbc = _mm_srli_epi16(_mm_add_epi16(a2b2, b1c1), 8);
		__m128i abab = _mm_packus_epi16(daab, abbc);
		*reinterpret_cast<__m128i*>(out + 2 * x) =
			_mm_shuffle_epi32(abab, 0xd8);
		abcd         = *reinterpret_cast<const __m128i*>(in + x + 16);
		a0b0         = _mm_unpacklo_epi8(abcd, zero);
		__m128i d0a0_= shuffle(c0d0, a0b0);
		__m128i c2d2 = _mm_mullo_epi16(c2, c0d0);
		d1a1         = _mm_mullo_epi16(c1, d0a0_);
		__m128i bccd = _mm_srli_epi16(_mm_add_epi16(b1c1, c2d2), 8);
		__m128i cdda = _mm_srli_epi16(_mm_add_epi16(c2d2, d1a1), 8);
		__m128i cdcd = _mm_packus_epi16(bccd, cdda);
		*reinterpret_cast<__m128i*>(out + 2 * x + 16) =
			_mm_shuffle_epi32(cdcd, 0xd8);
		x += 16;
	} while (x < 0);

	// Last iteration (because this doesn't need to read new input)
	__m128i c0d0 = _mm_unpackhi_epi8(abcd, zero);
	__m128i b0c0 = shuffle(a0b0, c0d0);
	__m128i a2b2 = _mm_mullo_epi16(c2, a0b0);
	__m128i b1c1 = _mm_mullo_epi16(c1, b0c0);
	__m128i daab = _mm_srli_epi16(_mm_add_epi16(d1a1, a2b2), 8);
	__m128i abbc = _mm_srli_epi16(_mm_add_epi16(a2b2, b1c1), 8);
	__m128i abab = _mm_packus_epi16(daab, abbc);
	*reinterpret_cast<__m128i*>(out) = _mm_shuffle_epi32(abab, 0xd8);
	__m128i d0d0 = _mm_shuffle_epi32(c0d0, 0xee);
	__m128i c2d2 = _mm_mullo_epi16(c2, c0d0);
	__m128i d1d1 = _mm_mullo_epi16(c1, d0d0);
	__m128i bccd = _mm_srli_epi16(_mm_add_epi16(b1c1, c2d2), 8);
	__m128i cddd = _mm_srli_epi16(_mm_add_epi16(c2d2, d1d1), 8);
	__m128i cdcd = _mm_packus_epi16(bccd, cddd);
	*reinterpret_cast<__m128i*>(out + 16) = _mm_shuffle_epi32(cdcd, 0xd8);
}

#endif

template<std::unsigned_integral Pixel>
void Simple2xScaler<Pixel>::blur1on2(
	std::span<const Pixel> in, std::span<Pixel> out, unsigned alpha)
{
	assert((2 * in.size()) == out.size());
	/* This routine is functionally equivalent to the following:
	 *
	 * void blur1on2(const Pixel* in, Pixel* out, unsigned alpha)
	 * {
	 *         unsigned c1 = alpha / 4;
	 *         unsigned c2 = 256 - c1;
	 *
	 *         Pixel prev, curr, next;
	 *         prev = curr = in[0];
	 *
	 *         unsigned x = 0;
	 *         for (; x < (srcWidth - 1); ++x) {
	 *                 out[2 * x + 0] = (c1 * prev + c2 * curr) >> 8;
	 *                 Pixel next = in[x + 1];
	 *                 out[2 * x + 1] = (c1 * next + c2 * curr) >> 8;
	 *                 prev = curr;
	 *                 curr = next;
	 *         }
	 *
	 *         out[2 * x + 0] = (c1 * prev + c2 * curr) >> 8;
	 *         next = curr;
	 *         out[2 * x + 1] = (c1 * next + c2 * curr) >> 8;
	 * }
	 */

	if (alpha == 0) {
		Scale_1on2<Pixel> scale;
		scale(in, out);
		return;
	}

	assert(alpha <= 256);
	unsigned c1 = alpha / 4;
	unsigned c2 = 256 - c1;

#ifdef __SSE2__
	if constexpr (sizeof(Pixel) == 4) {
		// SSE2, only 32bpp
		blur1on2_SSE2(in.data(), out.data(), c1, c2, in.size());
		return;
	}
#endif
	// C++ routine, both 16bpp and 32bpp.
	// The loop is 2x unrolled and all common subexpressions and redundant
	// assignments have been eliminated. 1 iteration generates 4 pixels.
	mult1.setFactor32(c1);
	mult2.setFactor32(c2);

	Pixel p0 = in[0];
	Pixel p1;
	unsigned f0 = mult1.mul32(p0);
	unsigned f1 = f0;
	unsigned tmp;

	size_t srcWidth = in.size();
	size_t x = 0;
	for (/**/; x < (srcWidth - 2); x += 2) {
		tmp = mult2.mul32(p0);
		out[2 * x + 0] = mult1.conv32(f1 + tmp);

		p1 = in[x + 1];
		f1 = mult1.mul32(p1);
		out[2 * x + 1] = mult1.conv32(f1 + tmp);

		tmp = mult2.mul32(p1);
		out[2 * x + 2] = mult1.conv32(f0 + tmp);

		p0 = in[x + 2];
		f0 = mult1.mul32(p0);
		out[2 * x + 3] = mult1.conv32(f0 + tmp);
	}

	tmp = mult2.mul32(p0);
	out[2 * x + 0] = mult1.conv32(f1 + tmp);

	p1 = in[x + 1];
	f1 = mult1.mul32(p1);
	out[2 * x + 1] = mult1.conv32(f1 + tmp);

	tmp = mult2.mul32(p1);
	out[2 * x + 2] = mult1.conv32(f0 + tmp);

	out[2 * x + 3] = p1;
}

#ifdef __SSE2__

// 32bpp
static void blur1on1_SSE2(
	const uint32_t* __restrict in_, uint32_t* __restrict out_,
	unsigned c1_, unsigned c2_, size_t width)
{
	width *= sizeof(uint32_t); // in bytes
	assert(width >= (2 * sizeof(__m128i)));
	assert((reinterpret_cast<uintptr_t>(in_ ) % sizeof(__m128i)) == 0);
	assert((reinterpret_cast<uintptr_t>(out_) % sizeof(__m128i)) == 0);

	ptrdiff_t x = -ptrdiff_t(width - sizeof(__m128i));
	const auto* in  = reinterpret_cast<const char*>(in_ ) - x;
	      auto* out = reinterpret_cast<      char*>(out_) - x;

	// Setup first iteration
	__m128i c1 = _mm_set1_epi16(c1_);
	__m128i c2 = _mm_set1_epi16(c2_);
	__m128i zero = _mm_setzero_si128();

	__m128i abcd = *reinterpret_cast<const __m128i*>(in);
	__m128i a0b0 = _mm_unpacklo_epi8(abcd, zero);
	__m128i d0a0 = _mm_shuffle_epi32(a0b0, 0x44);

	// Each iteration reads 4 pixels and generates 4 pixels
	do {
		// At the start of each iteration these variables are live:
		//   abcd, a0b0, d0a0
		__m128i c0d0 = _mm_unpackhi_epi8(abcd, zero);
		__m128i b0c0 = shuffle(a0b0, c0d0);
		__m128i a2b2 = _mm_mullo_epi16(c2, a0b0);
		__m128i dbac = _mm_mullo_epi16(c1, _mm_add_epi16(d0a0, b0c0));
		__m128i aabb = _mm_srli_epi16(_mm_add_epi16(dbac, a2b2), 8);
		abcd         = *reinterpret_cast<const __m128i*>(in + x + 16);
		a0b0         = _mm_unpacklo_epi8(abcd, zero);
		d0a0         = shuffle(c0d0, a0b0);
		__m128i c2d2 = _mm_mullo_epi16(c2, c0d0);
		__m128i bdca = _mm_mullo_epi16(c1, _mm_add_epi16(b0c0, d0a0));
		__m128i ccdd = _mm_srli_epi16(_mm_add_epi16(bdca, c2d2), 8);
		*reinterpret_cast<__m128i*>(out + x) =
			_mm_packus_epi16(aabb, ccdd);
		x += 16;
	} while (x < 0);

	// Last iteration (because this doesn't need to read new input)
	__m128i c0d0 = _mm_unpackhi_epi8(abcd, zero);
	__m128i b0c0 = shuffle(a0b0, c0d0);
	__m128i a2b2 = _mm_mullo_epi16(c2, a0b0);
	__m128i dbac = _mm_mullo_epi16(c1, _mm_add_epi16(d0a0, b0c0));
	__m128i aabb = _mm_srli_epi16(_mm_add_epi16(dbac, a2b2), 8);
	__m128i d0d0 = _mm_shuffle_epi32(c0d0, 0xee);
	__m128i c2d2 = _mm_mullo_epi16(c2, c0d0);
	__m128i bdcd = _mm_mullo_epi16(c1, _mm_add_epi16(b0c0, d0d0));
	__m128i ccdd = _mm_srli_epi16(_mm_add_epi16(bdcd, c2d2), 8);
	*reinterpret_cast<__m128i*>(out) = _mm_packus_epi16(aabb, ccdd);
}

#endif
template<std::unsigned_integral Pixel>
void Simple2xScaler<Pixel>::blur1on1(
	std::span<const Pixel> in, std::span<Pixel> out, unsigned alpha)
{
	/* This routine is functionally equivalent to the following:
	 *
	 * void blur1on1(const Pixel* in, Pixel* out, unsigned alpha)
	 * {
	 *         unsigned c1 = alpha / 4;
	 *         unsigned c2 = 256 - alpha / 2;
	 *
	 *         Pixel prev, curr, next;
	 *         prev = curr = in[0];
	 *
	 *         unsigned x = 0;
	 *         for (; x < (srcWidth - 1); ++x) {
	 *                 next = in[x + 1];
	 *                 out[x] = (c1 * prev + c2 * curr + c1 * next) >> 8;
	 *                 prev = curr;
	 *                 curr = next;
	 *         }
	 *
	 *         next = curr;
	 *         out[x] = c1 * prev + c2 * curr + c1 * next;
	 * }
	 */

	if (alpha == 0) {
		Scale_1on1<Pixel> copy;
		copy(in, out);
		return;
	}

	unsigned c1 = alpha / 4;
	unsigned c2 = 256 - alpha / 2;

#ifdef __SSE2__
	if constexpr (sizeof(Pixel) == 4) {
		// SSE2, only 32bpp
		blur1on1_SSE2(in.data(), out.data(), c1, c2, in.size());
		return;
	}
#endif
	// C++ routine, both 16bpp and 32bpp.
	// The loop is 2x unrolled and all common subexpressions and redundant
	// assignments have been eliminated. 1 iteration generates 2 pixels.
	mult1.setFactor32(c1);
	mult3.setFactor32(c2);

	Pixel p0 = in[0];
	Pixel p1;
	unsigned f0 = mult1.mul32(p0);
	unsigned f1 = f0;

	size_t srcWidth = in.size();
	size_t x = 0;
	for (/**/; x < (srcWidth - 2); x += 2) {
		p1 = in[x + 1];
		unsigned t0 = mult1.mul32(p1);
		out[x] = mult1.conv32(f0 + mult3.mul32(p0) + t0);
		f0 = t0;

		p0 = in[x + 2];
		unsigned t1 = mult1.mul32(p0);
		out[x + 1] = mult1.conv32(f1 + mult3.mul32(p1) + t1);
		f1 = t1;
	}

	p1 = in[x + 1];
	unsigned t0 = mult1.mul32(p1);
	out[x] = mult1.conv32(f0 + mult3.mul32(p0) + t0);

	out[x + 1] = mult1.conv32(f1 + mult3.mul32(p1) + t0);
}

template<std::unsigned_integral Pixel>
void Simple2xScaler<Pixel>::drawScanline(
		std::span<const Pixel> in1, std::span<const Pixel> in2, std::span<Pixel> out, int factor)
{
	if (factor != 255) {
		scanline.draw(in1, in2, out, factor);
	} else {
		Scale_1on1<Pixel> scale;
		scale(in1, out);
	}
}

template<std::unsigned_integral Pixel>
void Simple2xScaler<Pixel>::scale1x1to2x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	VLA_SSE_ALIGNED(Pixel, buf, srcWidth);
	int blur = settings.getBlurFactor();
	int scanlineFactor = settings.getScanlineFactor();
	size_t dstWidth = 2 * srcWidth;

	unsigned dstY = dstStartY;
	auto srcLine = src.getLine(srcStartY++, buf);
	auto dstLine0 = dst.acquireLine(dstY + 0);
	blur1on2(srcLine, dstLine0, blur);

	for (/**/; dstY < dstEndY - 2; dstY += 2) {
		srcLine = src.getLine(srcStartY++, buf);
		auto dstLine2 = dst.acquireLine(dstY + 2);
		blur1on2(srcLine, dstLine2, blur);

		auto dstLine1 = dst.acquireLine(dstY + 1);
		drawScanline(dstLine0, dstLine2, dstLine1, scanlineFactor);

		dst.releaseLine(dstY + 0, dstLine0);
		dst.releaseLine(dstY + 1, dstLine1);
		dstLine0 = dstLine2;
	}

	srcLine = src.getLine(srcStartY++, buf);
	VLA_SSE_ALIGNED(Pixel, buf2, dstWidth);
	blur1on2(srcLine, buf2, blur);

	auto dstLine1 = dst.acquireLine(dstY + 1);
	drawScanline(dstLine0, buf2, dstLine1, scanlineFactor);
	dst.releaseLine(dstY + 0, dstLine0);
	dst.releaseLine(dstY + 1, dstLine1);
}

template<std::unsigned_integral Pixel>
void Simple2xScaler<Pixel>::scale1x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	VLA_SSE_ALIGNED(Pixel, buf, srcWidth);
	int blur = settings.getBlurFactor();
	int scanlineFactor = settings.getScanlineFactor();

	unsigned dstY = dstStartY;
	auto srcLine = src.getLine(srcStartY++, buf);
	auto dstLine0 = dst.acquireLine(dstY);
	blur1on1(srcLine, dstLine0, blur);

	for (/**/; dstY < dstEndY - 2; dstY += 2) {
		srcLine = src.getLine(srcStartY++, buf);
		auto dstLine2 = dst.acquireLine(dstY + 2);
		blur1on1(srcLine, dstLine2, blur);

		auto dstLine1 = dst.acquireLine(dstY + 1);
		drawScanline(dstLine0, dstLine2, dstLine1, scanlineFactor);

		dst.releaseLine(dstY + 0, dstLine0);
		dst.releaseLine(dstY + 1, dstLine1);
		dstLine0 = dstLine2;
	}

	srcLine = src.getLine(srcStartY++, buf);
	VLA_SSE_ALIGNED(Pixel, buf2, srcWidth);
	blur1on1(srcLine, buf2, blur);

	auto dstLine1 = dst.acquireLine(dstY + 1);
	drawScanline(dstLine0, buf2, dstLine1, scanlineFactor);
	dst.releaseLine(dstY + 0, dstLine0);
	dst.releaseLine(dstY + 1, dstLine1);
}

template<std::unsigned_integral Pixel>
void Simple2xScaler<Pixel>::scaleImage(
	FrameSource& src, const RawFrame* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (superImpose) {
		// Note: this implementation is different from the openGL
		// version. Here we first alpha-blend and then scale, so the
		// video layer will also get blurred (and possibly down-scaled
		// to MSX resolution). The openGL version will only blur the
		// MSX frame, then blend with the video frame and then apply
		// scanlines. I think the openGL version is visually slightly
		// better, but much more work to implement in software (in
		// openGL shaders it's very easy). Maybe we can improve this
		// later (if required at all).
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
template class Simple2xScaler<uint16_t>;
#endif
#if HAVE_32BPP
template class Simple2xScaler<uint32_t>;
#endif

} // namespace openmsx
