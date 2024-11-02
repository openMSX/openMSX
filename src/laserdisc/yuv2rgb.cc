#include "yuv2rgb.hh"

#include "RawFrame.hh"

#include "Math.hh"
#include "xrange.hh"

#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx::yuv2rgb {

using Pixel = uint32_t;

#ifdef __SSE2__

/*
 * This implementation of yuv420 to rgb is based upon the corresponding routine
 * from Mono. See this blog entry:
 *	http://blog.sublimeintervention.com/archive/2008/Mar-21.html
 * Source code:
 *	http://anonsvn.mono-project.com/viewvc/trunk/moon/src/yuv-converter.cpp?revision=136072
 * This code is GPL2 (only)
 *
 * Copyright 2008 Novell, Inc. (http://www.novell.com)
 *
 * There are other implementations:
 *  - ffmpeg
 *  - mythtv
 *  - pcsx2
 * I have not done a comparison of these implementations.
 */

/* R = 1.164 * (Y - 16) + 1.596 * (V - 128)
 * G = 1.164 * (Y - 16) - 0.813 * (V - 128) - 0.391 * (U - 128)
 * B = 1.164 * (Y - 16)                     + 2.018 * (U - 128)
 *   OR
 * R = 1.164 * Y + 1.596 * V             - 222.921
 * G = 1.164 * Y - 0.813 * V - 0.391 * U + 135.576
 * B = 1.164 * Y             + 2.018 * U - 276.836
 */
