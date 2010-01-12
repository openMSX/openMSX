// $Id$

#include "yuv2rgb.hh"
#include "RawFrame.hh"
#include "Math.hh"
#include "HostCPU.hh"
#include <SDL.h>
#include <cassert>

namespace openmsx {
namespace yuv2rgb {

// TODO - This is temporary, until we have VC++ compatible versions of these routines
#ifdef _MSC_VER
#undef ASM_X86
#endif

#if ASM_X86

/*
 * This implementation of yuv420 to rgb has copied from Mono. See this
 * blog entry:
 * 	http://blog.sublimeintervention.com/archive/2008/Mar-21.html
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

/* R = 1.164 * (Y - 16)		+ 1.596 * (V - 128)
 * G = 1.164 * (Y - 16)		- 0.813 * (V - 128)	- 0.391 * (U - 128)
 * B = 1.164 * (Y - 16)					+ 2.018 * (U - 128)
 *
 * R V coefficient = 1.596*64 = 102 = 0x66
 * G V coefficient = 0.813*64 =  52 = 0x34 (-ve) == 0xFFCC
 * G U coefficient = 0.391*64 =  25 = 0x19 (-ve) == 0xFFE7
 * B U coefficient = 2.018*64 = 129 = 0x81
 * Y coefficient   = 1.164*64 =  74 = 0x4a
 */

#define RED_V_C 0x0066006600660066ULL
#define GREEN_V_C 0xffccffccffccffccULL
#define GREEN_U_C 0xffe7ffe7ffe7ffe7ULL
#define BLUE_U_C 0x0081008100810081ULL
#define Y_C 0x004a004a004a004aULL
#define UV_128 0x0080008000800080ULL
#define Y_16 0x1010101010101010ULL
#define ALPHA_MASK 0xFFFFFFFFFFFFFFFFULL

static const uint64 simd_table [16] __attribute__ ((aligned (16))) = {
									RED_V_C, RED_V_C,
									GREEN_V_C, GREEN_V_C,
									GREEN_U_C, GREEN_U_C,
									BLUE_U_C, BLUE_U_C,
									Y_C, Y_C,
									UV_128, UV_128,
									Y_16, Y_16,
									ALPHA_MASK, ALPHA_MASK,
};

#define PREFETCH(memory) do {			\
		__asm__ __volatile__ (		\
			"prefetchnta (%0);"	\
		: : "r" (memory));		\
	} while (0);

#if defined(__x86_64__)
#define ALIGN_CMP_REG "rax"
#else
#define ALIGN_CMP_REG "eax"
#endif

#define CALC_COLOR_MODIFIERS(mov_instr, reg_type, alignment, align_reg, u, v, coeff_storage) do {					\
			__asm__ __volatile__ (												\
				"mov %0, %%"align_reg";"										\
				"and $"alignment", %%"align_reg";"									\
				"test %%"align_reg", %%"align_reg";"									\
				"je 1f;"												\
																	\
				mov_instr " 48(%2), %%"reg_type"2;"			/* restore Dred */				\
				mov_instr " 64(%2), %%"reg_type"3;"			/* restore Dgreen */				\
				mov_instr " 80(%2), %%"reg_type"1;"			/* restore Dblue */				\
																	\
				mov_instr " %%"reg_type"2, (%2);"			/* backup Dred */				\
				mov_instr " %%"reg_type"3, 16(%2);"			/* backup Dgreen */				\
				mov_instr " %%"reg_type"1, 32(%2);"			/* backup Dblue */				\
																	\
				"jmp 2f;"												\
																	\
				"1:"													\
				"pxor %%"reg_type"7, %%"reg_type"7;"									\
																	\
				mov_instr " (%0), %%"reg_type"1;"									\
				mov_instr " (%1), %%"reg_type"2;"									\
																	\
				mov_instr " %%"reg_type"1, %%"reg_type"5;"								\
				mov_instr " %%"reg_type"2, %%"reg_type"6;"								\
																	\
				"punpckhbw %%"reg_type"7, %%"reg_type"5;"								\
				"punpckhbw %%"reg_type"7, %%"reg_type"6;"								\
																	\
				"punpcklbw %%"reg_type"7, %%"reg_type"1;"								\
				"punpcklbw %%"reg_type"7, %%"reg_type"2;"								\
																	\
				mov_instr " 80(%3), %%"reg_type"7;"									\
																	\
				"psubsw %%"reg_type"7, %%"reg_type"5;"			/* U[hi] = U[hi] - 128 */			\
				"psubsw %%"reg_type"7, %%"reg_type"6;"			/* V[hi] = V[hi] - 128 */			\
																	\
				"psubsw %%"reg_type"7, %%"reg_type"1;"			/* U[lo] = U[lo] - 128 */			\
				"psubsw %%"reg_type"7, %%"reg_type"2;"			/* V[lo] = V[lo] - 128 */			\
																	\
				mov_instr " %%"reg_type"5, %%"reg_type"3;"								\
				mov_instr " %%"reg_type"6, %%"reg_type"4;"								\
																	\
				mov_instr " 32(%3), %%"reg_type"7;"									\
				"pmullw %%"reg_type"7, %%"reg_type"3;"			/* calculate Ugreen[hi] */			\
				"psraw $6, %%"reg_type"3;"				/* Ugreen[hi] = Ugreen[hi] / 64 */		\
				mov_instr " 16(%3), %%"reg_type"7;"									\
				"pmullw %%"reg_type"7, %%"reg_type"4;"			/* calculate Vgreen[hi] */			\
				"psraw $6, %%"reg_type"4;"				/* Vgreen[hi] = Vgreen[hi] / 64 */		\
				"paddsw %%"reg_type"4, %%"reg_type"3;"			/* Dgreen[hi] = Ugreen[hi] + Vgreen[hi] */	\
																	\
				mov_instr " %%"reg_type"3, 64(%2);"			/* backup Dgreen[hi] (clobbered) */		\
																	\
				mov_instr " %%"reg_type"1, %%"reg_type"3;"								\
				mov_instr " %%"reg_type"2, %%"reg_type"4;"								\
																	\
				mov_instr " 32(%3), %%"reg_type"7;"									\
				"pmullw %%"reg_type"7, %%"reg_type"3;"			/* calculate Ugreen[lo] */			\
				"psraw $6, %%"reg_type"3;"				/* Ugreen[lo] = Ugreen[lo] / 64 */		\
				mov_instr " 16(%3), %%"reg_type"7;"									\
				"pmullw %%"reg_type"7, %%"reg_type"4;"			/* calculate Vgreen[lo] */			\
				"psraw $6, %%"reg_type"4;"				/* Vgreen[lo] = Vgreen[lo] / 64 */		\
				"paddsw %%"reg_type"4, %%"reg_type"3;"			/* Dgreen[lo] = Ugreen[lo] + Vgreen[lo] */	\
																	\
				mov_instr " 48(%3), %%"reg_type"7;"									\
				"pmullw %%"reg_type"7, %%"reg_type"5;"			/* calculate Dblue[hi] */			\
				"psraw $6, %%"reg_type"5;"				/* Dblue[hi] = Dblue[hi] / 64 */		\
				"pmullw %%"reg_type"7, %%"reg_type"1;"			/* calculate Dblue[lo] */			\
				"psraw $6, %%"reg_type"1;"				/* Dblue[lo] = Dblue[lo] / 64 */		\
																	\
				mov_instr " (%3), %%"reg_type"7;"									\
				"pmullw %%"reg_type"7, %%"reg_type"6;"			/* calculate Dred[hi] */			\
				"psraw $6, %%"reg_type"6;"				/* Dred[hi] = Dred[hi] / 64 */			\
				"pmullw %%"reg_type"7, %%"reg_type"2;"			/* calculate Dred[lo] */			\
				"psraw $6, %%"reg_type"2;"				/* Dred[lo] = Dred[lo] / 64 */			\
																	\
				mov_instr " %%"reg_type"6, 48(%2);"			/* backup Dred[hi] */				\
				mov_instr " %%"reg_type"5, 80(%2);"			/* backup Dblue[hi] */				\
																	\
				mov_instr " %%"reg_type"2, 0(%2);"			/* backup Dred[lo] */				\
				mov_instr " %%"reg_type"3, 16(%2);"			/* backup Dgreen[lo] */				\
				mov_instr " %%"reg_type"1, 32(%2);"			/* backup Dblue[lo] */				\
				"2:"													\
				: : "r" (u), "r" (v), "r" (coeff_storage), "r" (&simd_table) : "%"align_reg);				\
	} while (0);

