// $Id$

#include "SimpleScaler.hh"
#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "HostCPU.hh"
#include "openmsx.hh"
#include <cassert>


namespace openmsx {

// class Multiply<unsigned>

Multiply<unsigned>::Multiply(SDL_PixelFormat* /*format*/)
{
}

inline unsigned Multiply<unsigned>::multiply(unsigned p, unsigned f)
{
	return ((((p & 0xFF00FF) * f) & 0xFF00FF00) |
	        (((p & 0x00FF00) * f) & 0x00FF0000)) >> 8;
}

inline void Multiply<unsigned>::setFactor(unsigned f)
{
	factor = f;
}

inline unsigned Multiply<unsigned>::mul32(unsigned p)
{
	return (((p & 0xFF00FF) * factor) & 0xFF00FF00) |
	       (((p & 0x00FF00) * factor) & 0x00FF0000);
}

inline unsigned Multiply<unsigned>::conv32(unsigned p)
{
	return p >> 8;
}


// class Multiply<word>

// gcc can optimize these rotate functions to just one instruction.
// We don't really need a rotate, but we do need a shift over a positive or
// negative (not known at compile time) distance, rotate handles this just fine.
static inline unsigned rotLeft(unsigned a, unsigned n)
{
	return (a << n) | (a >> (32 - n));
}
static inline unsigned rotRight(unsigned a, unsigned n)
{
	return (a >> n) | (a << (32 - n));
}

Multiply<word>::Multiply(SDL_PixelFormat* format)
{
	Rmask1 = format->Rmask;;
	Gmask1 = format->Gmask;
	Bmask1 = format->Bmask;

	Rshift1 = (2 + format->Rloss) - format->Rshift;
	Gshift1 = (2 + format->Gloss) - format->Gshift;
	Bshift1 = (2 + format->Bloss) - format->Bshift;

	Rmask2 = ((1 << (2 + format->Rloss)) - 1) <<
	                (10 + format->Rshift - 2 * (2 + format->Rloss));
	Gmask2 = ((1 << (2 + format->Gloss)) - 1) <<
	                (10 + format->Gshift - 2 * (2 + format->Gloss));
	Bmask2 = ((1 << (2 + format->Bloss)) - 1) <<
	                (10 + format->Bshift - 2 * (2 + format->Bloss));

	Rshift2 = 2 * (2 + format->Rloss) - format->Rshift - 10;
	Gshift2 = 2 * (2 + format->Gloss) - format->Gshift - 10;
	Bshift2 = 2 * (2 + format->Bloss) - format->Bshift - 10;
	
	Rshift3 = Rshift1 + 0;
	Gshift3 = Gshift1 + 10;
	Bshift3 = Bshift1 + 20;

	factor = 0;
	memset(tab, 0, sizeof(tab));
}

inline word Multiply<word>::multiply(word p, unsigned f)
{
	unsigned r = (((p & Rmask1) * f) >> 8) & Rmask1;
	unsigned g = (((p & Gmask1) * f) >> 8) & Gmask1;
	unsigned b = (((p & Bmask1) * f) >> 8) & Bmask1;
	return r | g | b;
	
}

void Multiply<word>::setFactor(unsigned f)
{
	if (factor == f) {
		return;
	}
	factor = f;

	for (unsigned p = 0; p < 0x10000; ++p) {
		unsigned r = rotLeft((p & Rmask1), Rshift1) |
			     rotLeft((p & Rmask2), Rshift2);
		unsigned g = rotLeft((p & Gmask1), Gshift1) |
			     rotLeft((p & Gmask2), Gshift2);
		unsigned b = rotLeft((p & Bmask1), Bshift1) |
			     rotLeft((p & Bmask2), Bshift2);
		tab[p] = (((r * factor) >> 8) <<  0) |
		         (((g * factor) >> 8) << 10) |
		         (((b * factor) >> 8) << 20);
	}
}

inline unsigned Multiply<word>::mul32(word p)
{
	return tab[p];
}

inline word Multiply<word>::conv32(unsigned p)
{
	return (rotRight(p, Rshift3) & Rmask1) |
	       (rotRight(p, Gshift3) & Gmask1) |
	       (rotRight(p, Bshift3) & Bmask1);
}


// class Darkener

Darkener<word>::Darkener(SDL_PixelFormat* format_)
	: format(format_)
{
	factor = 0;
	memset(tab, 0, sizeof(tab));
}

void Darkener<word>::setFactor(unsigned f)
{
	if (f == factor) {
		return;
	}
	factor = f;

	for (unsigned p = 0; p < 0x10000; ++p) {
		tab[p] = ((((p & format->Rmask) * f) >> 8) & format->Rmask) |
		         ((((p & format->Gmask) * f) >> 8) & format->Gmask) |
		         ((((p & format->Bmask) * f) >> 8) & format->Bmask);
	}
}

word* Darkener<word>::getTable()
{
	return tab;
}


Darkener<unsigned>::Darkener(SDL_PixelFormat* /*format*/)
{
}

void Darkener<unsigned>::setFactor(unsigned /*f*/)
{
	assert(false);
}

word* Darkener<unsigned>::getTable()
{
	assert(false);
	return NULL;
}


// class SimpleScaler

template <class Pixel>
SimpleScaler<Pixel>::SimpleScaler(SDL_PixelFormat* format)
	: scanlineSetting(*RenderSettings::instance().getScanlineAlpha())
	, blurSetting(*RenderSettings::instance().getHorizontalBlur())
	, blender(Blender<Pixel>::createFromFormat(format))
	, mult1(format)
	, mult2(format)
	, mult3(format)
	, darkener(format)
{
}

template <class Pixel>
SimpleScaler<Pixel>::~SimpleScaler()
{
}

template <class Pixel>
void SimpleScaler<Pixel>::scaleBlank(Pixel colour, SDL_Surface* dst,
                                     int dstY, int endDstY)
{
	Pixel scanlineColour = scanlineSetting.getValue() == 0
		? colour
		: mult1.multiply(colour,
		                255 - (scanlineSetting.getValue() * 255) / 100);

	while (dstY < endDstY) {
		Pixel* dstUpper = Scaler<Pixel>::linePtr(dst, dstY++);
		Scaler<Pixel>::fillLine(dstUpper, colour, dst->w);
		if (dstY == endDstY) break;
		Pixel* dstLower = Scaler<Pixel>::linePtr(dst, dstY++);
		Scaler<Pixel>::fillLine(dstLower, scanlineColour, dst->w);
	}
}

template <class Pixel>
void SimpleScaler<Pixel>::blur256(const Pixel* pIn, Pixel* pOut, unsigned alpha)
{
	/* This routine is functionally equivalent to the following:
	 * 
	 * void blur256(const Pixel* pIn, Pixel* pOut, unsigned alpha)
	 * {
	 *         unsigned c1 = alpha;
	 *         unsigned c2 = 256 - c1;
	 * 
	 *         Pixel prev, curr, next;
	 *         prev = curr = pIn[0];
	 * 
	 *         unsigned x;
	 *         for (x = 0; x < (320 - 1); ++x) {
	 *                 pOut[2 * x + 0] = (c1 * prev + c2 * curr) >> 8;
	 *                 Pixel next = pIn[x + 1];
	 *                 pOut[2 * x + 1] = (c1 * next + c2 * curr) >> 8;
	 *                 prev = curr;
	 *                 curr = next;
	 *         }
	 * 
	 *         pOut[2 * x + 0] = (c1 * prev + c2 * curr) >> 8;
	 *         next = curr;
	 *         pOut[2 * x + 1] = (c1 * next + c2 * curr) >> 8;
	 * }
	 * 
	 * The loop is 2x unrolled and all common subexpressions and redundant
	 * assignments have been eliminated. 1 loop iteration processes 4
	 * (output) pixels.
	 */

	if (alpha == 0) {
		Scaler<Pixel>::scaleLine(pIn, pOut, 320, true); // in cache
		return;
	}

	assert(alpha <= 256);
	unsigned c1 = alpha / 4;
	unsigned c2 = 256 - c1;

	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if ((sizeof(Pixel) == 4) && cpu.hasMMX()) { // Note: not hasMMXEXT()
		// MMX routine, 32bpp
		asm (
			"xorl	%%eax, %%eax;"
			"movd	%2, %%mm5;"
			"punpcklwd %%mm5, %%mm5;"
			"punpckldq %%mm5, %%mm5;"	// mm5 = c1
			"movd	%3, %%mm6;"
			"punpcklwd %%mm6, %%mm6;"
			"punpckldq %%mm6, %%mm6;"	// mm6 = c2
			"pxor	%%mm7, %%mm7;"
			
			"movd	(%0,%%eax,4), %%mm0;"
			"punpcklbw %%mm7, %%mm0;"	// p0 = pIn[0]
			"movq	%%mm0, %%mm2;"
			"pmullw	%%mm5, %%mm2;"		// f0 = multiply(p0, c1)
			"movq	%%mm2, %%mm3;"		// f1 = f0
			
			".p2align 4,,15;"
		"1:"
			"pmullw	%%mm6, %%mm0;"
			"movq	%%mm0, %%mm4;"		// tmp = multiply(p0, c2)
			"paddw	%%mm3, %%mm0;"
			"psrlw	$8, %%mm0;"		// f1 + tmp

			"movd	4(%0,%%eax,4), %%mm1;"
			"punpcklbw %%mm7, %%mm1;"	// p1 = pIn[x + 1]
			"movq	%%mm1, %%mm3;"
			"pmullw	%%mm5, %%mm3;"		// f1 = multiply(p1, c1)
			"paddw	%%mm3, %%mm4;"
			"psrlw	$8, %%mm4;"		// f1 + tmp
			"packuswb %%mm4, %%mm0;"
			"movq	%%mm0, (%1,%%eax,8);"	// pOut[2*x+0] = ..  pOut[2*x+1] = ..

			"pmullw	%%mm6, %%mm1;"
			"movq	%%mm1, %%mm4;"		// tmp = multiply(p1, c2)
			"paddw	%%mm2, %%mm1;"
			"psrlw	$8, %%mm1;"		// f0 + tmp

			"movd	8(%0,%%eax,4), %%mm0;"
			"punpcklbw %%mm7, %%mm0;"	// p0 = pIn[x + 2]
			"movq	%%mm0, %%mm2;"
			"pmullw %%mm5, %%mm2;"		// f0 = multiply(p0, c1)
			"paddw	%%mm2, %%mm4;"
			"psrlw	$8, %%mm4;"		// f0 + tmp
			"packuswb %%mm4, %%mm1;"
			"movq	%%mm1, 8(%1,%%eax,8);"	// pOut[2*x+2] = ..  pOut[2*x+3] = ..

			"addl	$2, %%eax;"
			"cmpl	$318, %%eax;"
			"jl	1b;"

			"pmullw	%%mm6, %%mm0;"
			"movq	%%mm0, %%mm4;"		// tmp = multiply(p0, c2)
			"paddw	%%mm3, %%mm0;"
			"psrlw	$8, %%mm0;"		// f1 + tmp

			"movd	4(%0,%%eax,4), %%mm1;"
			"punpcklbw %%mm7, %%mm1;"	// p1 = pIn[x + 1]
			"movq	%%mm1, %%mm3;"
			"pmullw	%%mm5, %%mm3;"		// f1 = multiply(p1, c1)
			"paddw	%%mm3, %%mm4;"
			"psrlw	$8, %%mm4;"		// f1 + tmp
			"packuswb %%mm4, %%mm0;"
			"movq	%%mm0, (%1,%%eax,8);"	// pOut[2*x+0] = ..  pOut[2*x+1] = ..
			
			"movq	%%mm1, %%mm4;"
			"pmullw	%%mm6, %%mm1;" 		// tmp = multiply(p1, c2)
			"paddw	%%mm2, %%mm1;"
			"psrlw	$8, %%mm1;"		// f0 + tmp

			"packuswb %%mm4, %%mm1;"
			"movq	%%mm1, 8(%1,%%eax,8);"	// pOut[2*x+0] = ..  pOut[2*x+1] = ..
			
			"emms;"

			: // no output
			: "r" (pIn)   // 0
			, "r" (pOut)  // 1
			, "r" (c1)    // 2
			, "r" (c2)    // 3
			: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
			, "eax"
		);
		return;
	}
	#endif
	
	// non-MMX routine, both 16bpp and 32bpp
	mult1.setFactor(c1);
	mult2.setFactor(c2);
	 
	Pixel p0 = pIn[0];
	Pixel p1;
	unsigned f0 = mult1.mul32(p0);
	unsigned f1 = f0;
	unsigned tmp;

	unsigned x;
	for (x = 0; x < (320 - 2); x += 2) {
		tmp = mult2.mul32(p0);
		pOut[2 * x + 0] = mult1.conv32(f1 + tmp);

		p1 = pIn[x + 1];
		f1 = mult1.mul32(p1);
		pOut[2 * x + 1] = mult1.conv32(f1 + tmp);

		tmp = mult2.mul32(p1);
		pOut[2 * x + 2] = mult1.conv32(f0 + tmp);

		p0 = pIn[x + 2];
		f0 = mult1.mul32(p0);
		pOut[2 * x + 3] = mult1.conv32(f0 + tmp);
	}

	tmp = mult2.mul32(p0);
	pOut[2 * x + 0] = mult1.conv32(f1 + tmp);

	p1 = pIn[x + 1];
	f1 = mult1.mul32(p1);
	pOut[2 * x + 1] = mult1.conv32(f1 + tmp);

	tmp = mult2.mul32(p1);
	pOut[2 * x + 2] = mult1.conv32(f0 + tmp);

	pOut[2 * x + 3] = p1;
}

template <class Pixel>
void SimpleScaler<Pixel>::blur512(const Pixel* pIn, Pixel* pOut, unsigned alpha)
{
	/* This routine is functionally equivalent to the following:
	 * 
	 * void blur512(const Pixel* pIn, Pixel* pOut, unsigned alpha)
	 * {
	 *         unsigned c1 = alpha / 2;
	 *         unsigned c2 = 256 - alpha;
	 * 
	 *         Pixel prev, curr, next;
	 *         prev = curr = pIn[0];
	 * 
	 *         unsigned x;
	 *         for (x = 0; x < (640 - 1); ++x) {
	 *                 next = pIn[x + 1];
	 *                 pOut[x] = (c1 * prev + c2 * curr + c1 * next) >> 8;
	 *                 prev = curr;
	 *                 curr = next;
	 *         }
	 * 
	 *         next = curr;
	 *         pOut[x] = c1 * prev + c2 * curr + c1 * next;
	 * }
	 *
	 * The loop is 2x unrolled and all common subexpressions and redundant
	 * assignments have been eliminated. 1 loop iteration processes 2
	 * pixels.
	 */

	if (alpha == 0) {
		Scaler<Pixel>::copyLine(pIn, pOut, 640, true); // in cache
		return;
	}

	unsigned c1 = alpha / 4;
	unsigned c2 = 256 - alpha / 2;

	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if ((sizeof(Pixel) == 4) && cpu.hasMMX()) { // Note: not hasMMXEXT()
		// MMX routine, 32bpp
		asm (
			"xorl	%%eax, %%eax;"
			"movd	%2, %%mm5;"
			"punpcklwd %%mm5, %%mm5;"
			"punpckldq %%mm5, %%mm5;"	// mm5 = c1
			"movd	%3, %%mm6;"
			"punpcklwd %%mm6, %%mm6;"
			"punpckldq %%mm6, %%mm6;"	// mm6 = c2
			"pxor	%%mm7, %%mm7;"
		
			"movd	(%0,%%eax,4), %%mm0;"
			"punpcklbw %%mm7, %%mm0;"	// p0 = pIn[0]
			"movq	%%mm0, %%mm2;"
			"pmullw	%%mm5, %%mm2;"		// f0 = multiply(p0, c1)
			"movq	%%mm2, %%mm3;"		// f1 = f0
		
			".p2align 4,,15;"
		"1:"
			"movd	4(%0,%%eax,4), %%mm1;"
			"pxor	%%mm7, %%mm7;"
			"punpcklbw %%mm7, %%mm1;"	// p1 = pIn[x + 1]
			"movq	%%mm0, %%mm4;"
			"pmullw	%%mm6, %%mm4;"		// t = multiply(p0, c2)
			"movq	%%mm1, %%mm0;"
			"pmullw	%%mm5, %%mm0;"		// t0 = multiply(p1, c1)
			"paddw	%%mm2, %%mm4;"
			"paddw  %%mm0, %%mm4;"
			"psrlw	$8, %%mm4;"		// f0 + t + t0
			"movq	%%mm0, %%mm2;"		// f0 = t0

			"movd	8(%0,%%eax,4), %%mm0;"
			"punpcklbw %%mm7, %%mm0;"
			"movq	%%mm1, %%mm7;"
			"pmullw	%%mm6, %%mm7;"		// t = multiply(p1, c2)
			"movq	%%mm0, %%mm1;"
			"pmullw %%mm5, %%mm1;"		// t1 = multiply(p0, c1)
			"paddw	%%mm3, %%mm7;"
			"paddw	%%mm1, %%mm7;"
			"psrlw	$8, %%mm7;"		// f1 + t + t1
			"movq	%%mm1, %%mm3;"		// f1 = t1
			"packuswb %%mm7, %%mm4;"
			"movq	%%mm4, (%1,%%eax,4);"	// pOut[x] = ..  pOut[x+1] = ..
			
			"addl	$2, %%eax;"
			"cmpl	$638, %%eax;"
			"jl	1b;"

			"movd	4(%0,%%eax,4), %%mm1;"
			"pxor	%%mm7, %%mm7;"
			"punpcklbw %%mm7, %%mm1;"	// p1 = pIn[x + 1]
			"movq	%%mm0, %%mm4;"
			"pmullw	%%mm6, %%mm4;"		// t = multiply(p0, c2)
			"movq	%%mm1, %%mm0;"
			"pmullw	%%mm5, %%mm0;"		// t0 = multiply(p1, c1)
			"paddw	%%mm2, %%mm4;"
			"paddw  %%mm0, %%mm4;"
			"psrlw	$8, %%mm4;"		// f0 + t + t0

			"pmullw	%%mm6, %%mm1;"		// t = multiply(p1, c2)
			"paddw	%%mm3, %%mm1;"
			"paddw	%%mm0, %%mm1;"
			"psrlw	$8, %%mm1;"		// f1 + t + t1
			"packuswb %%mm1, %%mm4;"
			"movq	%%mm4, (%1,%%eax,4);"	// pOut[x] = ..  pOut[x+1] = ..
			
			"emms;"

			: // no output
			: "r" (pIn)   // 0
			, "r" (pOut)  // 1
			, "r" (c1)    // 2
			, "r" (c2)    // 3
			: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
			, "eax"
		);
		return;
	}
	#endif

	mult1.setFactor(c1);
	mult3.setFactor(c2);

	Pixel p0 = pIn[0];
	Pixel p1;
	unsigned f0 = mult1.mul32(p0);
	unsigned f1 = f0;

	unsigned x;
	for (x = 0; x < (640 - 2); x += 2) {
		p1 = pIn[x + 1];
		unsigned t0 = mult1.mul32(p1);
		pOut[x] = mult1.conv32(f0 + mult3.mul32(p0) + t0);
		f0 = t0;

		p0 = pIn[x + 2];
		unsigned t1 = mult1.mul32(p0);
		pOut[x + 1] = mult1.conv32(f1 + mult3.mul32(p1) + t1);
		f1 = t1;
	}

	p1 = pIn[x + 1];
	unsigned t0 = mult1.mul32(p1);
	pOut[x] = mult1.conv32(f0 + mult3.mul32(p0) + t0);

	pOut[x + 1] = mult1.conv32(f1 + mult3.mul32(p1) + t0);
}

template <class Pixel>
void SimpleScaler<Pixel>::average(
	const Pixel* src1, const Pixel* src2, Pixel* dst, unsigned alpha)
{
	if (alpha == 255) {
		// no average, just copy top line
		Scaler<Pixel>::copyLine(src1, dst, 640);
		return;
	}
	
	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if ((sizeof(Pixel) == 4) && cpu.hasMMXEXT()) {
		// extended-MMX routine, 32bpp
		asm (
			"movd	%3, %%mm6;"
			"pxor	%%mm7, %%mm7;"
			"xorl	%%eax, %%eax;"
			"pshufw $0, %%mm6, %%mm6;"
			".p2align 4,,15;"
		"1:"
			"movq	(%0,%%eax,4), %%mm0;"
			"pavgb	(%1,%%eax,4), %%mm0;"
			"movq	%%mm0, %%mm4;"
			"punpcklbw %%mm7, %%mm0;"
			"punpckhbw %%mm7, %%mm4;"
			"pmulhuw %%mm6, %%mm0;"
			"pmulhuw %%mm6, %%mm4;"
			"packuswb %%mm4, %%mm0;"

			"movq	8(%0,%%eax,4), %%mm1;"
			"pavgb	8(%1,%%eax,4), %%mm1;"
			"movq	%%mm1, %%mm5;"
			"punpcklbw %%mm7, %%mm1;"
			"punpckhbw %%mm7, %%mm5;"
			"pmulhuw %%mm6, %%mm1;"
			"pmulhuw %%mm6, %%mm5;"
			"packuswb %%mm5, %%mm1;"

			"movq	16(%0,%%eax,4), %%mm2;"
			"pavgb	16(%1,%%eax,4), %%mm2;"
			"movq	%%mm2, %%mm4;"
			"punpcklbw %%mm7, %%mm2;"
			"punpckhbw %%mm7, %%mm4;"
			"pmulhuw %%mm6, %%mm2;"
			"pmulhuw %%mm6, %%mm4;"
			"packuswb %%mm4, %%mm2;"

			"movq	24(%0,%%eax,4), %%mm3;"
			"pavgb	24(%1,%%eax,4), %%mm3;"
			"movq	%%mm3, %%mm5;"
			"punpcklbw %%mm7, %%mm3;"
			"punpckhbw %%mm7, %%mm5;"
			"pmulhuw %%mm6, %%mm3;"
			"pmulhuw %%mm6, %%mm5;"
			"packuswb %%mm5, %%mm3;"

			"movntq %%mm0,   (%2,%%eax,4);"
			"movntq %%mm1,  8(%2,%%eax,4);"
			"movntq %%mm2, 16(%2,%%eax,4);"
			"movntq %%mm3, 24(%2,%%eax,4);"
			
			"addl	$8, %%eax;"
			"cmpl	$640, %%eax;"
			"jl	1b;"
			
			"emms;"
			
			: // no output
			: "r" (src1)  // 0
			, "r" (src2)  // 1
			, "r" (dst)   // 2
			, "r" (alpha << 8) // 3
			: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
			, "eax"
		);
		return;

	} else if ((sizeof(Pixel) == 4) && cpu.hasMMX()) {
		// MMX routine, 32bpp
		asm (
			"movd	%3, %%mm6;"
			"pxor	%%mm7, %%mm7;"
			"punpcklwd %%mm6, %%mm6;"
			"punpckldq %%mm6, %%mm6;"

			"xorl	%%eax, %%eax;"
			".p2align 4,,15;"
		"1:"
			// load
			"movq	(%0,%%eax,4), %%mm0;"
			"movq	%%mm0, %%mm1;"
			"movq	(%1,%%eax,4), %%mm2;"
			"movq	%%mm2, %%mm3;"
			// unpack
			"punpcklbw %%mm7, %%mm0;"
			"punpckhbw %%mm7, %%mm1;"
			"punpcklbw %%mm7, %%mm2;"
			"punpckhbw %%mm7, %%mm3;"
			// average
			"paddw	%%mm2, %%mm0;"
			"paddw	%%mm3, %%mm1;"
			// darken
			"pmulhw	%%mm6, %%mm0;"
			"pmulhw	%%mm6, %%mm1;"
			// pack
			"packuswb %%mm1, %%mm0;"
			// store
			"movq %%mm0, (%2,%%eax,4);"
			
			"addl	$2, %%eax;"
			"cmpl	$640, %%eax;"
			"jl	1b;"
			
			"emms;"
			
			: // no output
			: "r" (src1)  // 0
			, "r" (src2)  // 1
			, "r" (dst)   // 2
			, "r" (alpha << 7) // 3
			: "mm0", "mm1", "mm2", "mm3", "mm6", "mm7"
			, "eax"
		);
		return;
	}

	if ((sizeof(Pixel) == 2) && cpu.hasMMXEXT()) {
		// extended-MMX routine, 16bpp

		darkener.setFactor(alpha);
		word* table = darkener.getTable();
		Pixel mask = ~blender.getMask();
		
		asm (
			"movd	%4, %%mm7;"
			"xorl	%%ecx, %%ecx;"
			"pshufw	$0, %%mm7, %%mm7;"
			
			".p2align 4,,15;"
		"1:"	"movq	 (%0,%%ecx,2), %%mm0;"
			"movq	8(%0,%%ecx,2), %%mm1;"
			"movq	 (%1,%%ecx,2), %%mm2;"
			"movq	8(%1,%%ecx,2), %%mm3;"

			"movq	%%mm7, %%mm4;"
			"movq	%%mm7, %%mm5;"
			"pand	%%mm7, %%mm0;"
			"pand	%%mm7, %%mm1;"
			"pandn  %%mm2, %%mm4;"
			"pandn  %%mm3, %%mm5;"
			"pand	%%mm7, %%mm2;"
			"pand	%%mm7, %%mm3;"
			"pavgw	%%mm2, %%mm0;"
			"pavgw	%%mm3, %%mm1;"
			"paddw	%%mm4, %%mm0;"
			"paddw	%%mm5, %%mm1;"
			
			"pextrw	$0, %%mm0, %%eax;"
			"movw	(%2,%%eax,2), %%ax;"
			"pinsrw	$0, %%eax, %%mm0;"
			"pextrw	$0, %%mm1, %%eax;"
			"movw	(%2,%%eax,2), %%ax;"
			"pinsrw	$0, %%eax, %%mm1;"
			
			"pextrw	$1, %%mm0, %%eax;"
			"movw	(%2,%%eax,2), %%ax;"
			"pinsrw	$1, %%eax, %%mm0;"
			"pextrw	$1, %%mm1, %%eax;"
			"movw	(%2,%%eax,2), %%ax;"
			"pinsrw	$1, %%eax, %%mm1;"

			"pextrw	$2, %%mm0, %%eax;"
			"movw	(%2,%%eax,2), %%ax;"
			"pinsrw	$2, %%eax, %%mm0;"
			"pextrw	$2, %%mm1, %%eax;"
			"movw	(%2,%%eax,2), %%ax;"
			"pinsrw	$2, %%eax, %%mm1;"

			"pextrw	$3, %%mm0, %%eax;"
			"movw	(%2,%%eax,2), %%ax;"
			"pinsrw	$3, %%eax, %%mm0;"
			"pextrw	$3, %%mm1, %%eax;"
			"movw	(%2,%%eax,2), %%ax;"
			"pinsrw	$3, %%eax, %%mm1;"
			
			"movntq	%%mm0,   (%3,%%ecx,2);"
			"movntq	%%mm1,  8(%3,%%ecx,2);"

			"addl	$8, %%ecx;"
			"cmpl	$640, %%ecx;"
			"jl	1b;"
			"emms;"
			: // no output
			: "r" (src1)  // 0
			, "r" (src2)  // 1
			, "r" (table) // 2
			, "r" (dst)   // 3
			, "m" (mask)   // 4
			: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm7"
			, "eax", "ecx"
		);
		return;
	}
	// MMX routine 16bpp is missing, but it's difficult to write because
	// of the missing "pextrw" and "pinsrw" instructions
	
	#endif
		
	// non-MMX routine, both 16bpp and 32bpp
	for (unsigned x = 0; x < 640; ++x) {
		dst[x] = mult1.multiply(
			blender.blend(src1[x], src2[x]), alpha);
	}
}

template <class Pixel>
void SimpleScaler<Pixel>::scale256(SDL_Surface* src, int srcY, int endSrcY,
	                           SDL_Surface* dst, int dstY )
{
	int blur = (blurSetting.getValue() * 256) / 100;
	int scanline = 255 - (scanlineSetting.getValue() * 255) / 100;

	Pixel* srcLine  = Scaler<Pixel>::linePtr(src, srcY++);
	Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, dstY++);
	Pixel* prevDstLine0 = dstLine0;
	blur256(srcLine, dstLine0, blur);

