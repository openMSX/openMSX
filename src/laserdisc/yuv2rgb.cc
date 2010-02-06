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

/* R = 1.164 * (Y - 16) + 1.596 * (V - 128)
 * G = 1.164 * (Y - 16) - 0.813 * (V - 128) - 0.391 * (U - 128)
 * B = 1.164 * (Y - 16)                     + 2.018 * (U - 128)
 *   OR
 * R = 1.164 * Y + 1.596 * V             - 222.921
 * G = 1.164 * Y - 0.813 * V - 0.391 * U + 135.576
 * B = 1.164 * Y             + 2.018 * U - 276.836
 */

static const unsigned short RED_V   = 0x0066; // 102/64 =  1.59
static const unsigned short GREEN_U = 0xFFE7; // -25/64 = -0.39
static const unsigned short GREEN_V = 0xFFCC; // -52/64 = -0.81
static const unsigned short BLUE_U  = 0x0081; // 129/64 =  2.02
static const unsigned short Y_C     = 0x004A; //  74/64 =  1.16
static const unsigned short ALPHA   = 0xFFFF;
static const unsigned short CNST_R  = 0xFF21; // -222.921
static const unsigned short CNST_G  = 0x0088; //  135.576
static const unsigned short CNST_B  = 0xFEEB; // -276.836
static const unsigned short Y_MASK  = 0x00FF;
static const unsigned short ZERO    = 0x0000;

static const unsigned short coef[11 * 8] __attribute__ ((aligned (16))) = {
	CNST_B,   CNST_B,   CNST_B,   CNST_B,  CNST_B,   CNST_B,   CNST_B,   CNST_B,   // -48
	Y_MASK,   Y_MASK,   Y_MASK,   Y_MASK,  Y_MASK,   Y_MASK,   Y_MASK,   Y_MASK,   // -32
	ZERO,     ZERO,     ZERO,     ZERO,    ZERO,     ZERO,     ZERO,     ZERO,     // -16
	Y_C,      Y_C,      Y_C,      Y_C,     Y_C,      Y_C,      Y_C,      Y_C,      //   0
	GREEN_V,  GREEN_V,  GREEN_V,  GREEN_V, GREEN_V,  GREEN_V,  GREEN_V,  GREEN_V,  //  16
	GREEN_U,  GREEN_U,  GREEN_U,  GREEN_U, GREEN_U,  GREEN_U,  GREEN_U,  GREEN_U,  //  32
	BLUE_U,   BLUE_U,   BLUE_U,   BLUE_U,  BLUE_U,   BLUE_U,   BLUE_U,   BLUE_U,   //  48
	RED_V,    RED_V,    RED_V,    RED_V,   RED_V,    RED_V,    RED_V,    RED_V,    //  64
	ALPHA,    ALPHA,    ALPHA,    ALPHA,   ALPHA,    ALPHA,    ALPHA,    ALPHA,    //  80
	CNST_R,   CNST_R,   CNST_R,   CNST_R,  CNST_R,   CNST_R,   CNST_R,   CNST_R,   //  96
	CNST_G,   CNST_G,   CNST_G,   CNST_G,  CNST_G,   CNST_G,   CNST_G,   CNST_G,   // 112
};

