/*
Original code: Copyright (C) 2001-2003 Andrea Mazzoleni
openMSX adaptation by Maarten ter Huurne

This file is based on code from the Scale2x project.
This modified version is licensed under GPL; the original code is dual-licensed
under GPL and under a custom license.

Visit the Scale2x site for info:
  http://scale2x.sourceforge.net/
*/

#include "Scale2xScaler.hh"
#include "FrameSource.hh"
#include "ScalerOutput.hh"
#include "unreachable.hh"
#include "vla.hh"
#include "xrange.hh"
#include <cassert>
#include <cstddef>
#include <cstdint>
#ifdef __SSE2__
#include "emmintrin.h" // SSE2
#ifdef __SSSE3__
#include "tmmintrin.h" // SSSE3  (supplemental SSE3)
#endif
#endif

namespace openmsx {

#ifdef __SSE2__

// Take an (unaligned) word from a certain position out of two adjacent
// (aligned) words. This either maps directly to the _mm_alignr_epi8()
// intrinsic or emulates that behavior.
template<int BYTES, int TMP = sizeof(__m128i) - BYTES>
[[nodiscard]] static inline __m128i align(__m128i high, __m128i low)
{
#ifdef __SSSE3__
	return _mm_alignr_epi8(high, low, BYTES);
#else
	return _mm_or_si128(
		_mm_slli_si128(high, TMP),
		_mm_srli_si128(low, BYTES));
#endif
}

// Select bits from either one of the two inputs depending on the value of the
// corresponding bit in a selection mask.
[[nodiscard]] static inline __m128i select(__m128i a0, __m128i a1, __m128i mask)
{
	// The traditional formula is:
	//   (a0 & ~mask) | (a1 & mask)
	// This can use the and-not instruction, so it's only 3 x86 asm
	// instructions. However this implementation uses the formula:
	//   ((a0 ^ a1) & mask) ^ a0
	// This also generates 3 instructions, but the advantage is that all
	// operations are commutative. This matters on 2-operand instruction
	// set like x86. In this particular case it results in better register
	// allocation and more common subexpression elimination.
	return _mm_xor_si128(_mm_and_si128(_mm_xor_si128(a0, a1), mask), a0);
}

// These three functions are abstracted to work either on 16bpp or 32bpp.
template<typename Pixel> [[nodiscard]] static inline __m128i isEqual(__m128i x, __m128i y)
{
	if constexpr (sizeof(Pixel) == 4) {
		return _mm_cmpeq_epi32(x, y);
	} else if constexpr (sizeof(Pixel) == 2) {
		return _mm_cmpeq_epi16(x, y);
	} else {
		UNREACHABLE;
	}
}
template<typename Pixel> [[nodiscard]] static inline __m128i unpacklo(__m128i x, __m128i y)
{
	if constexpr (sizeof(Pixel) == 4) {
		return _mm_unpacklo_epi32(x, y);
	} else if constexpr (sizeof(Pixel) == 2) {
		return _mm_unpacklo_epi16(x, y);
	} else {
		UNREACHABLE;
	}
}
template<typename Pixel> [[nodiscard]] static inline __m128i unpackhi(__m128i x, __m128i y)
{
	if constexpr (sizeof(Pixel) == 4) {
		return _mm_unpackhi_epi32(x, y);
	} else if constexpr (sizeof(Pixel) == 2) {
		return _mm_unpackhi_epi16(x, y);
	} else {
		UNREACHABLE;
	}
}

// Scale one 'unit'. A unit is 8x16bpp or 4x32bpp pixels.
template<typename Pixel, bool DOUBLE_X> static inline void scale1(
	__m128i top,	__m128i bottom,
	__m128i prev,	__m128i mid,	__m128i next,
	__m128i* out0,	__m128i* out1)
{
	__m128i left  = align<sizeof(__m128i) - sizeof(Pixel)>(mid, prev);
	__m128i right = align<                  sizeof(Pixel)>(next, mid);

	__m128i teqb = isEqual<Pixel>(top, bottom);
	__m128i leqt = isEqual<Pixel>(left, top);
	__m128i reqt = isEqual<Pixel>(right, top);
	__m128i leqb = isEqual<Pixel>(left, bottom);
	__m128i reqb = isEqual<Pixel>(right, bottom);

	__m128i cndA = _mm_andnot_si128(_mm_or_si128(teqb, reqt), leqt);
	__m128i cndB = _mm_andnot_si128(_mm_or_si128(teqb, leqt), reqt);
	__m128i cndC = _mm_andnot_si128(_mm_or_si128(teqb, reqb), leqb);
	__m128i cndD = _mm_andnot_si128(_mm_or_si128(teqb, leqb), reqb);

	__m128i a = select(mid, top,    cndA);
	__m128i b = select(mid, top,    cndB);
	__m128i c = select(mid, bottom, cndC);
	__m128i d = select(mid, bottom, cndD);

	if constexpr (DOUBLE_X) {
		out0[0] = unpacklo<Pixel>(a, b);
		out0[1] = unpackhi<Pixel>(a, b);
		out1[0] = unpacklo<Pixel>(c, d);
		out1[1] = unpackhi<Pixel>(c, d);
	} else {
		out0[0] = a;
		out1[0] = c;
	}
}

// Scale 1 input line (plus the line above and below) to 2 output lines,
// optionally doubling the amount of pixels within the output lines.
template<bool DOUBLE_X, typename Pixel,
         int SHIFT = sizeof(__m128i) - sizeof(Pixel)>
static inline void scaleSSE(
	      Pixel* __restrict out0_,  // top output line
	      Pixel* __restrict out1_,  // bottom output line
	const Pixel* __restrict in0_,   // top input line
	const Pixel* __restrict in1_,   // middle output line
	const Pixel* __restrict in2_,   // bottom output line
	size_t width)
{
	// Must be properly aligned.
	assert((reinterpret_cast<uintptr_t>(in0_ ) % sizeof(__m128i)) == 0);
	assert((reinterpret_cast<uintptr_t>(in1_ ) % sizeof(__m128i)) == 0);
	assert((reinterpret_cast<uintptr_t>(in2_ ) % sizeof(__m128i)) == 0);
	assert((reinterpret_cast<uintptr_t>(out0_) % sizeof(__m128i)) == 0);
	assert((reinterpret_cast<uintptr_t>(out1_) % sizeof(__m128i)) == 0);

	// Must be a (strict positive) multiple of 16 bytes.
	width *= sizeof(Pixel); // width in bytes
	assert((width % sizeof(__m128i)) == 0);
	assert(width > 1);
	width -= sizeof(__m128i); // handle last unit special

	constexpr size_t SCALE = DOUBLE_X ? 2 : 1;

	// Generated code seems more efficient when all address calculations
	// are done in bytes. Negative loop counter allows for a more efficient
	// loop-end test.
	const auto* in0  = reinterpret_cast<const char*>(in0_ ) +         width;
	const auto* in1  = reinterpret_cast<const char*>(in1_ ) +         width;
	const auto* in2  = reinterpret_cast<const char*>(in2_ ) +         width;
	      auto* out0 = reinterpret_cast<      char*>(out0_) + SCALE * width;
	      auto* out1 = reinterpret_cast<      char*>(out1_) + SCALE * width;
	ptrdiff_t x = -ptrdiff_t(width);

	// Setup for first unit
	__m128i next = *reinterpret_cast<const __m128i*>(in1 + x);
	__m128i mid = _mm_slli_si128(next, SHIFT);

	// Central units
	do {
		__m128i top    = *reinterpret_cast<const __m128i*>(in0 + x);
		__m128i bottom = *reinterpret_cast<const __m128i*>(in2 + x);
		__m128i prev = mid;
		mid = next;
		next = *reinterpret_cast<const __m128i*>(in1 + x + sizeof(__m128i));
		scale1<Pixel, DOUBLE_X>(top, bottom, prev, mid, next,
		                        reinterpret_cast<__m128i*>(out0 + SCALE * x),
		                        reinterpret_cast<__m128i*>(out1 + SCALE * x));
		x += sizeof(__m128i);
	} while (x < 0);
	assert(x == 0);

	// Last unit
	__m128i top    = *reinterpret_cast<const __m128i*>(in0);
	__m128i bottom = *reinterpret_cast<const __m128i*>(in2);
	__m128i prev = mid;
	mid = next;
	next = _mm_srli_si128(next, SHIFT);
	scale1<Pixel, DOUBLE_X>(top, bottom, prev, mid, next,
	                        reinterpret_cast<__m128i*>(out0),
	                        reinterpret_cast<__m128i*>(out1));
}

#endif


template<typename Pixel>
Scale2xScaler<Pixel>::Scale2xScaler(const PixelOperations<Pixel>& pixelOps_)
	: Scaler2<Pixel>(pixelOps_)
{
}

template<typename Pixel>
inline void Scale2xScaler<Pixel>::scaleLine_1on2(
	Pixel* __restrict dst0, Pixel* __restrict dst1,
	const Pixel* __restrict src0, const Pixel* __restrict src1,
	const Pixel* __restrict src2, size_t srcWidth) __restrict
{
	// For some reason, for the c++ version, processing the two output
	// lines separately is faster than merging them in a single loop (even
	// though a single loop only has to fetch the inputs once and can
	// eliminate some common sub-expressions). For the asm version the
	// situation is reversed.
#ifdef __SSE2__
	scaleSSE<true>(dst0, dst1, src0, src1, src2, srcWidth);
#else
	scaleLineHalf_1on2(dst0, src0, src1, src2, srcWidth);
	scaleLineHalf_1on2(dst1, src2, src1, src0, srcWidth);
#endif
}

template<typename Pixel>
void Scale2xScaler<Pixel>::scaleLineHalf_1on2(
	Pixel* __restrict dst, const Pixel* __restrict src0,
	const Pixel* __restrict src1, const Pixel* __restrict src2,
	size_t srcWidth) __restrict
{
	//   n      m is expanded to a b
	// w m e                     c d
	//   s         a = (w == n) && (s != n) && (e != n) ? n : m
	//             b =   .. swap w/e
	//             c =   .. swap n/s
	//             d =   .. swap w/e  n/s

	// First pixel.
	Pixel mid   = src1[0];
	Pixel right = src1[1];
	dst[0] = mid;
	dst[1] = (right == src0[0] && src2[0] != src0[0]) ? src0[0] : mid;

	// Central pixels.
	for (auto x : xrange(1u, srcWidth - 1)) {
		Pixel left = mid;
		mid   = right;
		right = src1[x + 1];
		Pixel top = src0[x];
		Pixel bot = src2[x];
		dst[2 * x + 0] = (left  == top && right != top && bot != top) ? top : mid;
		dst[2 * x + 1] = (right == top && left  != top && bot != top) ? top : mid;
	}

	// Last pixel.
	dst[2 * srcWidth - 2] =
		(mid == src0[srcWidth - 1] && src2[srcWidth - 1] != src0[srcWidth - 1])
		? src0[srcWidth - 1] : right;
	dst[2 * srcWidth - 1] =
		src1[srcWidth - 1];
}

template<typename Pixel>
inline void Scale2xScaler<Pixel>::scaleLine_1on1(
	Pixel* __restrict dst0, Pixel* __restrict dst1,
	const Pixel* __restrict src0, const Pixel* __restrict src1,
	const Pixel* __restrict src2, size_t srcWidth) __restrict
{
#ifdef __SSE2__
	scaleSSE<false>(dst0, dst1, src0, src1, src2, srcWidth);
#else
	scaleLineHalf_1on1(dst0, src0, src1, src2, srcWidth);
	scaleLineHalf_1on1(dst1, src2, src1, src0, srcWidth);
#endif
}

template<typename Pixel>
void Scale2xScaler<Pixel>::scaleLineHalf_1on1(
	Pixel* __restrict dst, const Pixel* __restrict src0,
	const Pixel* __restrict src1, const Pixel* __restrict src2,
	size_t srcWidth) __restrict
{
	//    ab ef
	// x0 12 34 5x
	//    cd gh

	// First pixel.
	Pixel mid =   src1[0];
	Pixel right = src1[1];
	dst[0] = mid;

	// Central pixels.
	for (auto x : xrange(1u, srcWidth - 1)) {
		Pixel left = mid;
		mid   = right;
		right = src1[x + 1];
		Pixel top = src0[x];
		Pixel bot = src2[x];
		dst[x] = (left == top && right != top && bot != top) ? top : mid;
	}

	// Last pixel.
	dst[srcWidth - 1] =
		(mid == src0[srcWidth - 1] && src2[srcWidth - 1] != src0[srcWidth - 1])
		? src0[srcWidth - 1] : right;
}

template<typename Pixel>
void Scale2xScaler<Pixel>::scale1x1to2x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	VLA_SSE_ALIGNED(Pixel, buf0_, srcWidth); auto* buf0 = buf0_;
	VLA_SSE_ALIGNED(Pixel, buf1_, srcWidth); auto* buf1 = buf1_;
	VLA_SSE_ALIGNED(Pixel, buf2_, srcWidth); auto* buf2 = buf2_;

