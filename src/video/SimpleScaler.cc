// $Id$

#include "SimpleScaler.hh"
#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "HostCPU.hh"
#include "openmsx.hh"
#include <cassert>


namespace openmsx {

// Force template instantiation.
template class SimpleScaler<word>;
template class SimpleScaler<unsigned int>;



Darkener<word>::Darkener(SDL_PixelFormat* format_)
	: format(format_)
{
	factor = 0;
	memset(tab, 0, sizeof(tab));
}

inline word Darkener<word>::darken(word p, unsigned f)
{
	return ((((p & format->Rmask) * f) >> 8) & format->Rmask) |
	       ((((p & format->Gmask) * f) >> 8) & format->Gmask) |
	       ((((p & format->Bmask) * f) >> 8) & format->Bmask);
}

void Darkener<word>::setFactor(unsigned f)
{
	if (f == factor) {
		return;
	}
	factor = f;

	for (unsigned p = 0; p < 0x10000; ++p) {
		unsigned d = darken(p, factor);
		tab[p] = d | (d << 16);
	}
}

inline word Darkener<word>::darken(word p)
{
	return tab[p];
}

inline unsigned Darkener<word>::darkenDouble(word p)
{
	return tab[p];
}

inline unsigned* Darkener<word>::getTable()
{
	return tab;
}


Darkener<unsigned>::Darkener(SDL_PixelFormat* /*format*/)
{
}

inline unsigned Darkener<unsigned>::darken(unsigned p, unsigned f)
{
	return ((((p & 0xFF00FF) * f) & 0xFF00FF00) |
	        (((p & 0x00FF00) * f) & 0x00FF0000)) >> 8;
}

inline void Darkener<unsigned>::setFactor(unsigned f)
{
	factor = f;
}

inline unsigned Darkener<unsigned>::darken(unsigned p)
{
	return darken(p, factor);
}

inline unsigned Darkener<unsigned>::darkenDouble(unsigned /*p*/)
{
	assert(false);
	return 0;
}

unsigned* Darkener<unsigned>::getTable()
{
	assert(false);
	return NULL;
}



template <class Pixel>
SimpleScaler<Pixel>::SimpleScaler(SDL_PixelFormat* format)
	: scanlineAlphaSetting(RenderSettings::instance().getScanlineAlpha())
	, darkener(format)
{
	update(scanlineAlphaSetting);
	scanlineAlphaSetting->addListener(this);
}

template <class Pixel>
SimpleScaler<Pixel>::~SimpleScaler()
{
	scanlineAlphaSetting->removeListener(this);
}

template <class Pixel>
void SimpleScaler<Pixel>::update(const SettingLeafNode* setting)
{
	assert(setting == scanlineAlphaSetting);
	scanlineAlpha = (scanlineAlphaSetting->getValue() * 255) / 100;
}


template <class Pixel>
void SimpleScaler<Pixel>::scaleBlank(
	Pixel colour,
	SDL_Surface* dst, int dstY, int endDstY
) {
	if (colour == 0) {
		// No need to draw scanlines if border is black.
		// This is a special case that occurs very often.
		Scaler<Pixel>::scaleBlank(colour, dst, dstY, endDstY);
	} else {
		// extended-MMX routine (both 16bpp and 32bpp)
		const HostCPU& cpu = HostCPU::getInstance();
		int darkenFactor = 256 - scanlineAlpha;
		Pixel scanlineColour = darkener.darken(colour, darkenFactor);
		if (ASM_X86 && cpu.hasMMXEXT()) {
			const unsigned col32 = sizeof(Pixel) == 2
				? (((unsigned)colour) << 16) | colour
				: colour;
			const unsigned scan32 = sizeof(Pixel) == 2
				? (((unsigned)scanlineColour) << 16) | scanlineColour
				: scanlineColour;
			while (dstY < endDstY) {
				Pixel* dstUpper = Scaler<Pixel>::linePtr(dst, dstY++);
				Pixel* dstLower = Scaler<Pixel>::linePtr(dst, dstY++);
				asm (
					// Precalc normal colour.
					"movd	%2, %%mm0;"
					"punpckldq	%%mm0, %%mm0;"

					"xorl	%%eax, %%eax;"
				"0:"
					// Store.
					"movntq	%%mm0,   (%0,%%eax);"
					"movntq	%%mm0,  8(%0,%%eax);"
					"movntq	%%mm0, 16(%0,%%eax);"
					"movntq	%%mm0, 24(%0,%%eax);"
					"movntq	%%mm0, 32(%0,%%eax);"
					"movntq	%%mm0, 40(%0,%%eax);"
					"movntq	%%mm0, 48(%0,%%eax);"
					"movntq	%%mm0, 56(%0,%%eax);"
					// Increment.
					"addl	$64, %%eax;"
					"cmpl	%4, %%eax;"
					"jl	0b;"

					// Precalc darkened colour.
					"movd	%3, %%mm1;"
					"punpckldq	%%mm1, %%mm1;"

					"xorl	%%eax, %%eax;"
				"1:"
					// Store.
					"movntq	%%mm1,   (%1,%%eax);"
					"movntq	%%mm1,  8(%1,%%eax);"
					"movntq	%%mm1, 16(%1,%%eax);"
					"movntq	%%mm1, 24(%1,%%eax);"
					"movntq	%%mm1, 32(%1,%%eax);"
					"movntq	%%mm1, 40(%1,%%eax);"
					"movntq	%%mm1, 48(%1,%%eax);"
					"movntq	%%mm1, 56(%1,%%eax);"
					// Increment.
					"addl	$64, %%eax;"
					"cmpl	%4, %%eax;"
					"jl	1b;"

					: // no output
					: "r" (dstUpper) // 0
					, "r" (dstLower) // 1
					, "rm" (col32) // 2
					, "rm" (scan32) // 3
					, "r" (dst->w * sizeof(Pixel)) // 4: bytes per line
					: "mm0", "mm1"
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

static const bool ASM_NOSUCHMACHINE = false;

template <class Pixel>
void SimpleScaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	if (scanlineAlpha == 0) {
		Scaler<Pixel>::scale256(src, srcY, endSrcY, dst, dstY);
		return;
	}

	int darkenFactor = 256 - scanlineAlpha;
	const int width = dst->w / 2;
	const HostCPU& cpu = HostCPU::getInstance();
	while (srcY < endSrcY) {
		if (ASM_X86 && (sizeof(Pixel) == 2) && cpu.hasMMXEXT()) {
			// extended-MMX routine 16bpp
			Scaler<Pixel>::scaleLine(src, srcY, dst, dstY++);
			if (dstY == dst->h) break;
			
			darkener.setFactor(darkenFactor);
			unsigned* darkenTab = darkener.getTable();
		
			const Pixel* srcLine = Scaler<Pixel>::linePtr(src, srcY++);
			Pixel* dstLine = Scaler<Pixel>::linePtr(dst, dstY++);

			asm (
				"xorl	%%ecx, %%ecx;"
			"0:"
				// src pixels 0-1
				"movzwl	  (%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm4;"	// already darkened/doubled
				
				"movzwl	 2(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpckldq %%mm0, %%mm4;"
				
				// src pixels 2-3
				"movzwl	 4(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm5;"
				
				"movzwl	 6(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpckldq %%mm0, %%mm5;"
				
				// src pixels 4-5
				"movzwl	 8(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm6;"
				
				"movzwl	10(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpckldq %%mm0, %%mm6;"
				
				// src pixels 6-7
				"movzwl	12(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm7;"
				
				"movzwl	14(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpckldq %%mm0, %%mm7;"
				
				// store
				"movntq	%%mm4,   (%2,%%ecx,4);"
				"movntq	%%mm5,  8(%2,%%ecx,4);"
				"movntq	%%mm6, 16(%2,%%ecx,4);"
				"movntq	%%mm7, 24(%2,%%ecx,4);"
				
				// Incrementr
				"addl	$8, %%ecx;"
				"cmpl	%3, %%ecx;"
				"jl	0b;"
				
				: // no output
				: "r" (darkenTab) // 0
				, "r" (srcLine) // 1
				, "r" (dstLine) // 2
				, "r" (width) // 3
				: "mm0", "mm4", "mm5", "mm6", "mm7"
				, "eax", "ecx"
			);
			
		} else if (ASM_X86 && (sizeof(Pixel) == 4) && cpu.hasMMXEXT()) {
			// extended-MMX routine 32bpp
			Scaler<Pixel>::scaleLine(src, srcY, dst, dstY++);
			if (dstY == dst->h) break;
			
			const Pixel* srcLine = Scaler<Pixel>::linePtr(src, srcY++);
			Pixel* dstLine = Scaler<Pixel>::linePtr(dst, dstY++);

			asm (
				// Scale and scanline.
				// Precalc: mm6 = darkenFactor, mm7 = 0.
				"movd	%0, %%mm6;"
				"pxor	%%mm7, %%mm7;"
				"punpcklwd	%%mm6, %%mm6;"
				"punpckldq	%%mm6, %%mm6;"
		
				"xorl	%%eax, %%eax;"
			"1:"
				// Load.
				"movq	 (%2,%%eax,4), %%mm0;"
				"movq	8(%2,%%eax,4), %%mm2;"
				"movq	%%mm0, %%mm1;"
				"movq	%%mm2, %%mm3;"
				// Darken and scale.
				"punpcklbw %%mm7, %%mm0;"
				"punpckhbw %%mm7, %%mm1;"
				"pmullw	%%mm6, %%mm0;"
				"pmullw	%%mm6, %%mm1;"
				"punpcklbw %%mm7, %%mm2;"
				"punpckhbw %%mm7, %%mm3;"
				"psrlw	$8, %%mm0;"
				"psrlw	$8, %%mm1;"
				"pmullw	%%mm6, %%mm2;"
				"pmullw	%%mm6, %%mm3;"
				"packuswb %%mm0, %%mm0;"
				"packuswb %%mm1, %%mm1;"
				"psrlw	$8, %%mm2;"
				"psrlw	$8, %%mm3;"
				"movntq	%%mm0,   (%1,%%eax,8);"
				"movntq	%%mm1,  8(%1,%%eax,8);"
				"packuswb %%mm2, %%mm2;"
				"packuswb %%mm3, %%mm3;"
				// Store.
				"movntq	%%mm2, 16(%1,%%eax,8);"
				"movntq	%%mm3, 24(%1,%%eax,8);"
				// Increment.
				"addl	$4, %%eax;"
				"cmpl	%3, %%eax;"
				"jl	1b;"
		
				: // no output
				: "mr" (darkenFactor) // 0
				, "r" (dstLine) // 1
				, "r" (srcLine) // 2
				, "r" (width) // 3
				: "mm0", "mm1", "mm2", "mm3"
				, "mm4", "mm5", "mm6", "mm7"
				, "eax"
				);
		// TODO: Test code, remove once we're satisfied all supported
		//       compilers skip code generation here.
		} else if (ASM_NOSUCHMACHINE) {
			asm ("nosuchinstruction");
		// End of test code.
		} else {
			// both destination lines in one loop is faster
			const Pixel* srcLine = Scaler<Pixel>::linePtr(src, srcY++);
			Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY++);
			Pixel* dstLine2 = (dstY != dst->h)
			                ? Scaler<Pixel>::linePtr(dst, dstY++)
			                : dstLine1;

			if (sizeof(Pixel) == 2) {
				darkener.setFactor(darkenFactor);
				unsigned* dst1 = reinterpret_cast<unsigned*>(dstLine1);
				unsigned* dst2 = reinterpret_cast<unsigned*>(dstLine2);
				for (int x = 0; x < width; x++) {
					Pixel p = srcLine[x];
					dst1[x] = ((unsigned)p) << 16 | p;
					dst2[x] = darkener.darkenDouble(p);
				}
			} else {
				for (int x = 0; x < width; x++) {
					Pixel p = srcLine[x];
					dstLine1[x * 2] = dstLine1[x * 2 + 1] = p;
					dstLine2[x * 2] = dstLine2[x * 2 + 1] =
						darkener.darken(p, darkenFactor);
				}
			}
		}
	}
	if (ASM_X86 && cpu.hasMMXEXT()) {
		asm volatile ("emms");
	}
}

template <class Pixel>
void SimpleScaler<Pixel>::scale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	if (scanlineAlpha == 0) {
		Scaler<Pixel>::scale512(src, srcY, endSrcY, dst, dstY);
		return;
	}

	int darkenFactor = 256 - scanlineAlpha;
	const unsigned width = dst->w;
	
	const HostCPU& cpu = HostCPU::getInstance();
	while (srcY < endSrcY) {
		Scaler<Pixel>::copyLine(src, srcY, dst, dstY++);
		if (dstY == dst->h) break;
		const Pixel* srcLine = Scaler<Pixel>::linePtr(src, srcY++);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, dstY++);
		if (ASM_X86 && (sizeof(Pixel) == 2) && cpu.hasMMXEXT()) {
			// extended-MMX routine 16bpp
			darkener.setFactor(darkenFactor);
			unsigned* darkenTab = darkener.getTable();
		
			asm (
				"xorl	%%ecx, %%ecx;"
			"0:"
				// src pixels 0-3
				"movzwl	  (%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm4;" // ignore upper 16-bits
				
				"movzwl	 2(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpcklwd %%mm0, %%mm4;"
				
				"movzwl	 4(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm1;"

				"movzwl	 6(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpcklwd %%mm0, %%mm1;"
				"punpckldq %%mm1, %%mm4;"

				// src pixels 4-7
				"movzwl	 8(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm5;"
				
				"movzwl	10(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpcklwd %%mm0, %%mm5;"
				
				"movzwl	12(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm1;"

				"movzwl	14(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpcklwd %%mm0, %%mm1;"
				"punpckldq %%mm1, %%mm5;"

				// src pixels 8-11
				"movzwl	 16(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm6;"
				
				"movzwl	18(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpcklwd %%mm0, %%mm6;"
				
				"movzwl	20(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm1;"

				"movzwl	22(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpcklwd %%mm0, %%mm1;"
				"punpckldq %%mm1, %%mm6;"
				
				// src pixels 12-15
				"movzwl	 24(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm7;"
				
				"movzwl	26(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpcklwd %%mm0, %%mm7;"
				
				"movzwl	28(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm1;"

				"movzwl	30(%1,%%ecx,2), %%eax;"
				"movd	(%0,%%eax,4), %%mm0;"
				"punpcklwd %%mm0, %%mm1;"
				"punpckldq %%mm1, %%mm7;"

				// store
				"movntq	%%mm4,   (%2,%%ecx,2);"
				"movntq	%%mm5,  8(%2,%%ecx,2);"
				"movntq	%%mm6, 16(%2,%%ecx,2);"
				"movntq	%%mm7, 24(%2,%%ecx,2);"
				
				// Increment.
				"addl	$16, %%ecx;"
				"cmpl	%3, %%ecx;"
				"jl	0b;"
				
				: // no output
				: "r" (darkenTab) // 0
				, "r" (srcLine) // 1
				, "r" (dstLine) // 2
				, "r" (width) // 3
				: "mm0", "mm1", "mm4", "mm5", "mm6", "mm7"
				, "eax", "ecx"
			);

		} else if (ASM_X86 && (sizeof(Pixel) == 4) && cpu.hasMMXEXT()) {
			// extended-MMX routine 32bpp
			asm (
				// Precalc: mm6 = darkenFactor, mm7 = 0.
				"movd	%0, %%mm6;"
				"pxor	%%mm7, %%mm7;"
				"punpcklwd	%%mm6, %%mm6;"
				"punpckldq	%%mm6, %%mm6;"
				
				// Copy with darken.
				"xorl	%%eax, %%eax;"
			"0:"
				// Load.
				"movq	 (%2,%%eax,4), %%mm0;"
				"movq	8(%2,%%eax,4), %%mm2;"
				"movq	%%mm0, %%mm1;"
				"movq	%%mm2, %%mm3;"
				// Darken and scale.
				"punpcklbw %%mm7, %%mm0;"
				"punpckhbw %%mm7, %%mm1;"
				"pmullw	%%mm6, %%mm0;"
				"pmullw	%%mm6, %%mm1;"
				"punpcklbw %%mm7, %%mm2;"
				"punpckhbw %%mm7, %%mm3;"
				"psrlw	$8, %%mm0;"
				"psrlw	$8, %%mm1;"
				"pmullw	%%mm6, %%mm2;"
				"pmullw	%%mm6, %%mm3;"
				"packuswb %%mm1, %%mm0;"
				"psrlw	$8, %%mm2;"
				"psrlw	$8, %%mm3;"
				"packuswb %%mm3, %%mm2;"
				// Store.
				"movntq	%%mm0,  (%1,%%eax,4);"
				"movntq	%%mm2, 8(%1,%%eax,4);"
				// Increment.
				"addl	$4, %%eax;"
				"cmpl	%3, %%eax;"
				"jl	0b;"
				
				: // no output
				: "mr" (darkenFactor) // 0
				, "r" (dstLine) // 1
				, "r" (srcLine) // 2
				, "r" (width) // 3
				: "mm0", "mm1", "mm2", "mm3", "mm6", "mm7"
				, "eax"
				);
		// TODO: Test code, remove once we're satisfied all supported
		//       compilers skip code generation here.
		} else if (ASM_NOSUCHMACHINE) {
			asm ("nosuchinstruction");
		// End of test code.
		} else {
			darkener.setFactor(darkenFactor);

			for (unsigned x = 0; x < width; x++) {
				dstLine[x] = darkener.darken(srcLine[x]);
			}
		}
	}
	if (ASM_X86 && cpu.hasMMXEXT()) {
		asm volatile ("emms");
	}
}

} // namespace openmsx
