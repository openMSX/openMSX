#ifndef LINESCALERS_HH
#define LINESCALERS_HH

#include "PixelOperations.hh"
#include "ranges.hh"
#include "view.hh"
#include "xrange.hh"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#ifdef __SSE2__
#include "emmintrin.h"
#endif

namespace openmsx {

using Pixel = uint32_t;

/**  Scale_XonY functions
 * Transforms an input line of pixel to an output line (possibly) with
 * a different width. X input pixels are mapped on Y output pixels.
 * @param in Input line
 * @param out Output line
 */
void scale_1on3(std::span<const Pixel> in, std::span<Pixel> out);
void scale_1on4(std::span<const Pixel> in, std::span<Pixel> out);
void scale_1on6(std::span<const Pixel> in, std::span<Pixel> out);
void Scale_1on2(std::span<const Pixel> in, std::span<Pixel> out);
void scale_2on1(std::span<const Pixel> in, std::span<Pixel> out);
void scale_6on1(std::span<const Pixel> in, std::span<Pixel> out);
void scale_4on1(std::span<const Pixel> in, std::span<Pixel> out);
void scale_3on1(std::span<const Pixel> in, std::span<Pixel> out);
void scale_3on2(std::span<const Pixel> in, std::span<Pixel> out);
void scale_3on4(std::span<const Pixel> in, std::span<Pixel> out);
void scale_3on8(std::span<const Pixel> in, std::span<Pixel> out);
void scale_2on3(std::span<const Pixel> in, std::span<Pixel> out);
void scale_4on3(std::span<const Pixel> in, std::span<Pixel> out);
void scale_8on3(std::span<const Pixel> in, std::span<Pixel> out);
void scale_2on9(std::span<const Pixel> in, std::span<Pixel> out);
void scale_4on9(std::span<const Pixel> in, std::span<Pixel> out);
void scale_8on9(std::span<const Pixel> in, std::span<Pixel> out);
void scale_4on5(std::span<const Pixel> in, std::span<Pixel> out);
void scale_7on8(std::span<const Pixel> in, std::span<Pixel> out);
void scale_9on10(std::span<const Pixel> in, std::span<Pixel> out);
void scale_17on20(std::span<const Pixel> in, std::span<Pixel> out);

/**  BlendLines functor
 * Generate an output line that is an interpolation of two input lines.
 * @param in1 First input line
 * @param in2 Second input line
 * @param out Output line
 * @param width Width of the lines in pixels
 */
template<unsigned w1 = 1, unsigned w2 = 1>
void blendLines(std::span<const Pixel> in1, std::span<const Pixel> in2,
                std::span<Pixel> out);

/**  AlphaBlendLines functor
 * Generate an output line that is a per-pixel-alpha-blend of the two input
 * lines. The first input line contains the alpha-value per pixel.
 * @param in1 First input line
 * @param in2 Second input line
 * @param out Output line
 */
void alphaBlendLines(std::span<const Pixel> in1, std::span<const Pixel> in2,
                     std::span<Pixel> out);
void alphaBlendLines(Pixel in1, std::span<const Pixel> in2,
                     std::span<Pixel> out);


// implementation

template<unsigned N>
static inline void scale_1onN(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	auto outWidth = out.size();
	assert(in.size() == (outWidth / N));

	size_t i = 0, j = 0;
	for (/* */; i < (outWidth - (N - 1)); i += N, j += 1) {
		Pixel pix = in[j];
		for (auto k : xrange(N)) {
			out[i + k] = pix;
		}
	}
	for (auto k : xrange(N - 1)) {
		if ((i + k) < outWidth) out[i + k] = 0;
	}
}

inline void scale_1on3(std::span<const Pixel> in, std::span<Pixel> out)
{
	scale_1onN<3>(in, out);
}

inline void scale_1on4(std::span<const Pixel> in, std::span<Pixel> out)
{
	scale_1onN<4>(in, out);
}

inline void scale_1on6(std::span<const Pixel> in, std::span<Pixel> out)
{
	scale_1onN<6>(in, out);
}

#ifdef __SSE2__
inline __m128i unpacklo(__m128i x, __m128i y)
{
	// 32bpp
	return _mm_unpacklo_epi32(x, y);
}
inline __m128i unpackhi(__m128i x, __m128i y)
{
	// 32bpp
	return _mm_unpackhi_epi32(x, y);
}

inline void scale_1on2_SSE(const Pixel* __restrict in_, Pixel* __restrict out_, size_t srcWidth)
{
	size_t bytes = srcWidth * sizeof(Pixel);
	assert((bytes % (4 * sizeof(__m128i))) == 0);
	assert(bytes != 0);

	const auto* in  = reinterpret_cast<const char*>(in_)  +     bytes;
	      auto* out = reinterpret_cast<      char*>(out_) + 2 * bytes;

	auto x = -ptrdiff_t(bytes);
	do {
		__m128i a0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + x +  0));
		__m128i a1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + x + 16));
		__m128i a2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + x + 32));
		__m128i a3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + x + 48));
		__m128i l0 = unpacklo(a0, a0);
		__m128i h0 = unpackhi(a0, a0);
		__m128i l1 = unpacklo(a1, a1);
		__m128i h1 = unpackhi(a1, a1);
		__m128i l2 = unpacklo(a2, a2);
		__m128i h2 = unpackhi(a2, a2);
		__m128i l3 = unpacklo(a3, a3);
		__m128i h3 = unpackhi(a3, a3);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +   0), l0);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  16), h0);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  32), l1);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  48), h1);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  64), l2);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  80), h2);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  96), l3);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x + 112), h3);
		x += 4 * sizeof(__m128i);
	} while (x < 0);
}
#endif