#define RESTORE_COLOR_MODIFIERS(mov_instr, reg_type, coeff_storage) do {				\
		__asm__ __volatile__ (									\
			mov_instr " (%0), %%"reg_type"2;"			/* restore Dred */	\
			mov_instr " 16(%0), %%"reg_type"3;"			/* restore Dgreen */	\
			mov_instr " 32(%0), %%"reg_type"1;"			/* restore Dblue */	\
			: : "r" (coeff_storage));							\
	} while (0);

#define YUV2RGB_INTEL_SIMD(mov_instr, reg_type, output_offset1, output_offset2, output_offset3, y_plane, dest) do { 	\
		__asm__ __volatile__ ( 											\
			mov_instr " (%0), %%"reg_type"0;"		/* Load Y plane into r0 */			\
			mov_instr " 96(%2), %%"reg_type"7;"		/* Load 16 into r7 */				\
			"psubusb %%"reg_type"7, %%"reg_type"0;"		/* Y = Y - 16 */				\
															\
			mov_instr " %%"reg_type"0, %%"reg_type"4;"	/* r4 == r0 */					\
															\
			"psllw $8, %%"reg_type"0;"			/* r0 [00 Y0 00 Y2 ...] */			\
			"psrlw $8, %%"reg_type"0;"			/* r0 [Y0 00 Y2 00 ...] */			\
			"psrlw $8, %%"reg_type"4;"			/* r4 [Y1 00 Y3 00 ...] */			\
															\
			mov_instr " 64(%2), %%"reg_type"7;"								\
			"pmullw %%"reg_type"7, %%"reg_type"0;"		/* calculate Y*Yc[even] */			\
			"pmullw %%"reg_type"7, %%"reg_type"4;"		/* calculate Y*Yc[odd] */			\
			"psraw $6, %%"reg_type"0;"			/* Yyc[even] = Yyc[even] / 64 */		\
			"psraw $6, %%"reg_type"4;"			/* Yyc[odd] = Yyc[odd] / 64 */			\
															\
			mov_instr " %%"reg_type"2, %%"reg_type"6;"							\
			mov_instr " %%"reg_type"3, %%"reg_type"7;"							\
			mov_instr " %%"reg_type"1, %%"reg_type"5;"							\
															\
			"paddsw %%"reg_type"0, %%"reg_type"2;"		/* CY[even] + DR */				\
			"paddsw %%"reg_type"0, %%"reg_type"3;"		/* CY[even] + DG */				\
			"paddsw %%"reg_type"0, %%"reg_type"1;"		/* CY[even] + DB */				\
															\
			"paddsw %%"reg_type"4, %%"reg_type"6;"		/* CY[odd] + DR */				\
			"paddsw %%"reg_type"4, %%"reg_type"7;"		/* CY[odd] + DG */				\
			"paddsw %%"reg_type"4, %%"reg_type"5;"		/* CY[odd] + DB */				\
															\
			"packuswb %%"reg_type"2, %%"reg_type"2;"	/* Clamp RGB to [0-255] */			\
			"packuswb %%"reg_type"3, %%"reg_type"3;"							\
			"packuswb %%"reg_type"1, %%"reg_type"1;"							\
															\
			"packuswb %%"reg_type"6, %%"reg_type"6;"							\
			"packuswb %%"reg_type"7, %%"reg_type"7;"							\
			"packuswb %%"reg_type"5, %%"reg_type"5;"							\
															\
			"punpcklbw %%"reg_type"6, %%"reg_type"2;"	/* r2 [R0 R1 R2 R3 ...] */			\
			"punpcklbw %%"reg_type"7, %%"reg_type"3;"	/* r3 [G0 G1 G2 G3 ...] */			\
			"punpcklbw %%"reg_type"5, %%"reg_type"1;"	/* r1 [B0 B1 B2 B3 ...] */			\
															\
			mov_instr " %%"reg_type"2, %%"reg_type"5;"	/* copy RGB */					\
			mov_instr " %%"reg_type"3, %%"reg_type"7;"							\
			mov_instr " %%"reg_type"1, %%"reg_type"6;"							\
															\
			mov_instr " 112(%2), %%"reg_type"4;"								\
			"punpcklbw %%"reg_type"2, %%"reg_type"1;"	/* r1 [B0 R0 B1 R1 ...] */			\
			"punpcklbw %%"reg_type"4, %%"reg_type"3;"	/* r4 [G0 FF G1 FF ...] */			\
															\
			mov_instr " %%"reg_type"1, %%"reg_type"0;"	/* r3 [G0 FF G1 FF ...] */			\
															\
			"punpcklbw %%"reg_type"3, %%"reg_type"1;"	/* r2 [B0 G0 R0 FF B1 G1 R1 FF ...] */		\
			"punpckhbw %%"reg_type"3, %%"reg_type"0;"	/* r3 [B2 G2 R2 FF B3 G3 R3 FF ...] */		\
															\
			mov_instr " %%"reg_type"1, (%1);"		/* output BGRA */	 			\
			mov_instr " %%"reg_type"0, "output_offset1"(%1);"						\
															\
			"punpckhbw %%"reg_type"5, %%"reg_type"6;"							\
			"punpckhbw %%"reg_type"4, %%"reg_type"7;"							\
															\
			mov_instr " %%"reg_type"6, %%"reg_type"0;"							\
															\
			"punpcklbw %%"reg_type"7, %%"reg_type"6;"							\
			"punpckhbw %%"reg_type"7, %%"reg_type"0;"							\
															\
			mov_instr " %%"reg_type"6, "output_offset2"(%1);"						\
			mov_instr " %%"reg_type"0, "output_offset3"(%1);"						\
			: : "r" (y_plane), "r" (dest), "r" (&simd_table));						\
	} while (0);