static inline void yuv2rgb_sse2(
	const void* u, const void* v, const void* y0, const void*y1,
	void* out0, void* out1)
{
	// The following asm code is split in 4 blocks. The reason for this
	// split is to be able to satisfy the register constraints: with one
	// big block we need 7 free registers. After this split we only need
	// maximum 5 registers in (some of the) blocks.
	//
	// Each block calculates 16 RGB values. Each block uses 16 unique 'Y'
	// values, but all 4 blocks share the same 'Cr' and 'Cb' values.

	__asm__ __volatile__ (
		"movdqa (%[U]),%%xmm2;"            // xmm2 = u0f
		"movdqa (%[V]),%%xmm3;"            // xmm3 = v0f
		"punpcklbw -16(%[COEF]),%%xmm2;"   // xmm2 = u07    (ZERO)
		"punpcklbw -16(%[COEF]),%%xmm3;"   // xmm3 = v07    (ZERO)
		"movdqa %%xmm3,%%xmm4;"            // xmm4 = v07
		"pmullw 64(%[COEF]),%%xmm4;"       // xmm4 = v07 * RED_V
		"pmullw 16(%[COEF]),%%xmm3;"       // xmm3 = v07 * GREEN_V
		"movdqa %%xmm2,%%xmm5;"            // xmm5 = u07
		"pmullw 32(%[COEF]),%%xmm5;"       // xmm5 = u07 * GREEN_U
		"pmullw 48(%[COEF]),%%xmm2;"       // xmm2 = u07 * BLUE_U
		"psraw  $0x6,%%xmm4;"              // xmm4 = vr07
		"paddsw 96(%[COEF]),%%xmm4;"       // xmm4 = dr07      (CNST_R)
		"paddsw %%xmm5,%%xmm3;"            // xmm3 = (vg07 + ug07) * 64
		"psraw  $0x6,%%xmm3;"              // xmm3 = vg07 + ug07
		"movdqa %%xmm4,%%xmm5;"            // xmm5 = dr07
		"paddsw 112(%[COEF]),%%xmm3;"      // xmm3 = dg07      (CNST_G)
		"movdqa (%[Y0]),%%xmm0;"           // xmm0 = y00_0f
		"movdqa %%xmm0,%%xmm1;"            // xmm1 = y00_0f
		"pand   -32(%[COEF]),%%xmm1;"      // xmm1 = y00_even  (Y_MASK)
		"psrlw  $0x6,%%xmm2;"              // xmm2 = ub07  ** unsigned **
		"pmullw (%[COEF]),%%xmm1;"         // xmm1 = y00_even * COEF_Y
		"paddsw -48(%[COEF]),%%xmm2;"      // xmm2 = db07      (CST_B)
		"psraw  $0x6,%%xmm1;"              // xmm1 = dy00_even
		"psrlw  $0x8,%%xmm0;"              // xmm0 = y00_odd
		"paddsw %%xmm1,%%xmm5;"            // xmm5 = r00_even
		"pmullw (%[COEF]),%%xmm0;"         // xmm0 = y00_odd * COEF_Y
		"movdqa %%xmm3,%%xmm6;"            // xmm6 = dg07
		"paddsw %%xmm1,%%xmm6;"            // xmm6 = g00_even
		"psraw  $0x6,%%xmm0;"              // xmm0 = dy00_odd
		"paddsw %%xmm2,%%xmm1;"            // xmm1 = b00_even
		"packuswb %%xmm5,%%xmm5;"          // xmm5 = r00_even2
		"movdqa %%xmm4,%%xmm7;"            // xmm7 = dr07
		"paddsw %%xmm0,%%xmm7;"            // xmm7 = r00_odd
		"packuswb %%xmm7,%%xmm7;"          // xmm7 = r00_odd2
		"punpcklbw %%xmm7,%%xmm5;"         // xmm5 = r00_0f
		"movdqa %%xmm3,%%xmm7;"            // xmm7 = dg07
		"paddsw %%xmm0,%%xmm7;"            // xmm7 = g00_odd
		"packuswb %%xmm7,%%xmm7;"          // xmm7 = g00_odd2
		"paddsw %%xmm2,%%xmm0;"            // xmm0 = b00_odd
		"packuswb %%xmm0,%%xmm0;"          // xmm0 = b00_odd2
		"packuswb %%xmm6,%%xmm6;"          // xmm6 = g00_even2
		"packuswb %%xmm1,%%xmm1;"          // xmm1 = b00_even2
		"punpcklbw %%xmm7,%%xmm6;"         // xmm6 = g00_0f
		"punpcklbw %%xmm0,%%xmm1;"         // xmm1 = b00_0f
		"movdqa %%xmm6,%%xmm7;"            // xmm7 = g00_0f
		"movdqa %%xmm1,%%xmm0;"            // xmm0 = b00_0f
		"punpckhbw 80(%[COEF]),%%xmm6;"    // xmm6 = ga00_8f   (ALPHA)
		"punpcklbw 80(%[COEF]),%%xmm7;"    // xmm7 = ga00_07   (ALPHA)
		"punpcklbw %%xmm5,%%xmm0;"         // xmm0 = br00_07
		"punpckhbw %%xmm5,%%xmm1;"         // xmm1 = br00_8f
		"movdqa %%xmm0,%%xmm5;"            // xmm5 = br00_07
		"punpcklbw %%xmm7,%%xmm5;"         // xmm5 = bgra00_03
		"punpckhbw %%xmm7,%%xmm0;"         // xmm0 = bgra00_47
		"movdqa %%xmm5,(%[OUT0]);"         // OUT0[0] <- bgra00_03
		"movdqa %%xmm0,0x10(%[OUT0]);"     // OUT0[1] <- bgra00_47
		"movdqa %%xmm1,%%xmm5;"            // xmm5 = br00_8f
		"punpcklbw %%xmm6,%%xmm5;"         // xmm5 = bgra00_8b
		"punpckhbw %%xmm6,%%xmm1;"         // xmm1 = bgra00_cf
		"movdqa %%xmm5,0x20(%[OUT0]);"     // OUT0[2] <- bgra00_8b
		"movdqa %%xmm1,0x30(%[OUT0]);"     // OUT0[3] <- bgra00_cf
		: // no output
		: [U]    "r" (u)
		, [V]    "r" (v)
		, [Y0]   "r" (y0)
		, [OUT0] "r" (out0)
		, [COEF] "r" (coef + 3 * 8)
		#ifdef __SSE2__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
		#endif
	);
	// At this point the following registers are still live:
	//  xmm2 = db07
	//  xmm3 = dg07
	//  xmm4 = dr07
	__asm__ __volatile__ (
		"movdqa %%xmm4,%%xmm6;"            // xmm6 = dr07
		"movdqa (%[Y1]),%%xmm7;"           // xmm7 = y10_0f
		"movdqa %%xmm3,%%xmm5;"            // xmm5 = dg07
		"movdqa %%xmm7,%%xmm1;"            // xmm1 = y10_0f
		"pand   -32(%[COEF]),%%xmm1;"      // xmm1 = y10_even   (Y_MASK)
		"psrlw  $0x8,%%xmm7;"              // xmm7 = y10_odd
		"pmullw (%[COEF]),%%xmm1;"         // xmm1 = y10_even * COEF_Y
		"pmullw (%[COEF]),%%xmm7;"         // xmm7 = y10_odd  * COEF_Y
		"psraw  $0x6,%%xmm1;"              // xmm1 = dy10_even
		"psraw  $0x6,%%xmm7;"              // xmm7 = dy10_odd
		"paddsw %%xmm1,%%xmm6;"            // xmm6 = r10_even
		"paddsw %%xmm7,%%xmm4;"            // xmm4 = r10_odd
		"paddsw %%xmm1,%%xmm5;"            // xmm5 = g10_even
		"packuswb %%xmm4,%%xmm4;"          // xmm4 = r10_odd2
		"paddsw %%xmm7,%%xmm3;"            // xmm3 = g10_odd
		"paddsw %%xmm2,%%xmm1;"            // xmm1 = b10_even
		"packuswb %%xmm3,%%xmm3;"          // xmm3 = g10_odd2
		"paddsw %%xmm7,%%xmm2;"            // xmm2 = b10_odd
		"packuswb %%xmm6,%%xmm6;"          // xmm6 = r10_even2
		"packuswb %%xmm2,%%xmm2;"          // xmm2 = b10_odd2
		"punpcklbw %%xmm4,%%xmm6;"         // xmm6 = r10_0f
		"packuswb %%xmm5,%%xmm5;"          // xmm5 = g10_even2
		"packuswb %%xmm1,%%xmm1;"          // xmm1 = b10_even2
		"punpcklbw %%xmm3,%%xmm5;"         // xmm5 = g10_0f
		"punpcklbw %%xmm2,%%xmm1;"         // xmm1 = b10_0f
		"movdqa %%xmm5,%%xmm3;"            // xmm3 = g10_0f
		"movdqa %%xmm1,%%xmm2;"            // xmm2 = b10_0f
		"punpcklbw 80(%[COEF]),%%xmm3;"    // xmm3 = ga10_07   (ALPHA)
		"punpckhbw 80(%[COEF]),%%xmm5;"    // xmm5 = ga10_8f   (ALPHA)
		"punpcklbw %%xmm6,%%xmm2;"         // xmm2 = br10_07
		"punpckhbw %%xmm6,%%xmm1;"         // xmm1 = br10_8f
		"movdqa %%xmm2,%%xmm4;"            // xmm4 = br10_07
		"punpckhbw %%xmm3,%%xmm2;"         // xmm2 = bgra10_47
		"punpcklbw %%xmm3,%%xmm4;"         // xmm4 = bgra10_03
		"movdqa %%xmm1,%%xmm3;"            // xmm3 = br10_8f
		"movdqa %%xmm4,(%[OUT1]);"         // OUT1[0] <- bgra10_03
		"movdqa %%xmm2,0x10(%[OUT1]);"     // OUT1[1] <- bgra10_47
		"punpcklbw %%xmm5,%%xmm3;"         // xmm3 = bgra10_8b
		"punpckhbw %%xmm5,%%xmm1;"         // xmm1 = bgra10_cf
		"movdqa %%xmm3,0x20(%[OUT1]);"     // OUT1[2] <- bgra10_8b
		"movdqa %%xmm1,0x30(%[OUT1]);"     // OUT1[3] <- bgra10_cf
		: // no output
		: [Y1]   "r" (y1)
		, [OUT1] "r" (out1)
		, [COEF] "r" (coef + 3 * 8)
		#ifdef __SSE2__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
		#endif
	);
	// At this point no (xmm?) registers are live.
	__asm__ __volatile__ (
		"movdqa (%[U]),%%xmm2;"            // xmm2 = u0f
		"movdqa (%[V]),%%xmm3;"            // xmm3 = v0f
		"punpckhbw -16(%[COEF]),%%xmm2;"   // xmm2 = u8f    (ZERO)
		"punpckhbw -16(%[COEF]),%%xmm3;"   // xmm3 = v8f    (ZERO)
		"movdqa %%xmm3,%%xmm4;"            // xmm4 = v8f
		"pmullw 64(%[COEF]),%%xmm4;"       // xmm4 = v8f * RED_V
		"pmullw 16(%[COEF]),%%xmm3;"       // xmm3 = v8f * GREEN_V
		"movdqa %%xmm2,%%xmm5;"            // xmm5 = u8f
		"pmullw 32(%[COEF]),%%xmm5;"       // xmm5 = u8f * GREEN_U
		"pmullw 48(%[COEF]),%%xmm2;"       // xmm2 = u8f * BLUE_U
		"psraw  $0x6,%%xmm4;"              // xmm4 = vr8f
		"paddsw 96(%[COEF]),%%xmm4;"       // xmm4 = dr8f      (CNST_R)
		"paddsw %%xmm5,%%xmm3;"            // xmm3 = (vg8f + ug8f) * 64
		"psraw  $0x6,%%xmm3;"              // xmm3 = vg8f + ug8f
		"movdqa %%xmm4,%%xmm5;"            // xmm5 = dr8f
		"paddsw 112(%[COEF]),%%xmm3;"      // xmm3 = dg8f      (CNST_G)
		"movdqa 0x10(%[Y0]),%%xmm0;"       // xmm0 = y01_0f
		"movdqa %%xmm0,%%xmm1;"            // xmm1 = y01_0f
		"pand   -32(%[COEF]),%%xmm1;"      // xmm1 = y01_even  (Y_MASK)
		"psrlw  $0x6,%%xmm2;"              // xmm2 = ub8f  **unsigned**
		"pmullw (%[COEF]),%%xmm1;"         // xmm1 = y01_even * COEF_Y
		"paddsw -48(%[COEF]),%%xmm2;"      // xmm2 = db8f      (CST_B)
		"psraw  $0x6,%%xmm1;"              // xmm1 = dy01_even
		"psrlw  $0x8,%%xmm0;"              // xmm0 = y01_odd
		"paddsw %%xmm1,%%xmm5;"            // xmm5 = r01_even
		"pmullw (%[COEF]),%%xmm0;"         // xmm0 = y01_odd * COEF_Y
		"movdqa %%xmm3,%%xmm6;"            // xmm6 = dg8f
		"paddsw %%xmm1,%%xmm6;"            // xmm6 = g01_even
		"psraw  $0x6,%%xmm0;"              // xmm0 = dy01_odd
		"paddsw %%xmm2,%%xmm1;"            // xmm1 = b01_even
		"packuswb %%xmm5,%%xmm5;"          // xmm5 = r01_even2
		"movdqa %%xmm4,%%xmm7;"            // xmm7 = dr8f
		"paddsw %%xmm0,%%xmm7;"            // xmm7 = r01_odd
		"packuswb %%xmm7,%%xmm7;"          // xmm7 = r01_odd2
		"punpcklbw %%xmm7,%%xmm5;"         // xmm5 = r01_0f
		"movdqa %%xmm3,%%xmm7;"            // xmm7 = dg8f
		"paddsw %%xmm0,%%xmm7;"            // xmm7 = g01_odd
		"packuswb %%xmm7,%%xmm7;"          // xmm7 = g01_odd2
		"paddsw %%xmm2,%%xmm0;"            // xmm0 = b01_odd
		"packuswb %%xmm0,%%xmm0;"          // xmm0 = b01_odd2
		"packuswb %%xmm6,%%xmm6;"          // xmm6 = g01_even2
		"packuswb %%xmm1,%%xmm1;"          // xmm1 = b01_even2
		"punpcklbw %%xmm7,%%xmm6;"         // xmm6 = g01_0f
		"punpcklbw %%xmm0,%%xmm1;"         // xmm1 = b01_0f
		"movdqa %%xmm6,%%xmm7;"            // xmm7 = g01_0f
		"movdqa %%xmm1,%%xmm0;"            // xmm0 = b01_0f
		"punpckhbw 80(%[COEF]),%%xmm6;"    // xmm6 = ga01_8f   (ALPHA)
		"punpcklbw 80(%[COEF]),%%xmm7;"    // xmm7 = ga01_8f   (ALPHA)
		"punpcklbw %%xmm5,%%xmm0;"         // xmm0 = br01_8f
		"punpckhbw %%xmm5,%%xmm1;"         // xmm1 = br01_8f
		"movdqa %%xmm0,%%xmm5;"            // xmm5 = br01_8f
		"punpcklbw %%xmm7,%%xmm5;"         // xmm5 = bgra01_03
		"punpckhbw %%xmm7,%%xmm0;"         // xmm0 = bgra01_47
		"movdqa %%xmm5,0x40(%[OUT0]);"     // OUT0[0] <- bgra01_03
		"movdqa %%xmm0,0x50(%[OUT0]);"     // OUT0[1] <- bgra01_47
		"movdqa %%xmm1,%%xmm5;"            // xmm5 = br01_8f
		"punpcklbw %%xmm6,%%xmm5;"         // xmm5 = bgra01_8b
		"punpckhbw %%xmm6,%%xmm1;"         // xmm1 = bgra01_cf
		"movdqa %%xmm5,0x60(%[OUT0]);"     // OUT0[2] <- bgra01_8b
		"movdqa %%xmm1,0x70(%[OUT0]);"     // OUT0[3] <- bgra01_cf
		: // no output
		: [U]    "r" (u)
		, [V]    "r" (v)
		, [Y0]   "r" (y0)
		, [OUT0] "r" (out0)
		, [COEF] "r" (coef + 3 * 8)
		#ifdef __SSE2__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
		#endif
	);
	// At this point the following registers are still live:
	//  xmm2 = db8f
	//  xmm3 = dg8f
	//  xmm4 = dr8f
	__asm__ __volatile__ (
		"movdqa %%xmm4,%%xmm6;"            // xmm6 = dr8f
		"movdqa 0x10(%[Y1]),%%xmm7;"       // xmm7 = y11_0f
		"movdqa %%xmm3,%%xmm5;"            // xmm5 = dg8f
		"movdqa %%xmm7,%%xmm1;"            // xmm1 = y11_0f
		"pand   -32(%[COEF]),%%xmm1;"      // xmm1 = y11_even   (Y_MASK)
		"psrlw  $0x8,%%xmm7;"              // xmm7 = y11_odd
		"pmullw (%[COEF]),%%xmm1;"         // xmm1 = y11_even * COEF_Y
		"pmullw (%[COEF]),%%xmm7;"         // xmm7 = y11_odd  * COEF_Y
		"psraw  $0x6,%%xmm1;"              // xmm1 = dy11_even
		"psraw  $0x6,%%xmm7;"              // xmm7 = dy11_odd
		"paddsw %%xmm1,%%xmm6;"            // xmm6 = r11_even
		"paddsw %%xmm7,%%xmm4;"            // xmm4 = r11_odd
		"paddsw %%xmm1,%%xmm5;"            // xmm5 = g11_even
		"packuswb %%xmm4,%%xmm4;"          // xmm4 = r11_odd2
		"paddsw %%xmm7,%%xmm3;"            // xmm3 = g11_odd
		"paddsw %%xmm2,%%xmm1;"            // xmm1 = b11_even
		"packuswb %%xmm3,%%xmm3;"          // xmm3 = g11_odd2
		"paddsw %%xmm7,%%xmm2;"            // xmm2 = b11_odd
		"packuswb %%xmm6,%%xmm6;"          // xmm6 = r11_even2
		"packuswb %%xmm2,%%xmm2;"          // xmm2 = b11_odd2
		"punpcklbw %%xmm4,%%xmm6;"         // xmm6 = r11_0f
		"packuswb %%xmm5,%%xmm5;"          // xmm5 = g11_even2
		"packuswb %%xmm1,%%xmm1;"          // xmm1 = b11_even2
		"punpcklbw %%xmm3,%%xmm5;"         // xmm5 = g11_0f
		"punpcklbw %%xmm2,%%xmm1;"         // xmm1 = b11_0f
		"movdqa %%xmm5,%%xmm3;"            // xmm3 = g11_0f
		"movdqa %%xmm1,%%xmm2;"            // xmm2 = b11_0f
		"punpcklbw 80(%[COEF]),%%xmm3;"    // xmm3 = ga11_8f   (ALPHA)
		"punpckhbw 80(%[COEF]),%%xmm5;"    // xmm5 = ga11_8f   (ALPHA)
		"punpcklbw %%xmm6,%%xmm2;"         // xmm2 = br11_8f
		"punpckhbw %%xmm6,%%xmm1;"         // xmm1 = br11_8f
		"movdqa %%xmm2,%%xmm4;"            // xmm4 = br11_8f
		"punpckhbw %%xmm3,%%xmm2;"         // xmm2 = bgra11_47
		"punpcklbw %%xmm3,%%xmm4;"         // xmm4 = bgra11_03
		"movdqa %%xmm1,%%xmm3;"            // xmm3 = br11_8f
		"movdqa %%xmm4,0x40(%[OUT1]);"     // OUT1[0] <- bgra11_03
		"movdqa %%xmm2,0x50(%[OUT1]);"     // OUT1[1] <- bgra11_47
		"punpcklbw %%xmm5,%%xmm3;"         // xmm3 = bgra11_8b
		"punpckhbw %%xmm5,%%xmm1;"         // xmm1 = bgra11_cf
		"movdqa %%xmm3,0x60(%[OUT1]);"     // OUT1[2] <- bgra11_8b
		"movdqa %%xmm1,0x70(%[OUT1]);"     // OUT1[3] <- bgra11_cf
		: // no output
		: [Y1]   "r" (y1)
		, [OUT1] "r" (out1)
		, [COEF] "r" (coef + 3 * 8)
		#ifdef __SSE2__
		: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
		#endif
	);
}

