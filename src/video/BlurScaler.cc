// $Id$

#include "BlurScaler.hh"
#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "HostCPU.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

// Force template instantiation.
template class BlurScaler<word>;
template class BlurScaler<unsigned int>;

template <class Pixel>
BlurScaler<Pixel>::BlurScaler(SDL_PixelFormat* format_)
	: scanlineSetting(*RenderSettings::instance().getScanlineAlpha())
	, blurSetting(*RenderSettings::instance().getHorizontalBlur())
	, blender(Blender<Pixel>::createFromFormat(format_))
	, format(format_)
{
}

template <class Pixel>
BlurScaler<Pixel>::~BlurScaler()
{
}

template <>
word BlurScaler<word>::multiply(word pixel, unsigned factor)
{
	return ((((pixel & format->Rmask) * factor) >> 8) & format->Rmask) |
	       ((((pixel & format->Gmask) * factor) >> 8) & format->Gmask) |
	       ((((pixel & format->Bmask) * factor) >> 8) & format->Bmask);
}

template <>
unsigned BlurScaler<unsigned>::multiply(unsigned pixel, unsigned factor)
{
	return ((((pixel & 0x00FF00FF) * factor) & 0xFF00FF00) |
                (((pixel & 0x0000FF00) * factor) & 0x00FF0000)) >> 8;
}


// TODO This code is copied from SimpleScaler. Share this code somehow
template <class Pixel>
void BlurScaler<Pixel>::scaleBlank(Pixel colour, SDL_Surface* dst,
                                   int dstY, int endDstY)
{
	if (colour == 0) {
		// No need to draw scanlines if border is black.
		// This is a special case that occurs very often.
		Scaler<Pixel>::scaleBlank(colour, dst, dstY, endDstY);
	} else {
		const HostCPU& cpu = HostCPU::getInstance();
		int scanline = 256 - (scanlineSetting.getValue() * 256) / 100;
		Pixel scanlineColour = multiply(colour, scanline);
		if (ASM_X86 && cpu.hasMMXEXT()) {
			const unsigned col32 =
				  sizeof(Pixel) == 2
				? (((unsigned)colour) << 16) | colour
				: colour;
			const unsigned scan32 =
				  sizeof(Pixel) == 2
				? (((unsigned)scanlineColour) << 16) | scanlineColour
				: scanlineColour;
			while (dstY < endDstY) {
				Pixel* dstUpper = Scaler<Pixel>::linePtr(dst, dstY++);
				Pixel* dstLower = Scaler<Pixel>::linePtr(dst, dstY++);
				asm (
					// Precalc normal colour.
					"movd	%2, %%mm0;"
					"punpckldq	%%mm0, %%mm0;"
					"movq	%%mm0, %%mm1;"
					"movq	%%mm0, %%mm2;"
					"movq	%%mm1, %%mm3;"

					"xorl	%%eax, %%eax;"
				"0:"
					// Store.
					"movntq	%%mm0,   (%0,%%eax);"
					"movntq	%%mm1,  8(%0,%%eax);"
					"movntq	%%mm2, 16(%0,%%eax);"
					"movntq	%%mm3, 24(%0,%%eax);"
					// Increment.
					"addl	$32, %%eax;"
					"cmpl	%4, %%eax;"
					"jl	0b;"

					// Precalc darkened colour.
					"movd	%3, %%mm4;"
					"punpckldq	%%mm4, %%mm4;"
					"movq	%%mm4, %%mm5;"
					"movq	%%mm4, %%mm6;"
					"movq	%%mm5, %%mm7;"

					"xorl	%%eax, %%eax;"
				"1:"
					// Store.
					"movntq	%%mm4,   (%1,%%eax);"
					"movntq	%%mm5,  8(%1,%%eax);"
					"movntq	%%mm6, 16(%1,%%eax);"
					"movntq	%%mm7, 24(%1,%%eax);"
					// Increment.
					"addl	$32, %%eax;"
					"cmpl	%4, %%eax;"
					"jl	1b;"

					: // no output
					: "r" (dstUpper) // 0
					, "r" (dstLower) // 1
					, "rm" (col32) // 2
					, "rm" (scan32) // 3
					, "r" (dst->w * sizeof(Pixel)) // 4: bytes per line
					: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
					, "eax"
					);
			}
			asm volatile ("emms");
		} else {
			// Note: SDL_FillRect is generally not allowed on locked surfaces.
			//       However, we're using a software surface, which doesn't
			//       have locking.
			// TODO: But it would be more generic to just write bytes.
			assert(!SDL_MUSTLOCK(dst));
	
			SDL_Rect rect;
			rect.x = 0;
			rect.w = dst->w;
			rect.h = 1;
			for (int y = dstY; y < endDstY; y += 2) {
				rect.y = y;
				// Note: return code ignored.
				SDL_FillRect(dst, &rect, colour);
				if (y + 1 == endDstY) break;
				rect.y = y + 1;
				// Note: return code ignored.
				SDL_FillRect(dst, &rect, scanlineColour);
			}
		}
	}
}

