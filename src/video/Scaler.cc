// $Id$

#include "Scaler.hh"
#include "SimpleScaler.hh"
#include "SaI2xScaler.hh"
#include "Scale2xScaler.hh"
#include "HQ2xScaler.hh"
#include "HostCPU.hh"
#include <cstring>


namespace openmsx {

// Force template instantiation.
template class Scaler<word>;
template class Scaler<unsigned int>;


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
void Scaler<Pixel>::copyLine(const Pixel* pIn, Pixel* pOut, unsigned width)
{
	const int nBytes = width * sizeof(Pixel);
	const HostCPU& cpu = HostCPU::getInstance();
	if (ASM_X86 && cpu.hasMMXEXT()) {
		// extended-MMX routine (both 16bpp and 32bpp)
		asm (
			"xorl	%%eax, %%eax;"
		"0:"
			// Load.
			"movq	  (%0,%%eax), %%mm0;"
			"movq	 8(%0,%%eax), %%mm1;"
			"movq	16(%0,%%eax), %%mm2;"
			"movq	24(%0,%%eax), %%mm3;"
			"movq	32(%0,%%eax), %%mm4;"
			"movq	40(%0,%%eax), %%mm5;"
			"movq	48(%0,%%eax), %%mm6;"
			"movq	56(%0,%%eax), %%mm7;"
			// Store.
			"movntq	%%mm0,   (%1,%%eax);"
			"movntq	%%mm1,  8(%1,%%eax);"
			"movntq	%%mm2, 16(%1,%%eax);"
			"movntq	%%mm3, 24(%1,%%eax);"
			"movntq	%%mm4, 32(%1,%%eax);"
			"movntq	%%mm5, 40(%1,%%eax);"
			"movntq	%%mm6, 48(%1,%%eax);"
			"movntq	%%mm7, 56(%1,%%eax);"
			// Increment.
			"addl	$64, %%eax;"
			"cmpl	%2, %%eax;"
			"jl	0b;"
			"emms;"
			
			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (nBytes) // 2
			: "eax"
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			);
	} else {
		memcpy(pOut, pIn, nBytes);
	}
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
void Scaler<Pixel>::scaleLine(const Pixel* pIn, Pixel* pOut, unsigned width)
{
	const HostCPU& cpu = HostCPU::getInstance();
	if (ASM_X86 && (sizeof(Pixel) == 2) && cpu.hasMMXEXT()) {
		// extended-MMX routine 16bpp
		asm (
			"xorl	%%eax, %%eax;"
		"0:"
			// Load.
			"movq	  (%0,%%eax,4), %%mm0;"
			"movq	 8(%0,%%eax,4), %%mm2;"
			"movq	16(%0,%%eax,4), %%mm4;"
			"movq	24(%0,%%eax,4), %%mm6;"
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
			"cmpl	%2, %%eax;"
			"jl	0b;"
			"emms;"
	
			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (width) // 2
			: "eax"
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			);
	} else if (ASM_X86 && (sizeof(Pixel) == 4) && cpu.hasMMXEXT()) {
		// extended-MMX routine 32bpp
		asm (
			"xorl	%%eax, %%eax;"
		"0:"
			// Load.
			"movq	  (%0,%%eax,4), %%mm0;"
			"movq	 8(%0,%%eax,4), %%mm2;"
			"movq	16(%0,%%eax,4), %%mm4;"
			"movq	24(%0,%%eax,4), %%mm6;"
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
			"cmpl	%2, %%eax;"
			"jl	0b;"
			"emms;"
	
			: // no output
			: "r" (pIn) // 0
			, "r" (pOut) // 1
			, "r" (width) // 2
			: "eax"
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			);
	} else {
		for (unsigned x = 0; x < width; x++) {
			pOut[x * 2] = pOut[x * 2 + 1] = pIn[x];
		}
	}
}

template <class Pixel>
Scaler<Pixel>::Scaler()
{
}

template <class Pixel>
void Scaler<Pixel>::scaleBlank(
	Pixel colour,
	SDL_Surface* dst, int dstY, int endDstY
) {
	const HostCPU& cpu = HostCPU::getInstance();
	if (ASM_X86 && cpu.hasMMXEXT()) {
		// extended-MMX (both 16bpp and 32bpp) routine
		const unsigned col32 =
				sizeof(Pixel) == 2
			? (((unsigned)colour) << 16) | colour
			: colour;
		while (dstY < endDstY) {
			Pixel* dstLine = Scaler<Pixel>::linePtr(dst, dstY++);
			asm (
				// Precalc colour.
				"movd	%1, %%mm0;"
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
				"cmpl	%2, %%eax;"
				"jl	0b;"
		
				: // no output
				: "r" (dstLine) // 0
				, "rm" (col32) // 1
				, "r" (dst->w * sizeof(Pixel)) // 2: bytes per line
				: "eax", "mm0"
				);
		}
		asm volatile ("emms");
	} else {
		SDL_Rect rect;
		rect.x = 0;
		rect.w = dst->w;
		rect.y = dstY;
		rect.h = endDstY - dstY;
		// Note: SDL_FillRect is generally not allowed on locked surfaces.
		//       However, we're using a software surface, which doesn't
		//       have locking.
		// TODO: But it would be more generic to just write bytes.
		assert(!SDL_MUSTLOCK(dst));
		// Note: return code ignored.
		SDL_FillRect(dst, &rect, colour);
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

} // namespace openmsx