static void convertHelperSSE2(const th_ycbcr_buffer& buffer, RawFrame& output)
{
	const int width      = buffer[0].width;
	const int y_stride   = buffer[0].stride;
	const int uv_stride2 = buffer[1].stride / 2;

	for (int y = 0; y < buffer[0].height; y += 2) {
		const byte* pY1 = buffer[0].data + y * y_stride;
		const byte* pY2 = buffer[0].data + (y + 1) * y_stride;
		const byte* pCb = buffer[1].data + y * uv_stride2;
		const byte* pCr = buffer[2].data + y * uv_stride2;
		unsigned* out0 = output.getLinePtrDirect<unsigned>(y + 0);
		unsigned* out1 = output.getLinePtrDirect<unsigned>(y + 1);

		for (int x = 0; x < width; x += 32) {
			// convert a block of (32 x 2) pixels
			yuv2rgb_sse2(pCb, pCr, pY1, pY2, out0, out1);
			pCb += 16;
			pCr += 16;
			pY1 += 32;
			pY2 += 32;
			out0 += 32;
			out1 += 32;
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
static void convertHelper(const th_ycbcr_buffer& buffer, RawFrame& output,
                          const SDL_PixelFormat& format)
{
	assert(buffer[1].width  * 2 == buffer[0].width);
	assert(buffer[1].height * 2 == buffer[0].height);

	const int width      = buffer[0].width;
	const int y_stride   = buffer[0].stride;
	const int uv_stride2 = buffer[1].stride / 2;

	for (int y = 0; y < buffer[0].height; y += 2) {
		const byte* pY  = buffer[0].data + y * y_stride;
		const byte* pCb = buffer[1].data + y * uv_stride2;
		const byte* pCr = buffer[2].data + y * uv_stride2;
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

void convert(const th_ycbcr_buffer& input, RawFrame& output)
{
	initTables();

	const SDL_PixelFormat& format = output.getSDLPixelFormat();
	if (format.BytesPerPixel == 4) {
#if ASM_X86
		const HostCPU& cpu = HostCPU::getInstance();
		if (cpu.hasSSE2()) {
			convertHelperSSE2(input, output);
		} else
#endif
		{
			convertHelper<unsigned>(input, output, format);
		}
	} else {
		assert(format.BytesPerPixel == 2);
		convertHelper<short>(input, output, format);
	}
}

} // namespace yuv2rgb
} // namespace openmsx
