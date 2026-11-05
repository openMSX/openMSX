#include "yuv2rgb.hh"

#include "RawFrame.hh"

#include "xrange.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>

#ifdef __AVX2__
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace openmsx::yuv2rgb {

using Pixel = uint32_t;

// ITU-R BT.601, limited range (Y in [16..235], Cb/Cr in [16..240]). Both
// color spaces supported by Theora (Rec. 470M and Rec. 470BG) use this matrix.
static constexpr double Kr = 0.299;
static constexpr double Kb = 0.114;
static constexpr double Kg = 1.0 - Kr - Kb;
static constexpr double SCALE_Y = 255.0 / 219.0;
static constexpr double SCALE_C = 255.0 / 224.0;
static constexpr double COEF_Y  = SCALE_Y;
static constexpr double COEF_RV = SCALE_C * 2.0 * (1.0 - Kr);
static constexpr double COEF_GU = SCALE_C * 2.0 * (1.0 - Kb) * Kb / Kg;
static constexpr double COEF_GV = SCALE_C * 2.0 * (1.0 - Kr) * Kr / Kg;
static constexpr double COEF_BU = SCALE_C * 2.0 * (1.0 - Kb);

// All calculations are done in 16-bit fixed point with F fractional bits:
//   luma term:    mulhi_u16(Y << 8, CY)             = Y * COEF_Y * 2^F
//   chroma terms: mulhi_s16((C - 128) << 8, coef)  = (C - 128) * coef * 2^F
// COEF_BU * 2^F * 256 doesn't fit in int16, so for blue the coefficient is
// halved and the result is doubled. With F=5 all intermediate values fit in
// int16. All results are within +/-1 of the exactly rounded result (~99.2% are
// identical).
static constexpr int F = 5;
[[nodiscard]] static constexpr int16_t toFixed(double c, int scale)
{
	double x = c * (1 << F) * scale;
	return int16_t(x < 0.0 ? x - 0.5 : x + 0.5);
}
static constexpr uint16_t CY  = uint16_t(toFixed(COEF_Y, 256));
static constexpr int16_t  CRV = toFixed( COEF_RV, 256);
static constexpr int16_t  CGU = toFixed(-COEF_GU, 256);
static constexpr int16_t  CGV = toFixed(-COEF_GV, 256);
static constexpr int16_t  CBU = toFixed( COEF_BU, 128);
// Offset for (Y - 16), plus rounding. Values found by exhaustive search.
static constexpr int16_t KR = -580;
static constexpr int16_t KG = -579;
static constexpr int16_t KB = -579;

// Each code path defines convertBlock(), which converts a block of BLOCK x 2
// pixels (and the corresponding BLOCK/2 x 1 chroma samples).
#ifdef __AVX2__

static constexpr int BLOCK = 64;

static inline void convertPixels(__m256i Y, Pixel* out, __m256i R, __m256i G, __m256i B)
{
	__m256i yt = _mm256_mulhi_epu16(Y, _mm256_set1_epi16(int16_t(CY)));
	__m256i r = _mm256_srai_epi16(_mm256_add_epi16(yt, R), F);
	__m256i g = _mm256_srai_epi16(_mm256_add_epi16(yt, G), F);
	__m256i b = _mm256_srai_epi16(_mm256_add_epi16(yt, B), F);
	__m256i rb = _mm256_packus_epi16(r, b);
	__m256i g0 = _mm256_packus_epi16(g, _mm256_setzero_si256());
	__m256i rg = _mm256_unpacklo_epi8(rb, g0);
	__m256i b0 = _mm256_unpackhi_epi8(rb, g0);
	_mm256_storeu_si256(reinterpret_cast<__m256i*>(out + 0), _mm256_unpacklo_epi16(rg, b0));
	_mm256_storeu_si256(reinterpret_cast<__m256i*>(out + 8), _mm256_unpackhi_epi16(rg, b0));
}

// Converts 32x2 pixels, u and v contain the 16 chroma samples as (C - 128) << 8.
// All 16-bit vectors use lane order [px0-3, px8-11 | px4-7, px12-15], then the
// in-lane pack/unpack sequence in convertPixels() produces the pixels in
// natural order.
static inline void convertBlock32(
	__m256i u, __m256i v, const uint8_t* pY0, const uint8_t* pY1,
	Pixel* out0, Pixel* out1)
{
	const __m256i perm = _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7);

	__m256i R = _mm256_add_epi16(_mm256_mulhi_epi16(v, _mm256_set1_epi16(CRV)), _mm256_set1_epi16(KR));
	__m256i G = _mm256_add_epi16(_mm256_add_epi16(_mm256_mulhi_epi16(u, _mm256_set1_epi16(CGU)),
	                                              _mm256_mulhi_epi16(v, _mm256_set1_epi16(CGV))),
	                             _mm256_set1_epi16(KG));
	__m256i B = _mm256_mulhi_epi16(u, _mm256_set1_epi16(CBU));
	B = _mm256_add_epi16(_mm256_add_epi16(B, B), _mm256_set1_epi16(KB));
	// horizontally duplicate the chroma terms
	__m256i R0 = _mm256_unpacklo_epi16(R, R), R1 = _mm256_unpackhi_epi16(R, R);
	__m256i G0 = _mm256_unpacklo_epi16(G, G), G1 = _mm256_unpackhi_epi16(G, G);
	__m256i B0 = _mm256_unpacklo_epi16(B, B), B1 = _mm256_unpackhi_epi16(B, B);

	auto doRow = [&](const uint8_t* pY, Pixel* out) {
		__m256i Y = _mm256_permutevar8x32_epi32(
			_mm256_loadu_si256(reinterpret_cast<const __m256i*>(pY)), perm);
		const __m256i zero = _mm256_setzero_si256();
		convertPixels(_mm256_unpacklo_epi8(zero, Y), out +  0, R0, G0, B0); // Y << 8
		convertPixels(_mm256_unpackhi_epi8(zero, Y), out + 16, R1, G1, B1);
	};
	doRow(pY0, out0);
	doRow(pY1, out1);
}