inline void scale_1on2(std::span<const Pixel> in, std::span<Pixel> out)
{
	// This is a fairly simple algorithm (output each input pixel twice).
	// An ideal compiler should generate optimal (vector) code for it.
	// I checked the 2013-05-29 dev snapshots of gcc-4.9 and clang-3.4:
	// - Clang is not able to vectorize this loop. My best tuned C version
	//   of this routine is a little over 4x slower than the tuned
	//   SSE-intrinsics version.
	// - Gcc can auto-vectorize this routine. Though my best tuned version
	//   (I mean tuned to further improve the auto-vectorization, including
	//   using the new __builtin_assume_aligned() intrinsic) still runs
	//   approx 40% slower than the intrinsics version.
	// Hopefully in some years the compilers have improved further so that
	// the intrinsic version is no longer needed.
	auto srcWidth = in.size();
	assert(out.size() == 2 * srcWidth);

#ifdef __SSE2__
	size_t chunk = 4 * sizeof(__m128i) / sizeof(Pixel);
	size_t srcWidth2 = srcWidth & ~(chunk - 1);
	scale_1on2_SSE(in.data(), out.data(), srcWidth2);
	in  = in .subspan(    srcWidth2);
	out = out.subspan(2 * srcWidth2);
	srcWidth -= srcWidth2;
#endif

	// C++ version. Used both on non-x86 machines and (possibly) on x86 for
	// the last few pixels of the line.
	for (auto x : xrange(srcWidth)) {
		out[x * 2] = out[x * 2 + 1] = in[x];
	}
}

#ifdef __SSE2__
template<int IMM8> static inline __m128i shuffle(__m128i x, __m128i y)
{
	return _mm_castps_si128(_mm_shuffle_ps(
		_mm_castsi128_ps(x), _mm_castsi128_ps(y), IMM8));
}

inline __m128i blend(__m128i x, __m128i y)
{
	// 32bpp
	__m128i p = shuffle<0x88>(x, y);
	__m128i q = shuffle<0xDD>(x, y);
	return _mm_avg_epu8(p, q);
}

inline void scale_2on1_SSE(
	const Pixel* __restrict in_, Pixel* __restrict out_, size_t dstBytes)
{
	assert((dstBytes % (4 * sizeof(__m128i))) == 0);
	assert(dstBytes != 0);

	const auto* in  = reinterpret_cast<const char*>(in_)  + 2 * dstBytes;
	      auto* out = reinterpret_cast<      char*>(out_) +     dstBytes;

	auto x = -ptrdiff_t(dstBytes);
	do {
		__m128i a0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +   0));
		__m128i a1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  16));
		__m128i a2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  32));
		__m128i a3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  48));
		__m128i a4 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  64));
		__m128i a5 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  80));
		__m128i a6 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  96));
		__m128i a7 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x + 112));
		__m128i b0 = blend(a0, a1);
		__m128i b1 = blend(a2, a3);
		__m128i b2 = blend(a4, a5);
		__m128i b3 = blend(a6, a7);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + x +  0), b0);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + x + 16), b1);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + x + 32), b2);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + x + 48), b3);
		x += 4 * sizeof(__m128i);
	} while (x < 0);
}
#endif