#define YUV2RGB_SSE(y_plane, dest) YUV2RGB_INTEL_SIMD("movdqa", "xmm", "16", "32", "48", y_plane, dest)

#define YUV2RGB_MMX(y_plane, dest) YUV2RGB_INTEL_SIMD("movq", "mm", "8", "16", "24", y_plane, dest)

static void convertHelperSSE2(const yuv_buffer& buffer, RawFrame& output)
{
	const int width      = buffer.y_width;
	const int y_stride   = buffer.y_stride;
	const int uv_stride2 = buffer.uv_stride / 2;
	byte rgb_uv[96] __attribute__((aligned(16)));

	for (int y = 0; y < buffer.y_height; y += 2) {
		const byte* pY1  = buffer.y + y * y_stride;
		const byte* pY2  = buffer.y + (y + 1) * y_stride;
		const byte* pCb = buffer.u + y * uv_stride2;
		const byte* pCr = buffer.v + y * uv_stride2;
		unsigned* out0 = output.getLinePtrDirect<unsigned>(y + 0);
		unsigned* out1 = output.getLinePtrDirect<unsigned>(y + 1);

		for (int x=0; x < width; x += 16) {
			PREFETCH(pY1);
			CALC_COLOR_MODIFIERS("movdqa", "xmm", "15",
					ALIGN_CMP_REG, pCb, pCr, rgb_uv);

			YUV2RGB_SSE(pY1, out0);

			PREFETCH(pY2);
			RESTORE_COLOR_MODIFIERS("movdqa", "xmm", rgb_uv);

			YUV2RGB_SSE(pY2, out1);

			pCb += 8;
			pCr += 8;
			pY1 += 16;
			pY2 += 16;
			out0 += 16;
			out1 += 16;
		}

		output.setLineWidth(y + 0, width);
		output.setLineWidth(y + 1, width);
	}
}

