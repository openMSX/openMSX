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

template <class Pixel>
SimpleScaler<Pixel>::SimpleScaler()
	: scanlineAlphaSetting(RenderSettings::instance().getScanlineAlpha())
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

// Upper 8 bits do not contain colours; use them as work area.
class HiFreeDarken {
public:
	static inline bool check(Uint32 rMask, Uint32 gMask, Uint32 bMask) {
		return ((rMask | gMask | bMask) & 0xFF000000) == 0;
	}
	static inline Uint32 darken(
		int darkenFactor,
		Uint32 rMask, Uint32 gMask, Uint32 bMask,
		Uint32 colour
		)
	{
		unsigned r = (((colour & rMask) * darkenFactor) >> 8) & rMask;
		unsigned g = (((colour & gMask) * darkenFactor) >> 8) & gMask;
		unsigned b = (((colour & bMask) * darkenFactor) >> 8) & bMask;
		return r | g | b;
	}
};

// Lower 8 bits do not contain colours; use them as work area.
class LoFreeDarken {
public:
	static inline bool check(Uint32 rMask, Uint32 gMask, Uint32 bMask) {
		return ((rMask | gMask | bMask) & 0x000000FF) == 0;
	}
	static inline Uint32 darken(
		int darkenFactor,
		Uint32 rMask, Uint32 gMask, Uint32 bMask,
		Uint32 colour
		)
	{
		unsigned r = (((colour & rMask) >> 8) * darkenFactor) & rMask;
		unsigned g = (((colour & gMask) >> 8) * darkenFactor) & gMask;
		unsigned b = (((colour & bMask) >> 8) * darkenFactor) & bMask;
		return r | g | b;
	}
};