inline void scale_2on1(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert(in.size() == 2 * out.size());
	auto outWidth = out.size();
#ifdef __SSE2__
	auto n64 = (outWidth * sizeof(Pixel)) & ~63;
	scale_2on1_SSE(in.data(), out.data(), n64); // process 64 byte chunks
	outWidth &= ((64 / sizeof(Pixel)) - 1); // remaining pixels (if any)
	if (outWidth == 0) [[likely]] return;
	in  = in .subspan(2 * n64 / sizeof(Pixel));
	out = out.subspan(    n64 / sizeof(Pixel));
	// fallthrough to c++ version
#endif
	// pure C++ version
	PixelOperations pixelOps;
	for (auto i : xrange(outWidth)) {
		out[i] = pixelOps.template blend<1, 1>(
			in[2 * i + 0], in[2 * i + 1]);
	}
}

inline void scale_6on1(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert(in.size() == 6 * out.size());
	PixelOperations pixelOps;
	for (auto i : xrange(out.size())) {
		out[i] = pixelOps.template blend<1, 1, 1, 1, 1, 1>(subspan<6>(in, 6 * i));
	}
}

inline void scale_4on1(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert(in.size() == 4 * out.size());
	PixelOperations pixelOps;
	for (auto i : xrange(out.size())) {
		out[i] = pixelOps.template blend<1, 1, 1, 1>(subspan<4>(in, 4 * i));
	}
}

inline void scale_3on1(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert(in.size() == 3 * out.size());
	PixelOperations pixelOps;
	for (auto i : xrange(out.size())) {
		out[i] = pixelOps.template blend<1, 1, 1>(subspan<3>(in, 3 * i));
	}
}

inline void scale_3on2(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 3) == (out.size() / 2));
	PixelOperations pixelOps;
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 1); i += 2, j += 3) {
		out[i + 0] = pixelOps.template blend<2, 1>(subspan<2>(in, j + 0));
		out[i + 1] = pixelOps.template blend<1, 2>(subspan<2>(in, j + 1));
	}
	if (i < n) out[i] = 0;
}

inline void scale_3on4(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 3) == (out.size() / 4));
	PixelOperations pixelOps;
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 3); i += 4, j += 3) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 2>(subspan<2>(in, j + 0));
		out[i + 2] = pixelOps.template blend<2, 1>(subspan<2>(in, j + 1));
		out[i + 3] = in[j + 2];
	}
	for (auto k : xrange(4 - 1)) {
		if ((i + k) < n) out[i + k] = 0;
	}
}

inline void scale_3on8(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 3) == (out.size() / 8));
	PixelOperations pixelOps;
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 7); i += 8, j += 3) {
		out[i + 0] = in[j + 0];
		out[i + 1] = in[j + 0];
		out[i + 2] = pixelOps.template blend<2, 1>(subspan<2>(in, j + 0));
		out[i + 3] = in[j + 1];
		out[i + 4] = in[j + 1];
		out[i + 5] = pixelOps.template blend<1, 2>(subspan<2>(in, j + 1));
		out[i + 6] = in[j + 2];
		out[i + 7] = in[j + 2];
	}
	for (auto k : xrange(8 - 1)) {
		if ((i + k) < n) out[i + k] = 0;
	}
}

inline void scale_2on3(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 2) == (out.size() / 3));
	PixelOperations pixelOps;
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 2); i += 3, j += 2) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 1>(subspan<2>(in, j));
		out[i + 2] = in[j + 1];
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
}

inline void scale_4on3(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 4) == (out.size() / 3));
	PixelOperations pixelOps;
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 2); i += 3, j += 4) {
		out[i + 0] = pixelOps.template blend<3, 1>(subspan<2>(in, j + 0));
		out[i + 1] = pixelOps.template blend<1, 1>(subspan<2>(in, j + 1));
		out[i + 2] = pixelOps.template blend<1, 3>(subspan<2>(in, j + 2));
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
}

inline void scale_8on3(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 8) == (out.size() / 3));
	PixelOperations pixelOps;
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 2); i += 3, j += 8) {
		out[i + 0] = pixelOps.template blend<3, 3, 2>   (subspan<3>(in, j + 0));
		out[i + 1] = pixelOps.template blend<1, 3, 3, 1>(subspan<4>(in, j + 2));
		out[i + 2] = pixelOps.template blend<2, 3, 3>   (subspan<3>(in, j + 5));
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
}

inline void scale_2on9(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 2) == (out.size() / 9));
	PixelOperations pixelOps;
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 8); i += 9, j += 2) {
		out[i + 0] = in[j + 0];
		out[i + 1] = in[j + 0];
		out[i + 2] = in[j + 0];
		out[i + 3] = in[j + 0];
		out[i + 4] = pixelOps.template blend<1, 1>(subspan<2>(in, j));
		out[i + 5] = in[j + 1];
		out[i + 6] = in[j + 1];
		out[i + 7] = in[j + 1];
		out[i + 8] = in[j + 1];
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
	if ((i + 2) < n) out[i + 2] = 0;
	if ((i + 3) < n) out[i + 3] = 0;
	if ((i + 4) < n) out[i + 4] = 0;
	if ((i + 5) < n) out[i + 5] = 0;
	if ((i + 6) < n) out[i + 6] = 0;
	if ((i + 7) < n) out[i + 7] = 0;
}