static void convertHelperMMX(const yuv_buffer& buffer, RawFrame& output)
{
	const int width      = buffer.y_width;
	const int y_stride   = buffer.y_stride;
	const int uv_stride2 = buffer.uv_stride / 2;
	byte rgb_uv[96] __attribute__((aligned(16)));

	for (int y = 0; y < buffer.y_height; y += 2) {
		const byte* pY1  = buffer.y + y * y_stride;
		const byte* pY2  = buffer.y + (y + 1) * y_stride;
		const byte* pCb = buffer.u + y * uv_stride2;
		const byte* pCr = buffer.v + y * uv_stride2;
		unsigned* out0 = output.getLinePtrDirect<unsigned>(y + 0);
		unsigned* out1 = output.getLinePtrDirect<unsigned>(y + 1);

		for (int x=0; x < width; x += 8) {
			PREFETCH(pY1);
			CALC_COLOR_MODIFIERS("movq", "mm", "7",
					ALIGN_CMP_REG, pCb, pCr, rgb_uv);

			YUV2RGB_MMX(pY1, out0);

			PREFETCH(pY2);
			RESTORE_COLOR_MODIFIERS("movq", "mm", rgb_uv);
			YUV2RGB_MMX(pY2, out1);

			pCb += 4;
			pCr += 4;
			pY1 += 8;
			pY2 += 8;
			out0 += 8;
			out1 += 8;
		}

		output.setLineWidth(y + 0, width);
		output.setLineWidth(y + 1, width);
	}
}