	int srcY = srcStartY;
	auto* srcPrev = src.getLinePtr(srcY - 1, srcWidth, buf0);
	auto* srcCurr = src.getLinePtr(srcY + 0, srcWidth, buf1);

	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 2) {
		auto* srcNext = src.getLinePtr(srcY + 1, srcWidth, buf2);
		auto* dstUpper = dst.acquireLine(dstY + 0);
		auto* dstLower = dst.acquireLine(dstY + 1);
		scaleLine_1on2(dstUpper, dstLower,
		               srcPrev, srcCurr, srcNext,
		               srcWidth);
		dst.releaseLine(dstY + 0, dstUpper);
		dst.releaseLine(dstY + 1, dstLower);
		srcPrev = srcCurr;
		srcCurr = srcNext;
		std::swap(buf0, buf1);
		std::swap(buf1, buf2);
	}
}

template<typename Pixel>
void Scale2xScaler<Pixel>::scale1x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	VLA_SSE_ALIGNED(Pixel, buf0_, srcWidth); auto* buf0 = buf0_;
	VLA_SSE_ALIGNED(Pixel, buf1_, srcWidth); auto* buf1 = buf1_;
	VLA_SSE_ALIGNED(Pixel, buf2_, srcWidth); auto* buf2 = buf2_;

	int srcY = srcStartY;
	auto* srcPrev = src.getLinePtr(srcY - 1, srcWidth, buf0);
	auto* srcCurr = src.getLinePtr(srcY + 0, srcWidth, buf1);

	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 2) {
		auto* srcNext = src.getLinePtr(srcY + 1, srcWidth, buf2);
		auto* dstUpper = dst.acquireLine(dstY + 0);
		auto* dstLower = dst.acquireLine(dstY + 1);
		scaleLine_1on1(dstUpper, dstLower,
		               srcPrev, srcCurr, srcNext,
		               srcWidth);
		dst.releaseLine(dstY + 0, dstUpper);
		dst.releaseLine(dstY + 1, dstLower);
		srcPrev = srcCurr;
		srcCurr = srcNext;
		std::swap(buf0, buf1);
		std::swap(buf1, buf2);
	}
}

// Force template instantiation.
#if HAVE_16BPP
template class Scale2xScaler<uint16_t>;
#endif
#if HAVE_32BPP
template class Scale2xScaler<uint32_t>;
#endif

} // namespace openmsx