inline void scale_4on9(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 4) == (out.size() / 9));
	PixelOperations pixelOps;
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 8); i += 9, j += 4) {
		out[i + 0] = in[j + 0];
		out[i + 1] = in[j + 0];
		out[i + 2] = pixelOps.template blend<1, 3>(subspan<2>(in, j + 0));
		out[i + 3] = in[j + 1];
		out[i + 4] = pixelOps.template blend<1, 1>(subspan<2>(in, j + 1));
		out[i + 5] = in[j + 2];
		out[i + 6] = pixelOps.template blend<3, 1>(subspan<2>(in, j + 2));
		out[i + 7] = in[j + 3];
		out[i + 8] = in[j + 3];
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
	if ((i + 2) < n) out[i + 2] = 0;
	if ((i + 3) < n) out[i + 3] = 0;
	if ((i + 4) < n) out[i + 4] = 0;
	if ((i + 5) < n) out[i + 5] = 0;
	if ((i + 6) < n) out[i + 6] = 0;
	if ((i + 7) < n) out[i + 7] = 0;
}

inline void scale_8on9(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 8) == (out.size() / 9));
	PixelOperations pixelOps;
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 8); i += 9, j += 8) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 7>(subspan<2>(in, j + 0));
		out[i + 2] = pixelOps.template blend<1, 3>(subspan<2>(in, j + 1));
		out[i + 3] = pixelOps.template blend<3, 5>(subspan<2>(in, j + 2));
		out[i + 4] = pixelOps.template blend<1, 1>(subspan<2>(in, j + 3));
		out[i + 5] = pixelOps.template blend<5, 3>(subspan<2>(in, j + 4));
		out[i + 6] = pixelOps.template blend<3, 1>(subspan<2>(in, j + 5));
		out[i + 7] = pixelOps.template blend<7, 1>(subspan<2>(in, j + 6));
		out[i + 8] = in[j + 7];
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
	if ((i + 2) < n) out[i + 2] = 0;
	if ((i + 3) < n) out[i + 3] = 0;
	if ((i + 4) < n) out[i + 4] = 0;
	if ((i + 5) < n) out[i + 5] = 0;
	if ((i + 6) < n) out[i + 6] = 0;
	if ((i + 7) < n) out[i + 7] = 0;
}

inline void scale_4on5(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 4) == (out.size() / 5));
	PixelOperations pixelOps;
	size_t n = out.size();
	assert((n % 5) == 0);
	for (size_t i = 0, j = 0; i < n; i += 5, j += 4) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 3>(subspan<2>(in, j + 0));
		out[i + 2] = pixelOps.template blend<1, 1>(subspan<2>(in, j + 1));
		out[i + 3] = pixelOps.template blend<3, 1>(subspan<2>(in, j + 2));
		out[i + 4] = in[j + 3];
	}
}

inline void scale_7on8(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 7) == (out.size() / 8));
	PixelOperations pixelOps;
	size_t n = out.size();
	assert((n % 8) == 0);
	for (size_t i = 0, j = 0; i < n; i += 8, j += 7) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 6>(subspan<2>(in, j + 0));
		out[i + 2] = pixelOps.template blend<2, 5>(subspan<2>(in, j + 1));
		out[i + 3] = pixelOps.template blend<3, 4>(subspan<2>(in, j + 2));
		out[i + 4] = pixelOps.template blend<4, 3>(subspan<2>(in, j + 3));
		out[i + 5] = pixelOps.template blend<5, 2>(subspan<2>(in, j + 4));
		out[i + 6] = pixelOps.template blend<6, 1>(subspan<2>(in, j + 5));
		out[i + 7] = in[j + 6];
	}
}