	while (srcY < endSrcY) {
		srcLine = Scaler<Pixel>::linePtr(src, srcY++);
		dstLine0 = Scaler<Pixel>::linePtr(dst, dstY + 1);
		blur256(srcLine, dstLine0, blur);
		
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY);
		average(prevDstLine0, dstLine0, dstLine1, scanline);
		prevDstLine0 = dstLine0;

		dstY += 2;
	}

	// When interlace is enabled, bottom line can fall off the screen.
	if (dstY < dst->h) {
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY);
		average(prevDstLine0, dstLine0, dstLine1, scanline);
	}
}

template <class Pixel>
void SimpleScaler<Pixel>::scale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	int blur = (blurSetting.getValue() * 256) / 100;
	int scanline = 255 - (scanlineSetting.getValue() * 255) / 100;

	Pixel* srcLine  = Scaler<Pixel>::linePtr(src, srcY++);
	Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, dstY++);
	Pixel* prevDstLine0 = dstLine0;
	blur512(srcLine, dstLine0, blur);

	while (srcY < endSrcY) {
		srcLine = Scaler<Pixel>::linePtr(src, srcY++);
		dstLine0 = Scaler<Pixel>::linePtr(dst, dstY + 1);
		blur512(srcLine, dstLine0, blur);
		
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY);
		average(prevDstLine0, dstLine0, dstLine1, scanline);
		prevDstLine0 = dstLine0;

		dstY += 2;
	}

	// When interlace is enabled, bottom line can fall off the screen.
	if (dstY < dst->h) {
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY);
		average(prevDstLine0, dstLine0, dstLine1, scanline);
	}
}


// Force template instantiation.
template class SimpleScaler<word>;
template class SimpleScaler<unsigned int>;

} // namespace openmsx