static inline void yuv2rgb_sse2(
	const uint8_t* u_ , const uint8_t* v_,
	const uint8_t* y0_, const uint8_t* y1_,
	Pixel* out0_, Pixel* out1_)
{
	// This routine calculates 32x2 RGBA pixels. Each output pixel uses a
	// unique corresponding input Y value, but a group of 2x2 ouput pixels
	// shares the same U and V input value.
	const auto* u    = std::bit_cast<const __m128i*>(u_);
	const auto* v    = std::bit_cast<const __m128i*>(v_);
	const auto* y0   = std::bit_cast<const __m128i*>(y0_);
	const auto* y1   = std::bit_cast<const __m128i*>(y1_);
	      auto* out0 = std::bit_cast<      __m128i*>(out0_);
	      auto* out1 = std::bit_cast<      __m128i*>(out1_);

	// constants
	const __m128i ZERO    = _mm_setzero_si128();
	const __m128i ALPHA   = _mm_set1_epi16(    -1); // 0xFFFF
	const __m128i RED_V   = _mm_set1_epi16(   102); // 102/64 =  1.59
	const __m128i GREEN_U = _mm_set1_epi16(   -25); // -25/64 = -0.39
	const __m128i GREEN_V = _mm_set1_epi16(   -52); // -52/64 = -0.81
	const __m128i BLUE_U  = _mm_set1_epi16(   129); // 129/64 =  2.02
	const __m128i COEF_Y  = _mm_set1_epi16(    74); //  74/64 =  1.16
	const __m128i CNST_R  = _mm_set1_epi16(  -223); // -222.921
	const __m128i CNST_G  = _mm_set1_epi16(   136); //  135.576
	const __m128i CNST_B  = _mm_set1_epi16(  -277); // -276.836
	const __m128i Y_MASK  = _mm_set1_epi16(0x00FF);

	// left
	__m128i u0f  = _mm_loadu_si128(u);
	__m128i v0f  = _mm_loadu_si128(v);
	__m128i u07  = _mm_unpacklo_epi8(u0f, ZERO);
	__m128i v07  = _mm_unpacklo_epi8(v0f, ZERO);
	__m128i mr07 = _mm_srai_epi16(_mm_mullo_epi16(v07, RED_V), 6);
	__m128i sg07 = _mm_mullo_epi16(v07, GREEN_V);
	__m128i tg07 = _mm_mullo_epi16(u07, GREEN_U);
	__m128i mg07 = _mm_srai_epi16(_mm_adds_epi16(sg07, tg07), 6);
	__m128i mb07 = _mm_srli_epi16(_mm_mullo_epi16(u07, BLUE_U), 6); // logical shift
	__m128i dr07 = _mm_adds_epi16(mr07, CNST_R);
	__m128i dg07 = _mm_adds_epi16(mg07, CNST_G);
	__m128i db07 = _mm_adds_epi16(mb07, CNST_B);

	// block top,left
	__m128i y00_0f    = _mm_loadu_si128(y0 + 0);
	__m128i y00_even  = _mm_and_si128(y00_0f, Y_MASK);
	__m128i y00_odd   = _mm_srli_epi16(y00_0f, 8);
	__m128i dy00_even = _mm_srai_epi16(_mm_mullo_epi16(y00_even, COEF_Y), 6);
	__m128i dy00_odd  = _mm_srai_epi16(_mm_mullo_epi16(y00_odd,  COEF_Y), 6);
	__m128i r00_even  = _mm_adds_epi16(dr07, dy00_even);
	__m128i g00_even  = _mm_adds_epi16(dg07, dy00_even);
	__m128i b00_even  = _mm_adds_epi16(db07, dy00_even);
	__m128i r00_odd   = _mm_adds_epi16(dr07, dy00_odd);
	__m128i g00_odd   = _mm_adds_epi16(dg07, dy00_odd);
	__m128i b00_odd   = _mm_adds_epi16(db07, dy00_odd);
	__m128i r00_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(r00_even, r00_even),
	                                      _mm_packus_epi16(r00_odd,  r00_odd));
	__m128i g00_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(g00_even, g00_even),
	                                      _mm_packus_epi16(g00_odd,  g00_odd));
	__m128i b00_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(b00_even, b00_even),
	                                      _mm_packus_epi16(b00_odd,  b00_odd));
	__m128i rb00_07   = _mm_unpacklo_epi8(r00_0f, b00_0f);
	__m128i rb00_8f   = _mm_unpackhi_epi8(r00_0f, b00_0f);
	__m128i ga00_07   = _mm_unpacklo_epi8(g00_0f, ALPHA);
	__m128i ga00_8f   = _mm_unpackhi_epi8(g00_0f, ALPHA);
	__m128i rgba00_03 = _mm_unpacklo_epi8(rb00_07, ga00_07);
	__m128i rgba00_47 = _mm_unpackhi_epi8(rb00_07, ga00_07);
	__m128i rgba00_8b = _mm_unpacklo_epi8(rb00_8f, ga00_8f);
	__m128i rgba00_cf = _mm_unpackhi_epi8(rb00_8f, ga00_8f);
	_mm_storeu_si128(out0 + 0, rgba00_03);
	_mm_storeu_si128(out0 + 1, rgba00_47);
	_mm_storeu_si128(out0 + 2, rgba00_8b);
	_mm_storeu_si128(out0 + 3, rgba00_cf);

	// block bottom,left
	__m128i y10_0f    = _mm_loadu_si128(y1 + 0);
	__m128i y10_even  = _mm_and_si128(y10_0f, Y_MASK);
	__m128i y10_odd   = _mm_srli_epi16(y10_0f, 8);
	__m128i dy10_even = _mm_srai_epi16(_mm_mullo_epi16(y10_even, COEF_Y), 6);
	__m128i dy10_odd  = _mm_srai_epi16(_mm_mullo_epi16(y10_odd,  COEF_Y), 6);
	__m128i r10_even  = _mm_adds_epi16(dr07, dy10_even);
	__m128i g10_even  = _mm_adds_epi16(dg07, dy10_even);
	__m128i b10_even  = _mm_adds_epi16(db07, dy10_even);
	__m128i r10_odd   = _mm_adds_epi16(dr07, dy10_odd);
	__m128i g10_odd   = _mm_adds_epi16(dg07, dy10_odd);
	__m128i b10_odd   = _mm_adds_epi16(db07, dy10_odd);
	__m128i r10_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(r10_even, r10_even),
	                                      _mm_packus_epi16(r10_odd,  r10_odd));
	__m128i g10_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(g10_even, g10_even),
	                                      _mm_packus_epi16(g10_odd,  g10_odd));
	__m128i b10_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(b10_even, b10_even),
	                                      _mm_packus_epi16(b10_odd,  b10_odd));
	__m128i rb10_07   = _mm_unpacklo_epi8(r10_0f, b10_0f);
	__m128i rb10_8f   = _mm_unpackhi_epi8(r10_0f, b10_0f);
	__m128i ga10_07   = _mm_unpacklo_epi8(g10_0f, ALPHA);
	__m128i ga10_8f   = _mm_unpackhi_epi8(g10_0f, ALPHA);
	__m128i rgba10_03 = _mm_unpacklo_epi8(rb10_07, ga10_07);
	__m128i rgba10_47 = _mm_unpackhi_epi8(rb10_07, ga10_07);
	__m128i rgba10_8b = _mm_unpacklo_epi8(rb10_8f, ga10_8f);
	__m128i rgba10_cf = _mm_unpackhi_epi8(rb10_8f, ga10_8f);
	_mm_storeu_si128(out1 + 0, rgba10_03);
	_mm_storeu_si128(out1 + 1, rgba10_47);
	_mm_storeu_si128(out1 + 2, rgba10_8b);
	_mm_storeu_si128(out1 + 3, rgba10_cf);

	// right
	__m128i u8f  = _mm_unpackhi_epi8(u0f, ZERO);
	__m128i v8f  = _mm_unpackhi_epi8(v0f, ZERO);
	__m128i mr8f = _mm_srai_epi16(_mm_mullo_epi16(v8f, RED_V), 6);
	__m128i sg8f = _mm_mullo_epi16(v8f, GREEN_V);
	__m128i tg8f = _mm_mullo_epi16(u8f, GREEN_U);
	__m128i mg8f = _mm_srai_epi16(_mm_adds_epi16(sg8f, tg8f), 6);
	__m128i mb8f = _mm_srli_epi16(_mm_mullo_epi16(u8f, BLUE_U), 6); // logical shift
	__m128i dr8f = _mm_adds_epi16(mr8f, CNST_R);
	__m128i dg8f = _mm_adds_epi16(mg8f, CNST_G);
	__m128i db8f = _mm_adds_epi16(mb8f, CNST_B);

	// block top,right
	__m128i y01_0f    = _mm_loadu_si128(y0 + 1);
	__m128i y01_even  = _mm_and_si128(y01_0f, Y_MASK);
	__m128i y01_odd   = _mm_srli_epi16(y01_0f, 8);
	__m128i dy01_even = _mm_srai_epi16(_mm_mullo_epi16(y01_even, COEF_Y), 6);
	__m128i dy01_odd  = _mm_srai_epi16(_mm_mullo_epi16(y01_odd,  COEF_Y), 6);
	__m128i r01_even  = _mm_adds_epi16(dr8f, dy01_even);
	__m128i g01_even  = _mm_adds_epi16(dg8f, dy01_even);
	__m128i b01_even  = _mm_adds_epi16(db8f, dy01_even);
	__m128i r01_odd   = _mm_adds_epi16(dr8f, dy01_odd);
	__m128i g01_odd   = _mm_adds_epi16(dg8f, dy01_odd);
	__m128i b01_odd   = _mm_adds_epi16(db8f, dy01_odd);
	__m128i r01_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(r01_even, r01_even),
	                                      _mm_packus_epi16(r01_odd,  r01_odd));
	__m128i g01_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(g01_even, g01_even),
	                                      _mm_packus_epi16(g01_odd,  g01_odd));
	__m128i b01_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(b01_even, b01_even),
	                                      _mm_packus_epi16(b01_odd,  b01_odd));
	__m128i rb01_07   = _mm_unpacklo_epi8(r01_0f, b01_0f);
	__m128i rb01_8f   = _mm_unpackhi_epi8(r01_0f, b01_0f);
	__m128i ga01_07   = _mm_unpacklo_epi8(g01_0f, ALPHA);
	__m128i ga01_8f   = _mm_unpackhi_epi8(g01_0f, ALPHA);
	__m128i rgba01_03 = _mm_unpacklo_epi8(rb01_07, ga01_07);
	__m128i rgba01_47 = _mm_unpackhi_epi8(rb01_07, ga01_07);
	__m128i rgba01_8b = _mm_unpacklo_epi8(rb01_8f, ga01_8f);
	__m128i rgba01_cf = _mm_unpackhi_epi8(rb01_8f, ga01_8f);
	_mm_storeu_si128(out0 + 4, rgba01_03);
	_mm_storeu_si128(out0 + 5, rgba01_47);
	_mm_storeu_si128(out0 + 6, rgba01_8b);
	_mm_storeu_si128(out0 + 7, rgba01_cf);

	// block bottom,right
	__m128i y11_0f    = _mm_loadu_si128(y1 + 1);
	__m128i y11_even  = _mm_and_si128(y11_0f, Y_MASK);
	__m128i y11_odd   = _mm_srli_epi16(y11_0f, 8);
	__m128i dy11_even = _mm_srai_epi16(_mm_mullo_epi16(y11_even, COEF_Y), 6);
	__m128i dy11_odd  = _mm_srai_epi16(_mm_mullo_epi16(y11_odd,  COEF_Y), 6);
	__m128i r11_even  = _mm_adds_epi16(dr8f, dy11_even);
	__m128i g11_even  = _mm_adds_epi16(dg8f, dy11_even);
	__m128i b11_even  = _mm_adds_epi16(db8f, dy11_even);
	__m128i r11_odd   = _mm_adds_epi16(dr8f, dy11_odd);
	__m128i g11_odd   = _mm_adds_epi16(dg8f, dy11_odd);
	__m128i b11_odd   = _mm_adds_epi16(db8f, dy11_odd);
	__m128i r11_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(r11_even, r11_even),
	                                      _mm_packus_epi16(r11_odd,  r11_odd));
	__m128i g11_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(g11_even, g11_even),
	                                      _mm_packus_epi16(g11_odd,  g11_odd));
	__m128i b11_0f    = _mm_unpackhi_epi8(_mm_packus_epi16(b11_even, b11_even),
	                                      _mm_packus_epi16(b11_odd,  b11_odd));
	__m128i rb11_07   = _mm_unpacklo_epi8(r11_0f, b11_0f);
	__m128i rb11_8f   = _mm_unpackhi_epi8(r11_0f, b11_0f);
	__m128i ga11_07   = _mm_unpacklo_epi8(g11_0f, ALPHA);
	__m128i ga11_8f   = _mm_unpackhi_epi8(g11_0f, ALPHA);
	__m128i rgba11_03 = _mm_unpacklo_epi8(rb11_07, ga11_07);
	__m128i rgba11_47 = _mm_unpackhi_epi8(rb11_07, ga11_07);
	__m128i rgba11_8b = _mm_unpacklo_epi8(rb11_8f, ga11_8f);
	__m128i rgba11_cf = _mm_unpackhi_epi8(rb11_8f, ga11_8f);
	_mm_storeu_si128(out1 + 4, rgba11_03);
	_mm_storeu_si128(out1 + 5, rgba11_47);
	_mm_storeu_si128(out1 + 6, rgba11_8b);
	_mm_storeu_si128(out1 + 7, rgba11_cf);
}