inline void scale_17on20(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 17) == (out.size() / 20));
	PixelOperations pixelOps;
	size_t n = out.size();
	assert((n % 20) == 0);
	for (size_t i = 0, j = 0; i < n; i += 20, j += 17) {
		out[i +  0] = in[j +  0];
		out[i +  1] = pixelOps.template blend< 3, 14>(subspan<2>(in, j +  0));
		out[i +  2] = pixelOps.template blend< 6, 11>(subspan<2>(in, j +  1));
		out[i +  3] = pixelOps.template blend< 9,  8>(subspan<2>(in, j +  2));
		out[i +  4] = pixelOps.template blend<12,  5>(subspan<2>(in, j +  3));
		out[i +  5] = pixelOps.template blend<15,  2>(subspan<2>(in, j +  4));
		out[i +  6] = in[j +  5];
		out[i +  7] = pixelOps.template blend< 1, 16>(subspan<2>(in, j +  5));
		out[i +  8] = pixelOps.template blend< 4, 13>(subspan<2>(in, j +  6));
		out[i +  9] = pixelOps.template blend< 7, 10>(subspan<2>(in, j +  7));
		out[i + 10] = pixelOps.template blend<10,  7>(subspan<2>(in, j +  8));
		out[i + 11] = pixelOps.template blend<13,  4>(subspan<2>(in, j +  9));
		out[i + 12] = pixelOps.template blend<16,  1>(subspan<2>(in, j + 10));
		out[i + 13] = in[j + 11];
		out[i + 14] = pixelOps.template blend< 2, 15>(subspan<2>(in, j + 11));
		out[i + 15] = pixelOps.template blend< 5, 12>(subspan<2>(in, j + 12));
		out[i + 16] = pixelOps.template blend< 8,  9>(subspan<2>(in, j + 13));
		out[i + 17] = pixelOps.template blend<11,  6>(subspan<2>(in, j + 14));
		out[i + 18] = pixelOps.template blend<14,  3>(subspan<2>(in, j + 15));
		out[i + 19] = in[j + 16];
	}
}

inline void scale_9on10(std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 9) == (out.size() / 10));
	PixelOperations pixelOps;
	size_t n = out.size();
	assert((n % 10) == 0);
	for (size_t i = 0, j = 0; i < n; i += 10, j += 9) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 8>(subspan<2>(in, j + 0));
		out[i + 2] = pixelOps.template blend<2, 7>(subspan<2>(in, j + 1));
		out[i + 3] = pixelOps.template blend<3, 6>(subspan<2>(in, j + 2));
		out[i + 4] = pixelOps.template blend<4, 5>(subspan<2>(in, j + 3));
		out[i + 5] = pixelOps.template blend<5, 4>(subspan<2>(in, j + 4));
		out[i + 6] = pixelOps.template blend<6, 3>(subspan<2>(in, j + 5));
		out[i + 7] = pixelOps.template blend<7, 2>(subspan<2>(in, j + 6));
		out[i + 8] = pixelOps.template blend<8, 1>(subspan<2>(in, j + 7));
		out[i + 9] = in[j + 8];
	}
}

template<unsigned w1, unsigned w2>
void blendLines(std::span<const Pixel> in1, std::span<const Pixel> in2, std::span<Pixel> out)
{
	// It _IS_ allowed that the output is the same as one of the inputs.
	// TODO SSE optimizations
	// pure C++ version
	assert(in1.size() == in2.size());
	assert(in1.size() == out.size());
	PixelOperations pixelOps;
	for (auto [i1, i2, o] : view::zip_equal(in1, in2, out)) {
		o = pixelOps.template blend<w1, w2>(i1, i2);
	}
}

inline void alphaBlendLines(
	std::span<const Pixel> in1, std::span<const Pixel> in2, std::span<Pixel> out)
{
	// It _IS_ allowed that the output is the same as one of the inputs.
	assert(in1.size() == in2.size());
	assert(in1.size() == out.size());
	PixelOperations pixelOps;
	for (auto [i1, i2, o] : view::zip_equal(in1, in2, out)) {
		o = pixelOps.alphaBlend(i1, i2);
	}
}

inline void alphaBlendLines(
	Pixel in1, std::span<const Pixel> in2, std::span<Pixel> out)
{
	// It _IS_ allowed that the output is the same as the input.

	// ATM this routine is only called when 'in1' is not fully opaque nor
	// fully transparent.
	assert(in2.size() == out.size());

	PixelOperations pixelOps;
	unsigned alpha = pixelOps.alpha(in1);

	// When one of the two colors is loop-invariant, using the
	// pre-multiplied-alpha-blending equation is a tiny bit more efficient
	// than using alphaBlend() or even lerp().
	//    for (auto i : xrange(width)) {
	//        out[i] = pixelOps.lerp(in1, in2[i], alpha);
	//    }
	Pixel in1M = pixelOps.multiply(in1, alpha);
	unsigned alpha2 = 256 - alpha;
	for (auto [i2, o] : view::zip_equal(in2, out)) {
		o = in1M + pixelOps.multiply(i2, alpha2);
	}
}

} // namespace openmsx

#endif