static inline void convertBlock(
	const uint8_t* pY0, const uint8_t* pY1, const uint8_t* pCb, const uint8_t* pCr,
	Pixel* out0, Pixel* out1)
{
	// Load 32 chroma samples (as C - 128) and shuffle them so that the low and
	// high halves of each lane give the lane order required by convertBlock32().
	auto loadC = [](const uint8_t* p) {
		__m256i c = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
		c = _mm256_xor_si256(c, _mm256_set1_epi8(int8_t(0x80)));
		c = _mm256_shuffle_epi8(c, _mm256_setr_epi8(
			0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15,
			0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15));
		return _mm256_permute4x64_epi64(c, 0xd8);
	};
	__m256i cb = loadC(pCb);
	__m256i cr = loadC(pCr);
	const __m256i zero = _mm256_setzero_si256();
	convertBlock32(_mm256_unpacklo_epi8(zero, cb), _mm256_unpacklo_epi8(zero, cr),
	               pY0 +  0, pY1 +  0, out0 +  0, out1 +  0);
	convertBlock32(_mm256_unpackhi_epi8(zero, cb), _mm256_unpackhi_epi8(zero, cr),
	               pY0 + 32, pY1 + 32, out0 + 32, out1 + 32);
}

#elif defined(__SSE2__)

static constexpr int BLOCK = 32;

static inline void convertPixels(__m128i Y, Pixel* out, __m128i R, __m128i G, __m128i B)
{
	__m128i yt = _mm_mulhi_epu16(Y, _mm_set1_epi16(int16_t(CY)));
	__m128i r = _mm_srai_epi16(_mm_add_epi16(yt, R), F);
	__m128i g = _mm_srai_epi16(_mm_add_epi16(yt, G), F);
	__m128i b = _mm_srai_epi16(_mm_add_epi16(yt, B), F);
	__m128i rb = _mm_packus_epi16(r, b);
	__m128i g0 = _mm_packus_epi16(g, _mm_setzero_si128());
	__m128i rg = _mm_unpacklo_epi8(rb, g0);
	__m128i b0 = _mm_unpackhi_epi8(rb, g0);
	_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 0), _mm_unpacklo_epi16(rg, b0));
	_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 4), _mm_unpackhi_epi16(rg, b0));
}