static inline void convertHelperSSE2(
	const th_ycbcr_buffer& buffer, RawFrame& output)
{
	const int    width      = buffer[0].width;
	const size_t y_stride   = buffer[0].stride;
	const size_t uv_stride2 = buffer[1].stride / 2;

	assert((width % 32) == 0);
	assert((buffer[0].height % 2) == 0);

	for (int y = 0; y < buffer[0].height; y += 2) {
		const uint8_t* pY1 = buffer[0].data + (y + 0) * y_stride;
		const uint8_t* pY2 = buffer[0].data + (y + 1) * y_stride;
		const uint8_t* pCb = buffer[1].data + (y + 0) * uv_stride2;
		const uint8_t* pCr = buffer[2].data + (y + 0) * uv_stride2;
		auto out0 = output.getLineDirect(y + 0);
		auto out1 = output.getLineDirect(y + 1);

		for (int x = 0; x < width; x += 32) {
			// convert a block of (32 x 2) pixels
			yuv2rgb_sse2(pCb, pCr, pY1, pY2, &out0[x], &out1[x]);
			pCb += 16;
			pCr += 16;
			pY1 += 32;
			pY2 += 32;
		}

		output.setLineWidth(y + 0, width);
		output.setLineWidth(y + 1, width);
	}
}

