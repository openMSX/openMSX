// $Id$

#include "Scaler.hh"
#include "SimpleScaler.hh"
#include "SaI2xScaler.hh"
#include "Scale2xScaler.hh"
#include "HQ2xScaler.hh"
#include "BlurScaler.hh"
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
		return auto_ptr<Scaler<Pixel> >(new SimpleScaler<Pixel>());
	case SCALER_SAI2X:
		return auto_ptr<Scaler<Pixel> >(new SaI2xScaler<Pixel>(format));
	case SCALER_SCALE2X:
		return auto_ptr<Scaler<Pixel> >(new Scale2xScaler<Pixel>());
	case SCALER_HQ2X:
		return auto_ptr<Scaler<Pixel> >(new HQ2xScaler<Pixel>());
	case SCALER_BLUR:
		return auto_ptr<Scaler<Pixel> >(new BlurScaler<Pixel>(format));
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
	byte* srcLine = (byte*)linePtr(src, srcY);
	byte* dstLine = (byte*)linePtr(dst, dstY);
	const int nBytes = dst->w * sizeof(Pixel);
	const HostCPU& cpu = HostCPU::getInstance();
	if (ASM_X86 && cpu.hasMMXEXT()) {
		asm (
			"xorl	%%eax, %%eax;"
		"0:"
			// Load.
			"movq	  (%0,%%eax), %%mm0;"
			"movq	 8(%0,%%eax), %%mm1;"
			"movq	16(%0,%%eax), %%mm2;"
			"movq	24(%0,%%eax), %%mm3;"
			// Store.
			"movntq	%%mm0,   (%1,%%eax);"
			"movntq	%%mm1,  8(%1,%%eax);"
			"movntq	%%mm2, 16(%1,%%eax);"
			"movntq	%%mm3, 24(%1,%%eax);"
			// Increment.
			"addl	$32, %%eax;"
			"cmpl	%2, %%eax;"
			"jl	0b;"
			"emms;"
			
			: // no output
			: "r" (srcLine) // 0
			, "r" (dstLine) // 1
			, "r" (nBytes) // 2
			: "eax", "mm0", "mm1", "mm2", "mm3"
			);
	} else {
		memcpy(dstLine, srcLine, nBytes);
	}
}

template <class Pixel>
void Scaler<Pixel>::scaleLine(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY )
{
	const int width = 320; // TODO: Specify this in a clean way.
	assert(dst->w == width * 2);
	byte* srcLine = (byte*)linePtr(src, srcY);
	byte* dstLine = (byte*)linePtr(dst, dstY);
	const HostCPU& cpu = HostCPU::getInstance();
	if (ASM_X86 && cpu.hasMMXEXT()) {
		asm (
			"xorl	%%eax, %%eax;"
		"0:"
			// Load.
			"movq	(%0,%%eax,4), %%mm0;"
			"movq	%%mm0, %%mm1;"
			// Scale.
			"punpckldq %%mm0, %%mm0;"
			"punpckhdq %%mm1, %%mm1;"
			// Store.
			"movntq	%%mm0,  (%1,%%eax,8);"
			"movntq	%%mm1, 8(%1,%%eax,8);"
			// Increment.
			"addl	$2, %%eax;"
			"cmpl	%2, %%eax;"
			"jl	0b;"
			"emms;"
	
			: // no output
			: "r" (srcLine) // 0
			, "r" (dstLine) // 1
			, "r" (width) // 2
			: "mm0", "mm1"
			, "eax"
			);
	} else {
		for (int x = 0; x < width; x++) {
			dstLine[x * 2] = dstLine[x * 2 + 1] = srcLine[x];
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
				"cmpl	%2, %%eax;"
				"jl	0b;"
		
				: // no output
				: "r" (dstLine) // 0
				, "rm" (col32) // 1
				, "r" (dst->w * sizeof(Pixel)) // 2: bytes per line
				: "eax", "mm0", "mm1", "mm2", "mm3"
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