// Uncommon pixel format; fall back to slightly slower routine.
class UniversalDarken {
public:
	static inline bool check(Uint32 rMask, Uint32 gMask, Uint32 bMask) {
		return true;
	}
	static inline Uint32 darken(
		int darkenFactor,
		Uint32 rMask, Uint32 gMask, Uint32 bMask,
		Uint32 colour
	) {
		Uint32 r =
			rMask & 0xFF
			? (((colour & rMask) * darkenFactor) >> 8) & rMask
			: (((colour & rMask) >> 8) * darkenFactor) & rMask;
		Uint32 g =
			gMask & 0xFF
			? (((colour & gMask) * darkenFactor) >> 8) & gMask
			: (((colour & gMask) >> 8) * darkenFactor) & gMask;
		Uint32 b =
			bMask & 0xFF
			? (((colour & bMask) * darkenFactor) >> 8) & bMask
			: (((colour & bMask) >> 8) * darkenFactor) & bMask;
		return r | g | b;
	}
	static inline Uint32 darken(
		int darkenFactor, const SDL_PixelFormat* format, Uint32 colour
	) {
		return darken(
			darkenFactor, 
			format->Rmask, format->Gmask, format->Bmask,
			colour
			);
	}
};

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
		const HostCPU& cpu = HostCPU::getInstance();
		int darkenFactor = 256 - scanlineAlpha;
		Pixel scanlineColour = UniversalDarken::darken(
			darkenFactor, dst->format, colour );
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
		const Pixel* srcLine = Scaler<Pixel>::linePtr(src, srcY++);
		Pixel* dstUpper = Scaler<Pixel>::linePtr(dst, dstY++);
		Pixel* dstLower =
			dstY == dst->h ? dstUpper : Scaler<Pixel>::linePtr(dst, dstY++);
		/*if (ASM_X86 && cpu.hasMMXEXT() && sizeof(Pixel) == 2) {
			 TODO: Implement.
		} else*/ if (ASM_X86 && cpu.hasMMXEXT() && sizeof(Pixel) == 4) {
			asm (
				// Upper line: scale, no scanline.
				// Note: Two separate loops is faster, probably because of
				//       linear memory access.
				"xorl	%%eax, %%eax;"
			"0:"
				// Load.
				"movq	  (%3,%%eax,4), %%mm0;"
				"movq	 8(%3,%%eax,4), %%mm2;"
				"movq	16(%3,%%eax,4), %%mm4;"
				"movq	24(%3,%%eax,4), %%mm6;"
				"movq	%%mm0, %%mm1;"
				"movq	%%mm2, %%mm3;"
				"movq	%%mm4, %%mm5;"
				"movq	%%mm6, %%mm7;"
				// Scale.
				"punpckldq %%mm0, %%mm0;"
				"punpckhdq %%mm1, %%mm1;"
				"punpckldq %%mm2, %%mm2;"
				"punpckhdq %%mm3, %%mm3;"
				"punpckldq %%mm4, %%mm4;"
				"punpckhdq %%mm5, %%mm5;"
				"punpckldq %%mm6, %%mm6;"
				"punpckhdq %%mm7, %%mm7;"
				// Store.
				"movntq	%%mm0,   (%1,%%eax,8);"
				"movntq	%%mm1,  8(%1,%%eax,8);"
				"movntq	%%mm2, 16(%1,%%eax,8);"
				"movntq	%%mm3, 24(%1,%%eax,8);"
				"movntq	%%mm4, 32(%1,%%eax,8);"
				"movntq	%%mm5, 40(%1,%%eax,8);"
				"movntq	%%mm6, 48(%1,%%eax,8);"
				"movntq	%%mm7, 56(%1,%%eax,8);"
				// Increment.
				"addl	$8, %%eax;"
				"cmpl	%4, %%eax;"
				"jl	0b;"
		
				// Lower line: scale and scanline.
				// Precalc: mm6 = darkenFactor, mm7 = 0.
				"movd	%0, %%mm6;"
				"pxor	%%mm7, %%mm7;"
				"punpcklwd	%%mm6, %%mm6;"
				"punpckldq	%%mm6, %%mm6;"
		
				"xorl	%%eax, %%eax;"
			"1:"
				// Load.
				"movq	 (%3,%%eax,4), %%mm0;"
				"movq	8(%3,%%eax,4), %%mm2;"
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
				"movntq	%%mm0,   (%2,%%eax,8);"
				"movntq	%%mm1,  8(%2,%%eax,8);"
				"packuswb %%mm2, %%mm2;"
				"packuswb %%mm3, %%mm3;"
				// Store.
				"movntq	%%mm2, 16(%2,%%eax,8);"
				"movntq	%%mm3, 24(%2,%%eax,8);"
				// Increment.
				"addl	$4, %%eax;"
				"cmpl	%4, %%eax;"
				"jl	1b;"
		
				: // no output
				: "mr" (darkenFactor) // 0
				, "r" (dstUpper) // 1
				, "r" (dstLower) // 2
				, "r" (srcLine) // 3
				, "r" (width) // 4
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
			unsigned rMask = dst->format->Rmask;
			unsigned gMask = dst->format->Gmask;
			unsigned bMask = dst->format->Bmask;
			for (int x = 0; x < width; x++) {
				Pixel p = srcLine[x];
				dstUpper[x * 2] = dstUpper[x * 2 + 1] = p;
				if (sizeof(Pixel) == 2) {
					dstLower[x * 2] = dstLower[x * 2 + 1] =
						HiFreeDarken::darken(
							darkenFactor, rMask, gMask, bMask, p );
				} else {
					// TODO: On my machine, HiFreeDarken is marginally
					//       faster, but is it worth the effort?
					dstLower[x * 2] = dstLower[x * 2 + 1] =
						UniversalDarken::darken(
							darkenFactor, rMask, gMask, bMask, p );
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

	SDL_PixelFormat* format = dst->format;
	Uint32 rMask = format->Rmask;
	Uint32 gMask = format->Gmask;
	Uint32 bMask = format->Bmask;
	int darkenFactor = 256 - scanlineAlpha;
	const unsigned width = dst->w;
	
	const HostCPU& cpu = HostCPU::getInstance();
	while (srcY < endSrcY) {
		Scaler<Pixel>::copyLine(src, srcY, dst, dstY++);
		if (dstY == dst->h) break;
		const Pixel* srcLine = Scaler<Pixel>::linePtr(src, srcY);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, dstY++);
		/*if (ASM_X86 && cpu.hasMMXEXT() && sizeof(Pixel) == 2) {
			 TODO: Implement.
		} else*/ if (ASM_X86 && cpu.hasMMXEXT() && sizeof(Pixel) == 4) {
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
			for (unsigned x = 0; x < width; x++) {
				dstLine[x] = UniversalDarken::darken(
					darkenFactor, rMask, gMask, bMask, srcLine[x] );
			}
		}
		srcY++;
	}
}

} // namespace openmsx

