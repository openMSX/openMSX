// $Id$

#include "Scaler.hh"
#include "SimpleScaler.hh"
#include "SaI2xScaler.hh"
#include "Scale2xScaler.hh"
#include "HQ2xScaler.hh"
#include "HQ3xScaler.hh"
#include "HQ2xLiteScaler.hh"
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
	case SCALER_LOW:
		return auto_ptr<Scaler<Pixel> >(
			new LowScaler<Pixel>(format));
	default:
		assert(false);
		return auto_ptr<Scaler<Pixel> >();
	}
}

template <class Pixel>
Scaler<Pixel>::Scaler(SDL_PixelFormat* format_)
	: format(*format_) // make a copy for faster access
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
inline unsigned Scaler<Pixel>::red(Pixel pix) {
	return (pix & format.Rmask) >> format.Rshift;
}

template <class Pixel>
inline unsigned Scaler<Pixel>::green(Pixel pix) {
	return (pix & format.Gmask) >> format.Gshift;
}

template <class Pixel>
inline unsigned Scaler<Pixel>::blue(Pixel pix) {
	return (pix & format.Bmask) >> format.Bshift;
}

template <class Pixel>
inline Pixel Scaler<Pixel>::combine(unsigned r, unsigned g, unsigned b)
{
	return (Pixel)(((r << format.Rshift) & format.Rmask) |
	               ((g << format.Gshift) & format.Gmask) |
	               ((b << format.Bshift) & format.Bmask));
}

template <class Pixel>
template <unsigned w1, unsigned w2>
inline Pixel Scaler<Pixel>::blendPixels2(const Pixel* source)
{
	unsigned total = w1 + w2;
	unsigned r = (red  (source[0]) * w1 + red  (source[1]) * w2) / total;
	unsigned g = (green(source[0]) * w1 + green(source[1]) * w2) / total;
	unsigned b = (blue (source[0]) * w1 + blue (source[1]) * w2) / total;
	return combine(r, g, b);
}

template <class Pixel>
template <unsigned w1, unsigned w2, unsigned w3>
inline Pixel Scaler<Pixel>::blendPixels3(const Pixel* source)
{
	unsigned total = w1 + w2 + w3;
	unsigned r = (red  (source[0]) * w1 +
	              red  (source[1]) * w2 +
	              red  (source[2]) * w3) / total;
	unsigned g = (green(source[0]) * w1 +
	              green(source[1]) * w2 +
	              green(source[2]) * w3) / total;
	unsigned b = (blue (source[0]) * w1 +
	              blue (source[1]) * w2 +
	              blue (source[2]) * w3) / total;
	return combine(r, g, b);
}

template <class Pixel>
template <unsigned w1, unsigned w2, unsigned w3, unsigned w4>
inline Pixel Scaler<Pixel>::blendPixels4(const Pixel* source)
{
	unsigned total = w1 + w2 + w3 + w4;
	unsigned r = (red  (source[0]) * w1 +
	              red  (source[1]) * w2 +
	              red  (source[2]) * w3 +
	              red  (source[3]) * w4) / total;
	unsigned g = (green(source[0]) * w1 +
	              green(source[1]) * w2 +
	              green(source[2]) * w3 +
	              green(source[3]) * w4) / total;
	unsigned b = (blue (source[0]) * w1 +
	              blue (source[1]) * w2 +
	              blue (source[2]) * w3 +
	              blue (source[3]) * w4) / total;
	return combine(r, g, b);
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
	for (int p = 0; p < nrPixels; p += 2) {
		*outPixels++ = blendPixels2<1, 1>(&inPixels[p]);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale_4on1(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		*outPixels++ = blendPixels4<1, 1, 1, 1>(&inPixels[p]);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale_2on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 2) {
		*outPixels++ =                     inPixels[p + 0];
		*outPixels++ = blendPixels2<1, 1>(&inPixels[p + 0]);
		*outPixels++ =                     inPixels[p + 1];
	}
}

template <class Pixel>
void Scaler<Pixel>::scale_4on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 4) {
		*outPixels++ = blendPixels2<3, 1>(&inPixels[p + 0]);
		*outPixels++ = blendPixels2<1, 1>(&inPixels[p + 1]);
		*outPixels++ = blendPixels2<1, 3>(&inPixels[p + 2]);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale_8on3(
	const Pixel* inPixels, Pixel* outPixels, int nrPixels)
{
	for (int p = 0; p < nrPixels; p += 8) {
		*outPixels++ = blendPixels3<3, 3, 2>   (&inPixels[p + 0]);
		*outPixels++ = blendPixels4<1, 3, 3, 1>(&inPixels[p + 2]);
		*outPixels++ = blendPixels3<2, 3, 3>   (&inPixels[p + 5]);
	}
}


template <class Pixel>
void Scaler<Pixel>::scaleBlank(Pixel color, SDL_Surface* dst,
                               unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	y2 = std::min(480u, y2);
	for (unsigned y = y1; y < y2; ++y) {
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		fillLine(dstLine, color, dst->w);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale256(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 2, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		scaleLine(srcLine, dstLine1, 320);
		if (y == (480 - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		scaleLine(srcLine, dstLine2, 320);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale256(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		scaleLine(srcLine0, dstLine0, 320);

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		scaleLine(srcLine1, dstLine1, 320);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale512(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 2, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		copyLine(srcLine, dstLine1, 640);
		if (y == (480 - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(srcLine, dstLine2, 640);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale512(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		copyLine(srcLine0, dstLine0, 640);

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		copyLine(srcLine1, dstLine1, 640);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale192(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 2, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_1on3(srcLine, dstLine1, 213); // TODO
		if (y == (480 - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine1, dstLine2, 640);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale192(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		scale_1on3(srcLine0, dstLine0, 213); // TODO

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		scale_1on3(srcLine1, dstLine1, 213); // TODO
	}
}

template <class Pixel>
void Scaler<Pixel>::scale384(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 2, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_2on3(srcLine, dstLine1, 426); // TODO
		if (y == (480 - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine1, dstLine2, 640);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale384(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		scale_2on3(srcLine0, dstLine0, 426); // TODO

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		scale_2on3(srcLine1, dstLine1, 426); // TODO
	}
}

template <class Pixel>
void Scaler<Pixel>::scale640(FrameSource& /*src*/, SDL_Surface* /*dst*/,
                             unsigned /*startY*/, unsigned /*endY*/, bool /*lower*/)
{
	// TODO
}

template <class Pixel>
void Scaler<Pixel>::scale640(FrameSource& /*src0*/, FrameSource& /*src1*/, SDL_Surface* /*dst*/,
                             unsigned /*startY*/, unsigned /*endY*/)
{
	// TODO
}

template <class Pixel>
void Scaler<Pixel>::scale768(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 2, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_4on3(srcLine, dstLine1, 853); // TODO
		if (y == (480 - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine1, dstLine2, 640);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale768(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		scale_4on3(srcLine0, dstLine0, 853); // TODO

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		scale_4on3(srcLine1, dstLine1, 853); // TODO
	}
}

template <class Pixel>
void Scaler<Pixel>::scale1024(FrameSource& src, SDL_Surface* dst,
                              unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 2, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_2on1(srcLine, dstLine1, 1280);
		if (y == (480 - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine1, dstLine2, 640);
	}
}

template <class Pixel>
void Scaler<Pixel>::scale1024(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                              unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		scale_2on1(srcLine0, dstLine0, 1280); // TODO

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		scale_2on1(srcLine1, dstLine1, 1280); // TODO
	}
}


// Force template instantiation.
template class Scaler<word>;
template class Scaler<unsigned>;

} // namespace openmsx