template <class Pixel>
void BlurScaler<Pixel>::blur256(const Pixel* pIn, Pixel* pOut, unsigned alpha)
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

	assert(alpha <= 256);
	unsigned c1 = alpha / 4;
	unsigned c2 = 256 - c1;

	const HostCPU& cpu = HostCPU::getInstance();
	if (ASM_X86 && cpu.hasMMXEXT() && sizeof(Pixel) == 4) {
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

			: // no output
			: "r" (pIn)   // 0
			, "r" (pOut)  // 1
			, "r" (c1)    // 2
			, "r" (c2)    // 3
			: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
			, "eax"
			);

	} else {
		// non-MMX routine, both 16bpp and 32bpp
		Pixel p0 = pIn[0];
		Pixel p1;
		unsigned f0 = multiply(p0, c1);
		unsigned f1 = f0;
		unsigned tmp;

		unsigned x;
		for (x = 0; x < (320 - 2); x += 2) {
			tmp = multiply(p0, c2);
			pOut[2 * x + 0] = f1 + tmp;

			p1 = pIn[x + 1];
			f1 = multiply(p1, c1);
			pOut[2 * x + 1] = f1 + tmp;

			tmp = multiply(p1, c2);
			pOut[2 * x + 2] = f0 + tmp;

			p0 = pIn[x + 2];
			f0 = multiply(p0, c1);
			pOut[2 * x + 3] = f0 + tmp;
		}

		tmp = multiply(p0, c2);
		pOut[2 * x + 0] = f1 + tmp;

		p1 = pIn[x + 1];
		f1 = multiply(p1, c1);
		pOut[2 * x + 1] = f1 + tmp;

		tmp = multiply(p1, c2);
		pOut[2 * x + 2] = f0 + tmp;

		pOut[2 * x + 3] = p1;
	}
}

template <class Pixel>
void BlurScaler<Pixel>::blur512(const Pixel* pIn, Pixel* pOut, unsigned alpha)
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

	unsigned c1 = alpha / 4;
	unsigned c2 = 256 - alpha / 2;

	const HostCPU& cpu = HostCPU::getInstance();
	if (false && cpu.hasMMXEXT() && sizeof(Pixel) == 4) {
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
			
			: // no output
			: "r" (pIn)   // 0
			, "r" (pOut)  // 1
			, "r" (c1)    // 2
			, "r" (c2)    // 3
			: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
			, "eax"
			);

	} else {

		Pixel p0 = pIn[0];
		Pixel p1;
		unsigned f0 = multiply(p0, c1);
		unsigned f1 = f0;

		unsigned x;
		for (x = 0; x < (640 - 2); x += 2) {
			p1 = pIn[x + 1];
			unsigned t0 = multiply(p1, c1);
			pOut[x] = f0 + multiply(p0, c2) + t0;
			f0 = t0;

			p0 = pIn[x + 2];
			unsigned t1 = multiply(p0, c1);
			pOut[x + 1] = f1 + multiply(p1, c2) + t1;
			f1 = t1;
		}

		p1 = pIn[x + 1];
		unsigned t0 = multiply(p1, c1);
		pOut[x] = f0 + multiply(p0, c2) + t0;

		pOut[x + 1] = f1 + multiply(p1, c2) + t0;
	}
}

template <class Pixel>
void BlurScaler<Pixel>::average(
	const Pixel* src1, const Pixel* src2, Pixel* dst, unsigned alpha)
{
	const HostCPU& cpu = HostCPU::getInstance();
	if (ASM_X86 && cpu.hasMMXEXT() && sizeof(Pixel) == 4) {
		// MMX routine, 32bpp
		asm (
			"movd	%3, %%mm6;"
			"pxor	%%mm7, %%mm7;"
			"psrlq	$1, %%mm6;"
			"punpcklwd %%mm6, %%mm6;"
			"punpckldq %%mm6, %%mm6;"

			"xorl	%%eax, %%eax;"
		"1:"
			// load
			"movq	(%0,%%eax,4), %%mm0;"
			"movq	%%mm0, %%mm2;"
			"movq	(%1,%%eax,4), %%mm1;"
			"movq	%%mm1, %%mm3;"
			// unpack
			"punpcklbw %%mm7, %%mm0;"
			"punpcklbw %%mm7, %%mm1;"
			"punpckhbw %%mm7, %%mm2;"
			"punpckhbw %%mm7, %%mm3;"
			// average
			"paddw	%%mm1, %%mm0;"
			"paddw	%%mm3, %%mm2;"
			// darken
			"pmullw	%%mm6, %%mm0;"
			"pmullw	%%mm6, %%mm2;"
			"psrlw	$8, %%mm0;"
			"psrlw	$8, %%mm2;"
			// pack
			"packuswb %%mm2, %%mm0;"
			// store
			"movntq %%mm0, (%2,%%eax,4);"
			
			"addl	$2, %%eax;"
			"cmpl	$640, %%eax;"
			"jl	1b;"
			
			: // no output
			: "r" (src1)  // 0
			, "r" (src2)  // 1
			, "r" (dst)   // 2
			, "r" (alpha) // 3
			: "mm0", "mm1", "mm2", "mm3", "mm6", "mm7"
			, "eax"
			);
		
	} else { 
		// non-MMX routine, both 16bpp and 32bpp
		for (unsigned x = 0; x < 640; ++x) {
			dst[x] = multiply(blender.blend(src1[x], src2[x]), alpha);
		}
	}
}

template <class Pixel>
void BlurScaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	int blur = (blurSetting.getValue() * 256) / 100;
	int scanline = 256 - (scanlineSetting.getValue() * 256) / 100;

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

	Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY);
	average(prevDstLine0, dstLine0, dstLine1, scanline);
}

template <class Pixel>
void BlurScaler<Pixel>::scale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	int blur = (blurSetting.getValue() * 256) / 100;
	int scanline = 256 - (scanlineSetting.getValue() * 256) / 100;

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

	Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY);
	average(prevDstLine0, dstLine0, dstLine1, scanline);
}

} // namespace openmsx