#endif /* ASM_X86 */

static int coefs_gu[256];
static int coefs_gv[256];
static int coefs_bu[256];
static int coefs_rv[256];
static int coefs_y [256];

static const int PREC = 15;
static const int COEF_Y  = int(1.164 * (1 << PREC) + 0.5);
static const int COEF_RV = int(1.596 * (1 << PREC) + 0.5);
static const int COEF_GU = int(0.391 * (1 << PREC) + 0.5);
static const int COEF_GV = int(0.813 * (1 << PREC) + 0.5);
static const int COEF_BU = int(2.018 * (1 << PREC) + 0.5);


static void initTables()
{
	static bool init = false;
	if (init) return;
	init = true;

	for (int i = 0; i < 256; ++i) {
		coefs_gu[i] = -COEF_GU * (i - 128);
		coefs_gv[i] = -COEF_GV * (i - 128);
		coefs_bu[i] =  COEF_BU * (i - 128);
		coefs_rv[i] =  COEF_RV * (i - 128);
		coefs_y[i]  =  COEF_Y  * (i -  16) + (PREC / 2);
	}
}

template<typename Pixel>
static inline Pixel calc(const SDL_PixelFormat& format,
                         int y, int ruv, int guv, int buv)
{
	byte r = Math::clipIntToByte((y + ruv) >> PREC);
	byte g = Math::clipIntToByte((y + guv) >> PREC);
	byte b = Math::clipIntToByte((y + buv) >> PREC);
	if (sizeof(Pixel) == 4) {
		return (r << 16) | (g << 8) | (b << 0);
	} else {
		return static_cast<Pixel>(SDL_MapRGB(&format, r, g, b));
	}
}

template<typename Pixel>
static void convertHelper(const yuv_buffer& buffer, RawFrame& output,
                          const SDL_PixelFormat& format)
{
	assert(buffer.uv_width  * 2 == buffer.y_width);
	assert(buffer.uv_height * 2 == buffer.y_height);

	const int width      = buffer.y_width;
	const int y_stride   = buffer.y_stride;
	const int uv_stride2 = buffer.uv_stride / 2;

	for (int y = 0; y < buffer.y_height; y += 2) {
		const byte* pY  = buffer.y + y * y_stride;
		const byte* pCb = buffer.u + y * uv_stride2;
		const byte* pCr = buffer.v + y * uv_stride2;
		Pixel* out0 = output.getLinePtrDirect<Pixel>(y + 0);
		Pixel* out1 = output.getLinePtrDirect<Pixel>(y + 1);

		for (int x = 0; x < width;
		     x += 2, pY += 2, ++pCr, ++pCb, out0 += 2, out1 += 2) {
			int ruv = coefs_rv[*pCr];
			int guv = coefs_gu[*pCb] + coefs_gv[*pCr];
			int buv = coefs_bu[*pCb];

			int Y00 = coefs_y[pY[0]];
			out0[0] = calc<Pixel>(format, Y00, ruv, guv, buv);

			int Y01 = coefs_y[pY[1]];
			out0[1] = calc<Pixel>(format, Y01, ruv, guv, buv);

			int Y10 = coefs_y[pY[y_stride + 0]];
			out1[0] = calc<Pixel>(format, Y10, ruv, guv, buv);

			int Y11 = coefs_y[pY[y_stride + 1]];
			out1[1] = calc<Pixel>(format, Y11, ruv, guv, buv);
		}

		output.setLineWidth(y + 0, width);
		output.setLineWidth(y + 1, width);
	}
}

void convert(const yuv_buffer& input, RawFrame& output)
{
	initTables();

	const SDL_PixelFormat& format = output.getSDLPixelFormat();
	if (format.BytesPerPixel == 4) {
#if ASM_X86
		const HostCPU& cpu = HostCPU::getInstance();
		if (cpu.hasSSE2()) {
			convertHelperSSE2(input, output);
		} else if (cpu.hasMMX()) {
			convertHelperMMX(input, output);
		} else
#endif
		{
			convertHelper<unsigned>(input, output, format);
		}
		return;
	} else {
		assert(format.BytesPerPixel == 2);
		convertHelper<short>(input, output, format);
	}
}


} // namespace yuv2rgb
} // namespace openmsx
