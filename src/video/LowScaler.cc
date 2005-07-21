// $Id$

#include "LowScaler.hh"
#include "RawFrame.hh"
#include "HostCPU.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

template <typename Pixel>
LowScaler<Pixel>::LowScaler(SDL_PixelFormat* format)
	: blender(Blender<Pixel>::createFromFormat(format))
{
}

template <typename Pixel>
LowScaler<Pixel>::~LowScaler()
{
}

template <typename Pixel>
void LowScaler<Pixel>::halve(const Pixel* pIn, Pixel* pOut)
{
	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if ((sizeof(Pixel) == 4) && cpu.hasMMXEXT()) {
		// extended-MMX routine, 32bpp 
		asm volatile (
			"xorl %%eax, %%eax;"
			".p2align 4,,15;"
		"0:"
			"movq	  (%0,%%eax,2), %%mm0;" // 0 = AB
			"movq	 8(%0,%%eax,2), %%mm1;" // 1 = CD
			"movq	16(%0,%%eax,2), %%mm2;" // 2 = EF
			"movq	24(%0,%%eax,2), %%mm3;" // 3 = GH
			"movq	%%mm0, %%mm4;"          // 4 = AB
			"punpckhdq	%%mm1, %%mm0;"  // 0 = BD
			"punpckldq	%%mm1, %%mm4;"  // 4 = AC
			"movq	%%mm2, %%mm5;"          // 5 = EF
			"punpckhdq	%%mm3, %%mm2;"  // 2 = FH
			"punpckldq	%%mm3, %%mm5;"  // 5 = EG
			"pavgb	%%mm0, %%mm4;"          // 4 = ab cd
			"movntq	%%mm4,  (%1,%%eax);"
			"pavgb	%%mm2, %%mm5;"          // 5 = ef gh
			"movntq	%%mm5, 8(%1,%%eax);"
			"addl	$16, %%eax;"
			"cmpl	$1280, %%eax;"
			"jne	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			: "eax"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3", "mm4", "mm5"
			#endif
		);
		return;
	}
	if ((sizeof(Pixel) == 4) && cpu.hasMMX()) {
		// MMX routine, 32bpp 
		asm volatile (
			"xorl	%%eax, %%eax;"
			"pxor	%%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq	  (%0,%%eax,2), %%mm0;" // 0 = AB
			"movq	%%mm0, %%mm4;"          // 4 = AB
			"punpckhbw	%%mm7, %%mm0;"  // 0 = 0B
			"movq	 8(%0,%%eax,2), %%mm1;" // 1 = CD
			"movq	16(%0,%%eax,2), %%mm2;" // 2 = EF
			"punpcklbw	%%mm7, %%mm4;"  // 4 = 0A
			"movq	%%mm1, %%mm5;"          // 5 = CD
			"paddw	%%mm4, %%mm0;"          // 0 = A + B
			"punpckhbw	%%mm7, %%mm1;"  // 1 = 0D
			"punpcklbw	%%mm7, %%mm5;"  // 5 = 0C
			"psrlw	$1, %%mm0;"             // 0 = (A + B) / 2 
			"paddw	%%mm5, %%mm1;"          // 1 = C + D
			"movq	%%mm2, %%mm4;"          // 4 = EF
			"punpckhbw	%%mm7, %%mm2;"  // 2 = 0F
			"punpcklbw	%%mm7, %%mm4;"  // 4 = 0E
			"psrlw	$1, %%mm1;"             // 1 = (C + D) / 2
			"paddw	%%mm4, %%mm2;"          // 2 = E + F
			"movq	24(%0,%%eax,2), %%mm3;" // 3 = GH
			"movq	%%mm3, %%mm5;"          // 5 = GH
			"punpckhbw	%%mm7, %%mm3;"  // 3 = 0H
			"packuswb	%%mm1, %%mm0;"  // 0 = ab cd
			"punpcklbw	%%mm7, %%mm5;"  // 5 = 0G
			"psrlw	$1, %%mm2;"             // 2 = (E + F) / 2
			"paddw	%%mm5, %%mm3;"          // 3 = G + H
			"psrlw	$1, %%mm3;"             // 3 = (G + H) / 2
			"packuswb	%%mm3, %%mm2;"  // 2 = ef gh
			"movq	%%mm0,  (%1,%%eax);"
			"movq	%%mm2, 8(%1,%%eax);"
			"addl	$16, %%eax;"
			"cmpl	$1280, %%eax;"
			"jne	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			: "eax"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}
	if ((sizeof(Pixel) == 2) && cpu.hasMMXEXT()) {
		// extended-MMX routine, 16bpp 
		unsigned mask = blender.getMask();
		mask = ~(mask | (mask << 16));
		asm volatile (
			"movd	%2, %%mm7;"
			"xorl %%eax, %%eax;"
			"punpckldq	%%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq	  (%0,%%eax,2), %%mm0;" // 0 = ABCD
			"movq	 8(%0,%%eax,2), %%mm1;" // 1 = EFGH
			"movq	%%mm0, %%mm4;"          // 4 = ABCD
			"movq	16(%0,%%eax,2), %%mm2;" // 2 = IJKL
			"punpcklwd	%%mm1, %%mm0;"  // 0 = AEBF
			"punpckhwd	%%mm1, %%mm4;"  // 4 = CGDH
			"movq	%%mm0, %%mm6;"          // 6 = AEBF
			"movq	24(%0,%%eax,2), %%mm3;" // 3 = MNOP
			"movq	%%mm2, %%mm5;"          // 5 = IJKL
			"punpckhwd	%%mm4, %%mm0;"  // 0 = BDFH
			"punpcklwd	%%mm4, %%mm6;"  // 6 = ACEG
			"punpcklwd	%%mm3, %%mm2;"  // 2 = IMJN
			"punpckhwd	%%mm3, %%mm5;"  // 5 = KOLP
			"movq	%%mm2, %%mm1;"          // 1 = IMJN
			"movq	%%mm6, %%mm3;"          // 3 = ACEG
			"movq	%%mm7, %%mm4;"          // 4 = M
			"punpckhwd	%%mm5, %%mm2;"  // 2 = JLNP
			"punpcklwd	%%mm5, %%mm1;"  // 1 = IKMO
			"pandn	%%mm6, %%mm4;"          // 4 = ACEG & ~M
			"pand	%%mm7, %%mm3;"          // 3 = ACEG & M
			"pand	%%mm7, %%mm2;"          // 2 = JLNP & M
			"pand	%%mm7, %%mm0;"          // 0 = BDFH & M
			"movq	%%mm1, %%mm6;"          // 6 = IKMO
			"movq	%%mm7, %%mm5;"          // 5 = M
			"psrlw	$1, %%mm3;"             // 3 = (ACEG & M) >> 1
			"psrlw	$1, %%mm2;"             // 2 = (JLNP & M) >> 1
			"pand	%%mm7, %%mm6;"          // 6 = IKMO & M
			"psrlw	$1, %%mm0;"             // 0 = (BDFH & M) >> 1
			"pandn	%%mm1, %%mm5;"          // 5 = IKMO & ~M
			"psrlw	$1, %%mm6;"             // 6 = (IKMO & M) >> 1
			"paddw	%%mm4, %%mm3;"          // 3 = ACEG & M  +  ACEG & ~M
			"paddw	%%mm2, %%mm6;"          // 6 = IKMO & M  +  JLNP & M
			"paddw	%%mm0, %%mm3;"          // 3 = ab cd ef gh
			"paddw	%%mm5, %%mm6;"          // 6 = ij kl mn op
			"movntq	%%mm3,  (%1,%%eax);"
			"movntq	%%mm6, 8(%1,%%eax);"
			"addl	$16, %%eax;"
			"cmpl	$640, %%eax;"
			"jne	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (mask) // 2
			: "eax"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}
	if ((sizeof(Pixel) == 2) && cpu.hasMMX()) {
		// MMX routine, 16bpp 
		unsigned mask = blender.getMask();
		mask = ~(mask | (mask << 16));
		asm volatile (
			"movd	%2, %%mm7;"
			"xorl %%eax, %%eax;"
			"punpckldq	%%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq	  (%0,%%eax,2), %%mm0;" // 0 = ABCD
			"movq	 8(%0,%%eax,2), %%mm1;" // 1 = EFGH
			"movq	%%mm0, %%mm4;"          // 4 = ABCD
			"movq	16(%0,%%eax,2), %%mm2;" // 2 = IJKL
			"punpcklwd	%%mm1, %%mm0;"  // 0 = AEBF
			"punpckhwd	%%mm1, %%mm4;"  // 4 = CGDH
			"movq	%%mm0, %%mm6;"          // 6 = AEBF
			"movq	24(%0,%%eax,2), %%mm3;" // 3 = MNOP
			"movq	%%mm2, %%mm5;"          // 5 = IJKL
			"punpckhwd	%%mm4, %%mm0;"  // 0 = BDFH
			"punpcklwd	%%mm4, %%mm6;"  // 6 = ACEG
			"punpcklwd	%%mm3, %%mm2;"  // 2 = IMJN
			"punpckhwd	%%mm3, %%mm5;"  // 5 = KOLP
			"movq	%%mm2, %%mm1;"          // 1 = IMJN
			"movq	%%mm6, %%mm3;"          // 3 = ACEG
			"movq	%%mm7, %%mm4;"          // 4 = M
			"punpckhwd	%%mm5, %%mm2;"  // 2 = JLNP
			"punpcklwd	%%mm5, %%mm1;"  // 1 = IKMO
			"pandn	%%mm6, %%mm4;"          // 4 = ACEG & ~M
			"pand	%%mm7, %%mm3;"          // 3 = ACEG & M
			"pand	%%mm7, %%mm2;"          // 2 = JLNP & M
			"pand	%%mm7, %%mm0;"          // 0 = BDFH & M
			"movq	%%mm1, %%mm6;"          // 6 = IKMO
			"movq	%%mm7, %%mm5;"          // 5 = M
			"psrlw	$1, %%mm3;"             // 3 = (ACEG & M) >> 1
			"psrlw	$1, %%mm2;"             // 2 = (JLNP & M) >> 1
			"pand	%%mm7, %%mm6;"          // 6 = IKMO & M
			"psrlw	$1, %%mm0;"             // 0 = (BDFH & M) >> 1
			"pandn	%%mm1, %%mm5;"          // 5 = IKMO & ~M
			"psrlw	$1, %%mm6;"             // 6 = (IKMO & M) >> 1
			"paddw	%%mm4, %%mm3;"          // 3 = ACEG & M  +  ACEG & ~M
			"paddw	%%mm2, %%mm6;"          // 6 = IKMO & M  +  JLNP & M
			"paddw	%%mm0, %%mm3;"          // 3 = ab cd ef gh
			"paddw	%%mm5, %%mm6;"          // 6 = ij kl mn op
			"movq	%%mm3,  (%1,%%eax);"
			"movq	%%mm6, 8(%1,%%eax);"
			"addl	$16, %%eax;"
			"cmpl	$640, %%eax;"
			"jne	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (mask) // 2
			: "eax"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}
	#endif
	
	// pure C++ version
	for (int i = 0; i < 320; ++i) {
		pOut[i] = blender.blend(pIn[2 * i + 0], pIn[2 * i + 1]);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::average(const Pixel* pIn0, const Pixel* pIn1, Pixel* pOut)
{
	// TODO MMX/SSE optimizations
	// pure C++ version
	for (int i = 0; i < 320; ++i) {
		pOut[i] = blender.blend(pIn0[i], pIn1[i]);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::averageHalve(const Pixel* pIn0, const Pixel* pIn1, Pixel* pOut)
{
	// TODO MMX/SSE optimizations
	// pure C++ version
	for (int i = 0; i < 320; ++i) {
		Pixel tmp0 = blender.blend(pIn0[2 * i + 0], pIn0[2 * i + 1]);
		Pixel tmp1 = blender.blend(pIn1[2 * i + 0], pIn1[2 * i + 1]);
		pOut[i] = blender.blend(tmp0, tmp1);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::scaleBlank(Pixel color, SDL_Surface* dst,
                                  unsigned startY, unsigned endY, bool /*lower*/)
{
	for (unsigned y = startY; y < endY; ++y) {
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		Scaler<Pixel>::fillLine(dstLine, color, 320);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::scale256(RawFrame& src, SDL_Surface* dst,
                                unsigned startY, unsigned endY, bool /*lower*/)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine = src.getPixelPtr(0, y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		Scaler<Pixel>::copyLine(srcLine, dstLine, 320, false);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::scale256(RawFrame& src0, RawFrame& src1, SDL_Surface* dst,
                                unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getPixelPtr(0, y, (Pixel*)0);
		const Pixel* srcLine1 = src1.getPixelPtr(0, y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		average(srcLine0, srcLine1, dstLine);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::scale512(RawFrame& src, SDL_Surface* dst,
                                unsigned startY, unsigned endY, bool /*lower*/)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine = src.getPixelPtr(0, y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		halve(srcLine, dstLine);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::scale512(RawFrame& src0, RawFrame& src1, SDL_Surface* dst,
                                unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getPixelPtr(0, y, (Pixel*)0);
		const Pixel* srcLine1 = src1.getPixelPtr(0, y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		averageHalve(srcLine0, srcLine1, dstLine);
	}
}


// Force template instantiation.
template class LowScaler<word>;
template class LowScaler<unsigned int>;

} // namespace openmsx