static inline void convertBlock(
	const uint8_t* pY0, const uint8_t* pY1, const uint8_t* pCb, const uint8_t* pCr,
	Pixel* out0, Pixel* out1)
{
	const __m128i zero = _mm_setzero_si128();
	auto loadC = [&](const uint8_t* p) { // (C - 128) << 8
		__m128i c = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
		return _mm_xor_si128(_mm_unpacklo_epi8(zero, c), _mm_set1_epi16(int16_t(0x8000)));
	};
	for (int x = 0; x < 32; x += 16) {
		__m128i u = loadC(pCb + x / 2);
		__m128i v = loadC(pCr + x / 2);
		__m128i R = _mm_add_epi16(_mm_mulhi_epi16(v, _mm_set1_epi16(CRV)), _mm_set1_epi16(KR));
		__m128i G = _mm_add_epi16(_mm_add_epi16(_mm_mulhi_epi16(u, _mm_set1_epi16(CGU)),
		                                        _mm_mulhi_epi16(v, _mm_set1_epi16(CGV))),
		                          _mm_set1_epi16(KG));
		__m128i B = _mm_mulhi_epi16(u, _mm_set1_epi16(CBU));
		B = _mm_add_epi16(_mm_add_epi16(B, B), _mm_set1_epi16(KB));
		// horizontally duplicate the chroma terms
		__m128i R0 = _mm_unpacklo_epi16(R, R), R1 = _mm_unpackhi_epi16(R, R);
		__m128i G0 = _mm_unpacklo_epi16(G, G), G1 = _mm_unpackhi_epi16(G, G);
		__m128i B0 = _mm_unpacklo_epi16(B, B), B1 = _mm_unpackhi_epi16(B, B);

		auto doRow = [&](const uint8_t* pY, Pixel* out) {
			__m128i Y = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pY + x));
			convertPixels(_mm_unpacklo_epi8(zero, Y), out + x + 0, R0, G0, B0); // Y << 8
			convertPixels(_mm_unpackhi_epi8(zero, Y), out + x + 8, R1, G1, B1);
		};
		doRow(pY0, out0);
		doRow(pY1, out1);
	}
}

#else

static constexpr int BLOCK = 32;

[[nodiscard]] static inline int16_t mulhi(int16_t a, int16_t b)
{
	return int16_t((int32_t(a) * b) >> 16);
}
[[nodiscard]] static inline uint16_t mulhi(uint16_t a, uint16_t b)
{
	return uint16_t((uint32_t(a) * b) >> 16);
}
[[nodiscard]] static inline uint8_t clip(int16_t x)
{
	return uint8_t(std::clamp<int16_t>(x, 0, 255));
}

// Written in a way that auto-vectorizes well. Note: the explicit truncations
// to int16_t matter, otherwise the compiler must use 32-bit arithmetic.
static inline void convertBlock(
	const uint8_t* pY0, const uint8_t* pY1, const uint8_t* pCb, const uint8_t* pCr,
	Pixel* out0, Pixel* out1)
{
	std::array<int16_t, 32> R, G, B; // horizontally duplicated chroma terms
	for (auto i : xrange(16)) {
		auto u = int16_t((pCb[i] - 128) * 256);
		auto v = int16_t((pCr[i] - 128) * 256);
		R[2 * i] = R[2 * i + 1] = int16_t(mulhi(v, CRV) + KR);
		G[2 * i] = G[2 * i + 1] = int16_t(mulhi(u, CGU) + mulhi(v, CGV) + KG);
		B[2 * i] = B[2 * i + 1] = int16_t(2 * mulhi(u, CBU) + KB);
	}
	auto doRow = [&](const uint8_t* pY, Pixel* out) {
		for (auto i : xrange(32)) {
			auto yt = int16_t(mulhi(uint16_t(pY[i] << 8), CY));
			auto r = int16_t(int16_t(yt + R[i]) >> F);
			auto g = int16_t(int16_t(yt + G[i]) >> F);
			auto b = int16_t(int16_t(yt + B[i]) >> F);
			auto lo = uint16_t(clip(r) | (clip(g) << 8));
			auto hi = uint16_t(clip(b));
			out[i] = Pixel(lo) | (Pixel(hi) << 16);
		}
	};
	doRow(pY0, out0);
	doRow(pY1, out1);
}

#endif

void convert(const th_ycbcr_buffer& buffer, RawFrame& output)
{
	assert(buffer[1].width  * 2 == buffer[0].width);
	assert(buffer[1].height * 2 == buffer[0].height);

	const int    width      = buffer[0].width;
	const size_t y_stride   = buffer[0].stride;
	const size_t uv_stride2 = buffer[1].stride / 2;
	assert((width % 64) == 0);

	for (int y = 0; y < buffer[0].height; y += 2) {
		const uint8_t* pY0 = buffer[0].data + y * y_stride;
		const uint8_t* pY1 = pY0 + y_stride;
		const uint8_t* pCb = buffer[1].data + y * uv_stride2;
		const uint8_t* pCr = buffer[2].data + y * uv_stride2;
		Pixel* out0 = output.getLineDirect(y + 0).data();
		Pixel* out1 = output.getLineDirect(y + 1).data();

		for (int x = 0; x < width; x += BLOCK) {
			convertBlock(pY0 + x, pY1 + x, pCb + x / 2, pCr + x / 2, out0 + x, out1 + x);
		}

		output.setLineWidth(y + 0, width);
		output.setLineWidth(y + 1, width);
	}
}

} // namespace openmsx::yuv2rgb
