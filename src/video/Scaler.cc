// $Id$

#include "Scaler.hh"
#include "SimpleScaler.hh"
#include "SaI2xScaler.hh"
#include "Scale2xScaler.hh"
#include "HQ2xScaler.hh"
#include "HQ2xLiteScaler.hh"
#include "HostCPU.hh"
#include <cstring>

using std::auto_ptr;

namespace openmsx {

template <class Pixel>
auto_ptr<Scaler<Pixel> > Scaler<Pixel>::createScaler(
	ScalerID id, SDL_PixelFormat* format)
{
	switch(id) {
	case SCALER_SIMPLE:
		return auto_ptr<Scaler<Pixel> >(new SimpleScaler<Pixel>(format));
	case SCALER_SAI2X:
		return auto_ptr<Scaler<Pixel> >(new SaI2xScaler<Pixel>(format));
	case SCALER_SCALE2X:
		return auto_ptr<Scaler<Pixel> >(new Scale2xScaler<Pixel>());
	case SCALER_HQ2X:
		return auto_ptr<Scaler<Pixel> >(new HQ2xScaler<Pixel>());
	case SCALER_HQ2XLITE:
		return auto_ptr<Scaler<Pixel> >(new HQ2xLiteScaler<Pixel>());
	default:
		assert(false);
		return auto_ptr<Scaler<Pixel> >();
	}
}

template <class Pixel>
void Scaler<Pixel>::copyLine(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY )
{
	assert(src->w == dst->w);
	Pixel* pIn  = linePtr(src, srcY);
	Pixel* pOut = linePtr(dst, dstY);
	copyLine(pIn, pOut, src->w);
}

template <class Pixel>
void Scaler<Pixel>::copyLine(const Pixel* pIn, Pixel* pOut, unsigned width,
                             bool inCache)
{
	const int nBytes = width * sizeof(Pixel);

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
void Scaler<Pixel>::scaleLine(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY )
{
	assert(dst->w == 640);
	Pixel* pIn  = linePtr(src, srcY);
	Pixel* pOut = linePtr(dst, dstY);
	scaleLine(pIn, pOut, 320);
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

template <class Pixel>
Scaler<Pixel>::Scaler()
{
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
void Scaler<Pixel>::scaleBlank(
	Pixel colour,
	SDL_Surface* dst, int dstY, int endDstY)
{
	while (dstY < endDstY) {
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, dstY++);
		fillLine(dstLine, colour, dst->w);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	while (srcY < endSrcY) {
		scaleLine(src, srcY, dst, dstY++);
		if (dstY == dst->h) break;
		scaleLine(src, srcY, dst, dstY++);
		srcY++;
	}
}

template <class Pixel>
void Scaler<Pixel>::scale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	while (srcY < endSrcY) {
		copyLine(src, srcY, dst, dstY++);
		if (dstY == dst->h) break;
		copyLine(src, srcY, dst, dstY++);
		srcY++;
	}
}


// Force template instantiation.
template class Scaler<word>;
template class Scaler<unsigned int>;

} // namespace openmsx

