// $Id$

#include "Scaler.hh"
#include "SimpleScaler.hh"
#include "SaI2xScaler.hh"
#include "Scale2xScaler.hh"
#include "HQ2xScaler.hh"
#include "HQ3xScaler.hh"
#include "HQ2xLiteScaler.hh"
#include "HQ3xLiteScaler.hh"
#include "RGBTriplet3xScaler.hh"
#include "LowScaler.hh"
#include "FrameSource.hh"
#include "HostCPU.hh"
#include <algorithm>
#include <cstring>

using std::auto_ptr;

namespace openmsx {

template <class Pixel>
auto_ptr<Scaler<Pixel> > Scaler<Pixel>::createScaler(
	ScalerID id, SDL_PixelFormat* format, RenderSettings& renderSettings)
{
	switch(id) {
	case SCALER_SIMPLE:
		return auto_ptr<Scaler<Pixel> >(
			new SimpleScaler<Pixel>(format, renderSettings));
	case SCALER_SAI2X:
		return auto_ptr<Scaler<Pixel> >(
			new SaI2xScaler<Pixel>(format));
	case SCALER_SCALE2X:
		return auto_ptr<Scaler<Pixel> >(
			new Scale2xScaler<Pixel>(format));
	case SCALER_HQ2X:
		return auto_ptr<Scaler<Pixel> >(
			new HQ2xScaler<Pixel>(format));
	case SCALER_HQ3X:
		return auto_ptr<Scaler<Pixel> >(
			new HQ3xScaler<Pixel>(format));
	case SCALER_HQ2XLITE:
		return auto_ptr<Scaler<Pixel> >(
			new HQ2xLiteScaler<Pixel>(format));
	case SCALER_HQ3XLITE:
		return auto_ptr<Scaler<Pixel> >(
			new HQ3xLiteScaler<Pixel>(format));
	case SCALER_RGBTRIPLET3X:
		return auto_ptr<Scaler<Pixel> >(
			new RGBTriplet3xScaler<Pixel>(format, renderSettings));
	case SCALER_LOW:
		return auto_ptr<Scaler<Pixel> >(
			new LowScaler<Pixel>(format));
	default:
		assert(false);
		return auto_ptr<Scaler<Pixel> >();
	}
}

template <class Pixel>
Scaler<Pixel>::Scaler(SDL_PixelFormat* format)
	: pixelOps(format)
{
}

template <class Pixel>
void Scaler<Pixel>::copyLine(const Pixel* pIn, Pixel* pOut, unsigned width,
                             bool inCache)
{
	unsigned nBytes = width * sizeof(Pixel);

	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if (!inCache && cpu.hasMMXEXT()) {
		// extended-MMX routine (both 16bpp and 32bpp)
		asm (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%0,%3), %%mm0;"
			"movq	 8(%0,%3), %%mm1;"
			"movq	16(%0,%3), %%mm2;"
			"movq	24(%0,%3), %%mm3;"
			"movq	32(%0,%3), %%mm4;"
			"movq	40(%0,%3), %%mm5;"
			"movq	48(%0,%3), %%mm6;"
			"movq	56(%0,%3), %%mm7;"
			// Store.
			"movntq	%%mm0,   (%1,%3);"
			"movntq	%%mm1,  8(%1,%3);"
			"movntq	%%mm2, 16(%1,%3);"
			"movntq	%%mm3, 24(%1,%3);"
			"movntq	%%mm4, 32(%1,%3);"
			"movntq	%%mm5, 40(%1,%3);"
			"movntq	%%mm6, 48(%1,%3);"
			"movntq	%%mm7, 56(%1,%3);"
			// Increment.
			"addl	$64, %3;"
			"cmpl	%2, %3;"
			"jl	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (nBytes) // 2
			, "r" (0) // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3",
			  "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}

	if (cpu.hasMMX()) {
		// MMX routine (both 16bpp and 32bpp)
		asm (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%0,%3), %%mm0;"
			"movq	 8(%0,%3), %%mm1;"
			"movq	16(%0,%3), %%mm2;"
			"movq	24(%0,%3), %%mm3;"
			"movq	32(%0,%3), %%mm4;"
			"movq	40(%0,%3), %%mm5;"
			"movq	48(%0,%3), %%mm6;"
			"movq	56(%0,%3), %%mm7;"
			// Store.
			"movq	%%mm0,   (%1,%3);"
			"movq	%%mm1,  8(%1,%3);"
			"movq	%%mm2, 16(%1,%3);"
			"movq	%%mm3, 24(%1,%3);"
			"movq	%%mm4, 32(%1,%3);"
			"movq	%%mm5, 40(%1,%3);"
			"movq	%%mm6, 48(%1,%3);"
			"movq	%%mm7, 56(%1,%3);"
			// Increment.
			"addl	$64, %3;"
			"cmpl	%2, %3;"
			"jl	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (nBytes) // 2
			, "r" (0) // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3",
			  "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}
	#endif

	memcpy(pOut, pIn, nBytes);
}

template <class Pixel>
void Scaler<Pixel>::scaleLine(const Pixel* pIn, Pixel* pOut, unsigned width,
                              bool inCache)
{
	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if ((sizeof(Pixel) == 2) && !inCache && cpu.hasMMXEXT()) {
		// extended-MMX routine 16bpp
		asm (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%0,%3,4), %%mm0;"
			"movq	 8(%0,%3,4), %%mm2;"
			"movq	16(%0,%3,4), %%mm4;"
			"movq	24(%0,%3,4), %%mm6;"
			"movq	%%mm0, %%mm1;"
			"movq	%%mm2, %%mm3;"
			"movq	%%mm4, %%mm5;"
			"movq	%%mm6, %%mm7;"
			// Scale.
			"punpcklwd %%mm0, %%mm0;"
			"punpckhwd %%mm1, %%mm1;"
			"punpcklwd %%mm2, %%mm2;"
			"punpckhwd %%mm3, %%mm3;"
			"punpcklwd %%mm4, %%mm4;"
			"punpckhwd %%mm5, %%mm5;"
			"punpcklwd %%mm6, %%mm6;"
			"punpckhwd %%mm7, %%mm7;"
			// Store.
			"movntq	%%mm0,   (%1,%3,8);"
			"movntq	%%mm1,  8(%1,%3,8);"
			"movntq	%%mm2, 16(%1,%3,8);"
			"movntq	%%mm3, 24(%1,%3,8);"
			"movntq	%%mm4, 32(%1,%3,8);"
			"movntq	%%mm5, 40(%1,%3,8);"
			"movntq	%%mm6, 48(%1,%3,8);"
			"movntq	%%mm7, 56(%1,%3,8);"
			// Increment.
			"addl	$8, %3;"
			"cmpl	%2, %3;"
			"jl	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (width) // 2
			, "r" (0) // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3",
			  "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}

	if ((sizeof(Pixel) == 2) && cpu.hasMMX()) {
		// MMX routine 16bpp
		asm (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%0,%3,4), %%mm0;"
			"movq	 8(%0,%3,4), %%mm2;"
			"movq	16(%0,%3,4), %%mm4;"
			"movq	24(%0,%3,4), %%mm6;"
			"movq	%%mm0, %%mm1;"
			"movq	%%mm2, %%mm3;"
			"movq	%%mm4, %%mm5;"
			"movq	%%mm6, %%mm7;"
			// Scale.
			"punpcklwd %%mm0, %%mm0;"
			"punpckhwd %%mm1, %%mm1;"
			"punpcklwd %%mm2, %%mm2;"
			"punpckhwd %%mm3, %%mm3;"
			"punpcklwd %%mm4, %%mm4;"
			"punpckhwd %%mm5, %%mm5;"
			"punpcklwd %%mm6, %%mm6;"
			"punpckhwd %%mm7, %%mm7;"
			// Store.
			"movq	%%mm0,   (%1,%3,8);"
			"movq	%%mm1,  8(%1,%3,8);"
			"movq	%%mm2, 16(%1,%3,8);"
			"movq	%%mm3, 24(%1,%3,8);"
			"movq	%%mm4, 32(%1,%3,8);"
			"movq	%%mm5, 40(%1,%3,8);"
			"movq	%%mm6, 48(%1,%3,8);"
			"movq	%%mm7, 56(%1,%3,8);"
			// Increment.
			"addl	$8, %3;"
			"cmpl	%2, %3;"
			"jl	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (width) // 2
			, "r" (0) // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3",
			  "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}

	if ((sizeof(Pixel) == 4) && !inCache && cpu.hasMMXEXT()) {
		// extended-MMX routine 32bpp
		asm (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%0,%3,4), %%mm0;"
			"movq	 8(%0,%3,4), %%mm2;"
			"movq	16(%0,%3,4), %%mm4;"
			"movq	24(%0,%3,4), %%mm6;"
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
			"movntq	%%mm0,   (%1,%3,8);"
			"movntq	%%mm1,  8(%1,%3,8);"
			"movntq	%%mm2, 16(%1,%3,8);"
			"movntq	%%mm3, 24(%1,%3,8);"
			"movntq	%%mm4, 32(%1,%3,8);"
			"movntq	%%mm5, 40(%1,%3,8);"
			"movntq	%%mm6, 48(%1,%3,8);"
			"movntq	%%mm7, 56(%1,%3,8);"
			// Increment.
			"addl	$8, %3;"
			"cmpl	%2, %3;"
			"jl	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (width) // 2
			, "r" (0) // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3",
			  "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}

	if ((sizeof(Pixel) == 4) && cpu.hasMMX()) {
		// MMX routine 32bpp
		asm (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%0,%3,4), %%mm0;"
			"movq	 8(%0,%3,4), %%mm2;"
			"movq	16(%0,%3,4), %%mm4;"
			"movq	24(%0,%3,4), %%mm6;"
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
			"movq	%%mm0,   (%1,%3,8);"
			"movq	%%mm1,  8(%1,%3,8);"
			"movq	%%mm2, 16(%1,%3,8);"
			"movq	%%mm3, 24(%1,%3,8);"
			"movq	%%mm4, 32(%1,%3,8);"
			"movq	%%mm5, 40(%1,%3,8);"
			"movq	%%mm6, 48(%1,%3,8);"
			"movq	%%mm7, 56(%1,%3,8);"
			// Increment.
			"addl	$8, %3;"
			"cmpl	%2, %3;"
			"jl	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (width) // 2
			, "r" (0) // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3",
			  "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}
	#endif

	for (unsigned x = 0; x < width; x++) {
		pOut[x * 2] = pOut[x * 2 + 1] = pIn[x];
	}
}

template <typename Pixel>
void Scaler<Pixel>::average(const Pixel* pIn0, const Pixel* pIn1, Pixel* pOut,
                            unsigned width)
{
	// TODO MMX/SSE optimizations
	// pure C++ version
	for (unsigned i = 0; i < width; ++i) {
		pOut[i] = blend(pIn0[i], pIn1[i]);
	}
}

template <typename Pixel>
void Scaler<Pixel>::halve(const Pixel* pIn, Pixel* pOut, unsigned width)
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
			"cmpl	%2, %%eax;"
			"jne	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (width * 4) // 2
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
			"cmpl	%2, %%eax;"
			"jne	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (width * 4) // 2
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
		unsigned mask = pixelOps.getBlendMask();
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
			"cmpl	%3, %%eax;"
			"jne	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (mask) // 2
			, "r" (width * 2) // 3
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
		unsigned mask = pixelOps.getBlendMask();
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
			"cmpl	%3, %%eax;"
			"jne	0b;"
			"emms;"

			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (mask) // 2
			, "r" (width * 2) // 3
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
	for (unsigned i = 0; i < width; ++i) {
		pOut[i] = blend(pIn[2 * i + 0], pIn[2 * i + 1]);
	}
}


template <class Pixel>
void Scaler<Pixel>::fillLine(Pixel* pOut, Pixel colour, unsigned width)
{
	const unsigned col32 = (sizeof(Pixel) == 2)
		? (((unsigned)colour) << 16) | colour
		: colour;

	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if (cpu.hasMMXEXT()) {
		// extended-MMX routine (both 16bpp and 32bpp)
		asm (
			// Precalc colour.
			"movd	%1, %%mm0;"
			"punpckldq	%%mm0, %%mm0;"
			".p2align 4,,15;"
		"0:"
			// Store.
			"movntq	%%mm0,   (%0,%3);"
			"movntq	%%mm0,  8(%0,%3);"
			"movntq	%%mm0, 16(%0,%3);"
			"movntq	%%mm0, 24(%0,%3);"
			"movntq	%%mm0, 32(%0,%3);"
			"movntq	%%mm0, 40(%0,%3);"
			"movntq	%%mm0, 48(%0,%3);"
			"movntq	%%mm0, 56(%0,%3);"
			// Increment.
			"addl	$64, %3;"
			"cmpl	%2, %3;"
			"jl	0b;"
			"emms;"

			: // no output
			: "r" (pOut) // 0
			, "rm" (col32) // 1
			, "r" (width * sizeof(Pixel)) // 2
			, "r" (0) // 3
			#ifdef __MMX__
			: "mm0"
			#endif
		);
		return;
	}

	if (cpu.hasMMX()) {
		// MMX routine (both 16bpp and 32bpp)
		asm (
			// Precalc colour.
			"movd	%1, %%mm0;"
			"punpckldq	%%mm0, %%mm0;"
			".p2align 4,,15;"
		"0:"
			// Store.
			"movq	%%mm0,   (%0,%3);"
			"movq	%%mm0,  8(%0,%3);"
			"movq	%%mm0, 16(%0,%3);"
			"movq	%%mm0, 24(%0,%3);"
			"movq	%%mm0, 32(%0,%3);"
			"movq	%%mm0, 40(%0,%3);"
			"movq	%%mm0, 48(%0,%3);"
			"movq	%%mm0, 56(%0,%3);"
			// Increment.
			"addl	$64, %3;"
			"cmpl	%2, %3;"
			"jl	0b;"
			"emms;"

			: // no output
			: "r" (pOut) // 0
			, "rm" (col32) // 1
			, "r" (width * sizeof(Pixel)) // 2
			, "r" (0) // 3
			#ifdef __MMX__
			: "mm0"
			#endif
		);
		return;
	}
	#endif

	unsigned* p = reinterpret_cast<unsigned*>(pOut);
	unsigned num = width * sizeof(Pixel) / sizeof(unsigned);
	for (unsigned i = 0; i < num; ++i) {
		*p++ = col32;
	}
}



template <class Pixel>
void Scaler<Pixel>::scale_1on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; ++p) {
		Pixel pix = inPixels[p];
		outPixels[3 * p + 0] = pix;
		outPixels[3 * p + 1] = pix;
		outPixels[3 * p + 2] = pix;
	}
}

template <class Pixel>
void Scaler<Pixel>::scale_1on2(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	scaleLine(inPixels, outPixels, nrPixels);
}

template <class Pixel>
void Scaler<Pixel>::scale_1on1(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	copyLine(inPixels, outPixels, nrPixels);
}

template <class Pixel>
void Scaler<Pixel>::scale_2on1(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	halve(inPixels, outPixels, nrPixels / 2);
}

template <class Pixel>
void Scaler<Pixel>::scale_4on1(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		*outPixels++ = pixelOps.template blend4<1, 1, 1, 1>(&inPixels[p]);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale_2on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		*outPixels++ =                                 inPixels[p + 0];
		*outPixels++ = pixelOps.template blend2<1, 1>(&inPixels[p + 0]);
		*outPixels++ =                                 inPixels[p + 1];
	}
}

template <class Pixel>
void Scaler<Pixel>::scale_4on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		*outPixels++ = pixelOps.template blend2<3, 1>(&inPixels[p + 0]);
		*outPixels++ = pixelOps.template blend2<1, 1>(&inPixels[p + 1]);
		*outPixels++ = pixelOps.template blend2<1, 3>(&inPixels[p + 2]);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale_8on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 8) {
		*outPixels++ = pixelOps.template blend3<3, 3, 2>   (&inPixels[p + 0]);
		*outPixels++ = pixelOps.template blend4<1, 3, 3, 1>(&inPixels[p + 2]);
		*outPixels++ = pixelOps.template blend3<2, 3, 3>   (&inPixels[p + 5]);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale_2on9(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		*outPixels++ =                                 inPixels[p + 0];
		*outPixels++ =                                 inPixels[p + 0];
		*outPixels++ =                                 inPixels[p + 0];
		*outPixels++ =                                 inPixels[p + 0];
		*outPixels++ = pixelOps.template blend2<1, 1>(&inPixels[p + 0]);
		*outPixels++ =                                 inPixels[p + 1];
		*outPixels++ =                                 inPixels[p + 1];
		*outPixels++ =                                 inPixels[p + 1];
		*outPixels++ =                                 inPixels[p + 1];
	}
}

template <class Pixel>
void Scaler<Pixel>::scale_4on9(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		*outPixels++ =                                 inPixels[p + 0];
		*outPixels++ =                                 inPixels[p + 0];
		*outPixels++ = pixelOps.template blend2<1, 3>(&inPixels[p + 0]);
		*outPixels++ =                                 inPixels[p + 1];
		*outPixels++ = pixelOps.template blend2<1, 1>(&inPixels[p + 1]);
		*outPixels++ =                                 inPixels[p + 2];
		*outPixels++ = pixelOps.template blend2<3, 1>(&inPixels[p + 2]);
		*outPixels++ =                                 inPixels[p + 3];
		*outPixels++ =                                 inPixels[p + 3];
	}
}

template <class Pixel>
void Scaler<Pixel>::scale_8on9(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		*outPixels++ =                                 inPixels[p + 0];
		*outPixels++ = pixelOps.template blend2<1, 7>(&inPixels[p + 0]);
		*outPixels++ = pixelOps.template blend2<1, 3>(&inPixels[p + 1]);
		*outPixels++ = pixelOps.template blend2<3, 5>(&inPixels[p + 2]);
		*outPixels++ = pixelOps.template blend2<1, 1>(&inPixels[p + 3]);
		*outPixels++ = pixelOps.template blend2<5, 3>(&inPixels[p + 4]);
		*outPixels++ = pixelOps.template blend2<3, 1>(&inPixels[p + 5]);
		*outPixels++ = pixelOps.template blend2<7, 1>(&inPixels[p + 6]);
		*outPixels++ =                                 inPixels[p + 7];
	}
}

template <class Pixel>
void Scaler<Pixel>::scaleBlank(Pixel color, SDL_Surface* dst,
                               unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		fillLine(dstLine, color, dst->w);
	}
}

// Force template instantiation.
template class Scaler<word>;
template class Scaler<unsigned>;

} // namespace openmsx

