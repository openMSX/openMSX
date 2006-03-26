// $Id$

#include "Scanline.hh"
#include "HostCPU.hh"
#include <cassert>

namespace openmsx {

// class Multiply<word>

Multiply<word>::Multiply(const SDL_PixelFormat* format_)
	: format(format_)
{
	factor = 0;
	memset(tab, 0, sizeof(tab));
}

void Multiply<word>::setFactor(unsigned f)
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

inline word Multiply<word>::multiply(word p, unsigned f) const
{
	unsigned r = (((p & format->Rmask) * f) >> 8) & format->Rmask;
	unsigned g = (((p & format->Gmask) * f) >> 8) & format->Gmask;
	unsigned b = (((p & format->Bmask) * f) >> 8) & format->Bmask;
	return r | g | b;
}

inline word Multiply<word>::multiply(word p) const
{
	return tab[p];
}

inline const word* Multiply<word>::getTable() const
{
	return tab;
}


// class Multiply<unsigned>

Multiply<unsigned>::Multiply(const SDL_PixelFormat* /*format*/)
{
}

void Multiply<unsigned>::setFactor(unsigned f)
{
	factor = f;
}

inline unsigned Multiply<unsigned>::multiply(unsigned p, unsigned f) const
{
	return ((((p & 0xFF00FF) * f) & 0xFF00FF00) |
	        (((p & 0x00FF00) * f) & 0x00FF0000)) >> 8;
}

inline unsigned Multiply<unsigned>::multiply(unsigned p) const
{
	return multiply(p, factor);
}

const unsigned* Multiply<unsigned>::getTable() const
{
	assert(false);
	return NULL;
}


// class Scanline

template <class Pixel>
Scanline<Pixel>::Scanline(const PixelOperations<Pixel>& pixelOps)
	: darkener(pixelOps.format)
	, pixelOps(pixelOps.format)
{
}

template <class Pixel>
void Scanline<Pixel>::draw(const Pixel* src1, const Pixel* src2,
		           Pixel* dst, unsigned factor, unsigned width)
{
	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if ((sizeof(Pixel) == 4) && cpu.hasMMXEXT()) {
		// extended-MMX routine, 32bpp
		assert(((4 * width) % 32) == 0);
		asm (
			"movd	%3, %%mm6;"
			"pxor	%%mm7, %%mm7;"
			"pshufw $0, %%mm6, %%mm6;"
			".p2align 4,,15;"
		"1:"
			"movq	(%0,%4), %%mm0;"
			"pavgb	(%1,%4), %%mm0;"
			"movq	%%mm0, %%mm4;"
			"punpcklbw %%mm7, %%mm0;"
			"punpckhbw %%mm7, %%mm4;"
			"pmulhuw %%mm6, %%mm0;"
			"pmulhuw %%mm6, %%mm4;"
			"packuswb %%mm4, %%mm0;"

			"movq	8(%0,%4), %%mm1;"
			"pavgb	8(%1,%4), %%mm1;"
			"movq	%%mm1, %%mm5;"
			"punpcklbw %%mm7, %%mm1;"
			"punpckhbw %%mm7, %%mm5;"
			"pmulhuw %%mm6, %%mm1;"
			"pmulhuw %%mm6, %%mm5;"
			"packuswb %%mm5, %%mm1;"

			"movq	16(%0,%4), %%mm2;"
			"pavgb	16(%1,%4), %%mm2;"
			"movq	%%mm2, %%mm4;"
			"punpcklbw %%mm7, %%mm2;"
			"punpckhbw %%mm7, %%mm4;"
			"pmulhuw %%mm6, %%mm2;"
			"pmulhuw %%mm6, %%mm4;"
			"packuswb %%mm4, %%mm2;"

			"movq	24(%0,%4), %%mm3;"
			"pavgb	24(%1,%4), %%mm3;"
			"movq	%%mm3, %%mm5;"
			"punpcklbw %%mm7, %%mm3;"
			"punpckhbw %%mm7, %%mm5;"
			"pmulhuw %%mm6, %%mm3;"
			"pmulhuw %%mm6, %%mm5;"
			"packuswb %%mm5, %%mm3;"

			"movntq %%mm0,   (%2,%4);"
			"movntq %%mm1,  8(%2,%4);"
			"movntq %%mm2, 16(%2,%4);"
			"movntq %%mm3, 24(%2,%4);"

			"addl	$32, %4;"
			"jnz	1b;"

			"emms;"

			: // no output
			: "r" (src1 + width) // 0
			, "r" (src2 + width) // 1
			, "r" (dst  + width) // 2
			, "r" (factor << 8)  // 3
			, "r" (-4 * width)   // 4
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;

	} else if ((sizeof(Pixel) == 4) && cpu.hasMMX()) {
		// MMX routine, 32bpp
		assert(((4 * width) % 8) == 0);
		asm (
			"movd	%3, %%mm6;"
			"pxor	%%mm7, %%mm7;"
			"punpcklwd %%mm6, %%mm6;"
			"punpckldq %%mm6, %%mm6;"
			".p2align 4,,15;"
		"1:"
			// load
			"movq	(%0,%4), %%mm0;"
			"movq	%%mm0, %%mm1;"
			"movq	(%1,%4), %%mm2;"
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
			"movq %%mm0, (%2,%4);"

			"addl	$8, %4;"
			"jnz	1b;"

			"emms;"

			: // no output
			: "r" (src1 + width) // 0
			, "r" (src2 + width) // 1
			, "r" (dst + width)  // 2
			, "r" (factor << 7)  // 3
			, "r" (-4 * width)   // 4
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3", "mm6", "mm7"
			#endif
		);
		return;
	}

	#ifndef __APPLE__
	// On Mac OS X, we are one register short, because EBX is not available.
	// We disable this piece of assembly and fall back to the C++ code.
	// It's unlikely modern Macs will be running in 16bpp anyway.
	if ((sizeof(Pixel) == 2) && cpu.hasMMXEXT()) {
		// extended-MMX routine, 16bpp
		assert(((2 * width) % 16) == 0);

		darkener.setFactor(factor);
		const Pixel* table = darkener.getTable();
		Pixel mask = ~pixelOps.getBlendMask();

		asm (
			"movd	%5, %%mm7;"
			"pshufw	$0, %%mm7, %%mm7;"

			".p2align 4,,15;"
		"1:"	"movq	 (%0,%4), %%mm0;"
			"movq	8(%0,%4), %%mm1;"
			"movq	 (%1,%4), %%mm2;"
			"movq	8(%1,%4), %%mm3;"

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

			"movntq	%%mm0,   (%3,%4);"
			"movntq	%%mm1,  8(%3,%4);"

			"addl	$16, %4;"
			"jnz	1b;"
			"emms;"
			: // no output
			: "r" (src1 + width) // 0
			, "r" (src2 + width) // 1
			, "r" (table)        // 2
			, "r" (dst + width)  // 3
			, "r" (-2 * width)   // 4
			, "m" (mask)         // 5
			: "eax"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm7"
			#endif
		);
		return;
	}
	#endif // !__APPLE__
	// MMX routine 16bpp is missing, but it's difficult to write because
	// of the missing "pextrw" and "pinsrw" instructions

	#endif

	// non-MMX routine, both 16bpp and 32bpp
	darkener.setFactor(factor);
	for (unsigned x = 0; x < width; ++x) {
		dst[x] = darkener.multiply(
			pixelOps.template blend<1, 1>(src1[x], src2[x]));
	}
}

template <class Pixel>
Pixel Scanline<Pixel>::darken(Pixel p, unsigned factor)
{
	return darkener.multiply(p, factor);
}

template <class Pixel>
Pixel Scanline<Pixel>::darken(Pixel p1, Pixel p2, unsigned factor)
{
	return darkener.multiply(pixelOps.template blend<1, 1>(p1, p2), factor);
}

// Force template instantiation.
template class Scanline<word>;
template class Scanline<unsigned>;

} // namespace openmsx