#endif // __SSE2__

static constexpr int PREC = 15;
static constexpr int COEF_Y  = int(1.164 * (1 << PREC) + 0.5); // prefer to use lrint() to round
static constexpr int COEF_RV = int(1.596 * (1 << PREC) + 0.5); // but that's not (yet) constexpr
static constexpr int COEF_GU = int(0.391 * (1 << PREC) + 0.5); // in current versions of c++
static constexpr int COEF_GV = int(0.813 * (1 << PREC) + 0.5);
static constexpr int COEF_BU = int(2.018 * (1 << PREC) + 0.5);

struct Coefs {
	std::array<int, 256> gu;
	std::array<int, 256> gv;
	std::array<int, 256> bu;
	std::array<int, 256> rv;
	std::array<int, 256> y;
};

[[nodiscard]] static constexpr Coefs getCoefs()
{
	Coefs coefs = {};
	for (auto i : xrange(256)) {
		coefs.gu[i] = -COEF_GU * (i - 128);
		coefs.gv[i] = -COEF_GV * (i - 128);
		coefs.bu[i] =  COEF_BU * (i - 128);
		coefs.rv[i] =  COEF_RV * (i - 128);
		coefs.y[i]  =  COEF_Y  * (i -  16) + (PREC / 2);
	}
	return coefs;
}

[[nodiscard]] static inline Pixel calc(
	int y, int ruv, int guv, int buv)
{
	uint8_t r = Math::clipIntToByte((y + ruv) >> PREC);
	uint8_t g = Math::clipIntToByte((y + guv) >> PREC);
	uint8_t b = Math::clipIntToByte((y + buv) >> PREC);
	return (r << 0) | (g << 8) | (b << 16);
}

static void convertHelper(const th_ycbcr_buffer& buffer, RawFrame& output)
{
	assert(buffer[1].width  * 2 == buffer[0].width);
	assert(buffer[1].height * 2 == buffer[0].height);

	static constexpr Coefs coefs = getCoefs();

	const int    width      = buffer[0].width;
	const size_t y_stride   = buffer[0].stride;
	const size_t uv_stride2 = buffer[1].stride / 2;

	for (int y = 0; y < buffer[0].height; y += 2) {
		const uint8_t* pY  = buffer[0].data + y * y_stride;
		const uint8_t* pCb = buffer[1].data + y * uv_stride2;
		const uint8_t* pCr = buffer[2].data + y * uv_stride2;
		auto out0 = output.getLineDirect(y + 0);
		auto out1 = output.getLineDirect(y + 1);

		for (int x = 0; x < width; x += 2, pY += 2, ++pCr, ++pCb) {
			int ruv = coefs.rv[*pCr];
			int guv = coefs.gu[*pCb] + coefs.gv[*pCr];
			int buv = coefs.bu[*pCb];

			int Y00 = coefs.y[pY[0]];
			out0[x + 0] = calc(Y00, ruv, guv, buv);

			int Y01 = coefs.y[pY[1]];
			out0[x + 1] = calc(Y01, ruv, guv, buv);

			int Y10 = coefs.y[pY[y_stride + 0]];
			out1[x + 0] = calc(Y10, ruv, guv, buv);

			int Y11 = coefs.y[pY[y_stride + 1]];
			out1[x + 1] = calc(Y11, ruv, guv, buv);
		}

		output.setLineWidth(y + 0, width);
		output.setLineWidth(y + 1, width);
	}
}

void convert(const th_ycbcr_buffer& input, RawFrame& output)
{
#ifdef __SSE2__
		convertHelperSSE2(input, output);
		return;
#endif
		convertHelper(input, output);
}

} // namespace openmsx::yuv2rgb
