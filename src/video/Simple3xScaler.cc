// $Id$

#include "Simple3xScaler.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "RenderSettings.hh"
#include "MemoryOps.hh"
#include "Multiply32.hh"
#include "HostCPU.hh"

namespace openmsx {

template <class Pixel> class Blur_1on3
{
public:
	Blur_1on3(const PixelOperations<Pixel>& pixelOps,
	          const RenderSettings& settings);
	void operator()(const Pixel* in, Pixel* out, unsigned dstWidth);
private:
	Multiply32<Pixel> mult0;
	Multiply32<Pixel> mult1;
	Multiply32<Pixel> mult2;
	Multiply32<Pixel> mult3;
	const RenderSettings& settings;
};


template <class Pixel>
Simple3xScaler<Pixel>::Simple3xScaler(
		const PixelOperations<Pixel>& pixelOps_,
		const RenderSettings& settings_)
	: Scaler3<Pixel>(pixelOps_)
	, pixelOps(pixelOps_)
	, scanline(pixelOps_)
	, blur_1on3(new Blur_1on3<Pixel>(pixelOps_, settings_))
	, settings(settings_)
{
}

template <typename Pixel>
template <typename ScaleOp>
void Simple3xScaler<Pixel>::doScale1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	int scanlineFactor = settings.getScanlineFactor();
	unsigned y = dstStartY;
	Pixel* dummy = 0;
	const Pixel* srcLine = src.getLinePtr(srcStartY++, srcWidth, dummy);
	Pixel* prevDstLine0 = dst.getLinePtr(y + 0, dummy);
	scale(srcLine, prevDstLine0, dst.getWidth());

	Scale_1on1<Pixel> copy;
	Pixel* dstLine1     = dst.getLinePtr(y + 1, dummy);
	copy(prevDstLine0, dstLine1, dst.getWidth());

	for (/* */; (y + 4) < dstEndY; y += 3, srcStartY += 1) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, srcWidth, dummy);
		Pixel* dstLine0 = dst.getLinePtr(y + 3, dummy);
		scale(srcLine, dstLine0, dst.getWidth());

		Pixel* dstLine1 = dst.getLinePtr(y + 4, dummy);
		copy(dstLine0, dstLine1, dst.getWidth());

		Pixel* dstLine2 = dst.getLinePtr(y + 2, dummy);
		scanline.draw(prevDstLine0, dstLine0, dstLine2,
		              scanlineFactor, dst.getWidth());

		prevDstLine0 = dstLine0;
	}
	srcLine = src.getLinePtr(srcStartY, srcWidth, dummy);
	Pixel buf[dst.getWidth()];
	scale(srcLine, buf, dst.getWidth());

	Pixel* dstLine2 = dst.getLinePtr(y + 2, dummy);
	scanline.draw(prevDstLine0, buf, dstLine2,
		      scanlineFactor, dst.getWidth());
}

template <typename Pixel>
template <typename ScaleOp>
void Simple3xScaler<Pixel>::doScale2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	Pixel* dummy = 0;
	int scanlineFactor = settings.getScanlineFactor();
	for (unsigned srcY = srcStartY, dstY = dstStartY; dstY < dstEndY;
	     srcY += 2, dstY += 3) {
		const Pixel* srcLine0 = src.getLinePtr(srcY + 0, srcWidth, dummy);
		Pixel* dstLine0 = dst.getLinePtr(dstY + 0, dummy);
		scale(srcLine0, dstLine0, dst.getWidth());

		const Pixel* srcLine1 = src.getLinePtr(srcY + 1, srcWidth, dummy);
		Pixel* dstLine2 = dst.getLinePtr(dstY + 2, dummy);
		scale(srcLine1, dstLine2, dst.getWidth());

		Pixel* dstLine1 = dst.getLinePtr(dstY + 1, dummy);
		scanline.draw(dstLine0, dstLine2, dstLine1,
		              scanlineFactor, dst.getWidth());
	}
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale2x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_2on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale2x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_2on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (settings.getBlurFactor()) {
		doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
			 *blur_1on3);
	} else {
		doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
			 Scale_1on3<Pixel>());
	}
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale1x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_1on3<Pixel>());
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale4x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_4on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale4x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_4on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale2x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale2x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale8x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_8on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale8x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_8on9<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale4x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale4x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY,
	         Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void Simple3xScaler<Pixel>::scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	int scanlineFactor = settings.getScanlineFactor();

	unsigned stopDstY = (dstEndY == dst.getHeight())
	                  ? dstEndY : dstEndY - 3;
	unsigned srcY = srcStartY, dstY = dstStartY;
	for (/* */; dstY < stopDstY; srcY += 1, dstY += 3) {
		Pixel* dummy = 0;
		Pixel color0 = src.getLinePtr(srcY, dummy)[0];
		Pixel* dstLine0 = dst.getLinePtr(dstY + 0, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine0, dst.getWidth(), color0);
		Pixel* dstLine1 = dst.getLinePtr(dstY + 1, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine1, dst.getWidth(), color0);
		Pixel color1 = scanline.darken(color0, scanlineFactor);
		Pixel* dstLine2 = dst.getLinePtr(dstY + 2, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine2, dst.getWidth(), color1);
	}
	if (dstY != dst.getHeight()) {
		unsigned nextLineWidth = src.getLineWidth(srcY + 1);
		assert(src.getLineWidth(srcY) == 1);
		assert(nextLineWidth != 1);
		this->scaleImage(src, srcY, srcEndY, nextLineWidth,
		                 dst, dstY, dstEndY);
	}
}

template <class Pixel>
void Simple3xScaler<Pixel>::scaleBlank2to3(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	int scanlineFactor = settings.getScanlineFactor();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 3) {
		Pixel* dummy = 0;
		Pixel color0 = src.getLinePtr(srcY + 0, dummy)[0];
		Pixel color1 = src.getLinePtr(srcY + 1, dummy)[0];
		Pixel color01 = scanline.darken(color0, color1, scanlineFactor);
		Pixel* dstLine0 = dst.getLinePtr(dstY + 0, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine0, dst.getWidth(), color0);
		Pixel* dstLine1 = dst.getLinePtr(dstY + 1, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine1, dst.getWidth(), color01);
		Pixel* dstLine2 = dst.getLinePtr(dstY + 2, dummy);
		MemoryOps::memset<Pixel, MemoryOps::STREAMING>(
			dstLine2, dst.getWidth(), color1);
	}
}


// class Blur_1on3

template <class Pixel>
Blur_1on3<Pixel>::Blur_1on3(const PixelOperations<Pixel>& pixelOps,
                            const RenderSettings& settings_) 
	: mult0(pixelOps.format)
	, mult1(pixelOps.format)
	, mult2(pixelOps.format)
	, mult3(pixelOps.format)
	, settings(settings_)
{
}

template <class Pixel>
void Blur_1on3<Pixel>::operator()(const Pixel* in, Pixel* out, unsigned dstWidth)
{
	/* The following code is equivalent to this loop. It is 2x unrolled
	 * and common subexpressions have been eliminated. The last iteration
	 * is also moved outside the for loop.
	 * 
	 *  unsigned c0 = alpha / 2;
	 *  unsigned c1 = c0 + alpha;
	 *  unsigned c2 = 256 - c1;
	 *  unsigned c3 = 256 - 2 * c0;
	 *  Pixel prev, curr, next;
	 *  prev = curr = next = in[0];
	 *  unsigned srcWidth = dstWidth / 3;
	 *  for (unsigned x = 0; x < srcWidth; ++x) {
	 *      if (x != (srcWidth - 1)) next = in[x + 1];
	 *      out[3 * x + 0] = mul(c1, prev) + mul(c2, curr);
	 *      out[3 * x + 1] = mul(c0, prev) + mul(c3, curr) + mul(c0, next);
	 *      out[3 * x + 2] =                 mul(c2, curr) + mul(c1, next);
	 *      prev = curr;
	 *      curr = next;
	 *  }
	 */
	
	unsigned alpha = settings.getBlurFactor() / 3;
	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if ((sizeof(Pixel) == 4) && cpu.hasMMXEXT()) {
		// MMX-EXT routine, 32bpp
		alpha *= 256;
		unsigned t0, t1, t2, t3;
		struct {
			unsigned long long zero;
			unsigned long long c0;
			unsigned long long c1;
			unsigned long long c2;
			unsigned long long c3;
			unsigned c0_;
			unsigned c1_;
			unsigned c2_;
			unsigned c3_;
		} c;
		c.c0_ = alpha / 2;
		c.c1_ = alpha + c.c0_;
		c.c2_ = 0x10000 - c.c1_;
		c.c3_ = 0x10000 - 2 * c.c0_;

		asm volatile (
			"pxor      %%mm0, %%mm0;"
			"pshufw    $0,40(%[CONST]),%%mm1;"
			"pshufw    $0,44(%[CONST]),%%mm2;"
			"pshufw    $0,48(%[CONST]),%%mm3;"
			"pshufw    $0,52(%[CONST]),%%mm4;"
			"movq      %%mm0,   (%[CONST]);"   // zero    store constants
			"movq      %%mm1,  8(%[CONST]);"   // c0
			"movq      %%mm2, 16(%[CONST]);"   // c1
			"movq      %%mm3, 24(%[CONST]);"   // c2
			"movq      %%mm4, 32(%[CONST]);"   // c3

			"movq      (%[IN]), %%mm0;"        // in[0] | in[1]
			"movq      %%mm0, %%mm5;"          // in[0] | in[1]
			"punpcklbw (%[CONST]), %%mm0;"     // p0 = unpack(in[0])
			"movq      %%mm0, %%mm2;"          // p0
			"pmulhuw    8(%[CONST]), %%mm2;"   // f0 = c0 * p0
			"movq      %%mm0, %%mm3;"          // p0
			"pmulhuw   16(%[CONST]), %%mm3;"   // f1 = c1 * p0
			"movq      %%mm2, %%mm4;"          // g0 = f0;
			"movq      %%mm3, %%mm6;"          // g1 = f1;

		"0:"
			// Note:  no streaming stores
			"prefetchnta 192( %[IN]);"         //
			"prefetcht0  320(%[OUT],%[Y]);"    //
			"movq      %%mm5, %%mm1;"          // in[x + 1]
			"movq      %%mm0, %%mm7;"          // p0
			"punpckhbw   (%[CONST]), %%mm1;"   // p1 = unpack(in[x + 1])
			"pmulhuw   32(%[CONST]), %%mm7;"   // s0 = c3 * p0
			"paddw     %%mm2, %%mm7;"          // s0 + f0
			"movq      %%mm1, %%mm2;"          // p1
			"pmulhuw   24(%[CONST]), %%mm0;"   // g2 = c2 * p0
			"pmulhuw    8(%[CONST]), %%mm2;"   // t0 = c0 * p1 (= f0)
			"movq      8(%[IN]), %%mm5;"       // in[x + 2] | in[x + 3]
			"paddw     %%mm0, %%mm3;"          // a0 = g2 + f1
			"paddw     %%mm2, %%mm7;"          // a1 = t0 + s0 + f0
			"packuswb  %%mm7, %%mm3;"          // a0 | a1
			"movq      %%mm3, (%[OUT],%[Y]);"  // out[y + 0] = ...
			"movq      %%mm1, %%mm3;"          // p1
			"movq      %%mm1, %%mm7;"          // p1
			"pmulhuw   16(%[CONST]), %%mm3;"   // f1 = c1 * p1
			"pmulhuw   24(%[CONST]), %%mm7;"   // f2 = c2 * p1
			"paddw     %%mm3, %%mm0;"          // a2 = g2 + f1
			"paddw     %%mm7, %%mm6;"          // b0 = f2 + g1
			"packuswb  %%mm6, %%mm0;"          // a2 | b0
			"movq      %%mm0, 8(%[OUT],%[Y]);" // out[y + 2] = ...
			"movq      %%mm5, %%mm0;"          // in[x + 2]
			"pmulhuw   32(%[CONST]), %%mm1;"   // s1 = c3 * p1
			"punpcklbw   (%[CONST]), %%mm0;"   // p0 = unpack(in[x + 2])
			"paddw     %%mm4, %%mm1;"          // s1 + g0
			"movq      %%mm0, %%mm6;"          // p0
			"movq      %%mm0, %%mm4;"          // p0
			"pmulhuw   16(%[CONST]), %%mm6;"   // g1 = c1 * p0
			"pmulhuw    8(%[CONST]), %%mm4;"   // t1 = c0 * p0 (= g0)
			"paddw     %%mm6, %%mm7;"          // b2 = g1 + f2
			"paddw     %%mm4, %%mm1;"          // b1 = t1 + s1 + g0
			"packuswb  %%mm7, %%mm1;"          // b1 | b2
			"addl      $8, %[IN];"             // x += 2
			"movq      %%mm1, 16(%[OUT],%[Y]);"// out[y + 4] = 
			"addl      $24, %[Y];"             // y += 6
			"jnz       0b;"                    //
			
			"movq      %%mm5, %%mm1;"          // in[x + 1]
			"movq      %%mm0, %%mm7;"          // p0
			"punpckhbw   (%[CONST]), %%mm1;"   // p1 = unpack(in[x + 1])
			"pmulhuw   32(%[CONST]), %%mm7;"   // s0 = c3 * p0
			"paddw     %%mm2, %%mm7;"          // s0 + f0
			"movq      %%mm1, %%mm2;"          // p1
			"pmulhuw   24(%[CONST]), %%mm0;"   // g2 = c2 * p0
			"pmulhuw    8(%[CONST]), %%mm2;"   // t0 = c0 * p1 (= f0)
			"paddw     %%mm0, %%mm3;"          // a0 = g2 + f1
			"paddw     %%mm2, %%mm7;"          // a1 = t0 + s0 + f0
			"packuswb  %%mm7, %%mm3;"          // a0 | a1
			"movq      %%mm3, (%[OUT]);"       // out[y + 0] = ...
			"movq      %%mm1, %%mm3;"          // p1
			"movq      %%mm1, %%mm7;"          // p1
			"pmulhuw   16(%[CONST]), %%mm3;"   // f1 = c1 * p1
			"pmulhuw   24(%[CONST]), %%mm7;"   // f2 = c2 * p1
			"paddw     %%mm3, %%mm0;"          // a2 = g2 + f1
			"paddw     %%mm7, %%mm6;"          // b0 = f2 + g1
			"packuswb  %%mm6, %%mm0;"          // a2 | b0
			"movq      %%mm0, 8(%[OUT]);"      // out[y + 2] = ...
			"movq      %%mm1, %%mm7;"          // p1
			"pmulhuw   32(%[CONST]), %%mm1;"   // s1 = c3 * p1
			"paddw     %%mm4, %%mm1;"          // s1 + g0
			"paddw     %%mm2, %%mm1;"          // b1 = t1 + s1 + g0
			"packuswb  %%mm7, %%mm1;"          // b1 | b2
			"movq      %%mm1, 16(%[OUT]);"     // out[y + 4] = 

			"emms;"

			: "=&r" (t0), "=&r" (t1), "=&r" (t2), "=&r" (t3)
			: [IN]    "0" (in)
			, [OUT]   "1" (out + (dstWidth - 6))
			, [Y]     "2" (-4 * (dstWidth - 6))
			, [CONST] "3" (&c)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}
	#endif

	// non-MMX routine, both 16bpp and 32bpp
	unsigned c0 = alpha / 2;
	unsigned c1 = alpha + c0;
	unsigned c2 = 256 - c1;
	unsigned c3 = 256 - 2 * c0;
	mult0.setFactor32(c0);
	mult1.setFactor32(c1);
	mult2.setFactor32(c2);
	mult3.setFactor32(c3);

	Pixel p0 = in[0];
	Pixel p1;
	unsigned f0 = mult0.mul32(p0);
	unsigned f1 = mult1.mul32(p0);
	unsigned g0 = f0;
	unsigned g1 = f1;

	unsigned x;
	unsigned srcWidth = dstWidth / 3;
	for (x = 0; x < (srcWidth - 2); x += 2) {
		unsigned g2 = mult2.mul32(p0);
		out[3 * x + 0] = mult0.conv32(g2 + f1);
		p1 = in[x + 1];
		unsigned t0 = mult0.mul32(p1);
		out[3 * x + 1] = mult0.conv32(f0 + mult3.mul32(p0) + t0);
		f0 = t0;
		f1 = mult1.mul32(p1);
		out[3 * x + 2] = mult0.conv32(g2 + f1);

		unsigned f2 = mult2.mul32(p1);
		out[3 * x + 3] = mult0.conv32(f2 + g1);
		p0 = in[x + 2];
		unsigned t1 = mult0.mul32(p0);
		out[3 * x + 4] = mult0.conv32(g0 + mult3.mul32(p1) + t1);
		g0 = t1;
		g1 = mult1.mul32(p0);
		out[3 * x + 5] = mult0.conv32(g1 + f2);
	}
	unsigned g2 = mult2.mul32(p0);
	out[3 * x + 0] = mult0.conv32(g2 + f1);
	p1 = in[x + 1];
	unsigned t0 = mult0.mul32(p1);
	out[3 * x + 1] = mult0.conv32(f0 + mult3.mul32(p0) + t0);
	f0 = t0;
	f1 = mult1.mul32(p1);
	out[3 * x + 2] = mult0.conv32(g2 + f1);

	unsigned f2 = mult2.mul32(p1);
	out[3 * x + 3] = mult0.conv32(f2 + g1);
	out[3 * x + 4] = mult0.conv32(g0 + mult3.mul32(p1) + f0);
	out[3 * x + 5] = p1;
}

// Force template instantiation.
template class Simple3xScaler<word>;
template class Simple3xScaler<unsigned>;

} // namespace openmsx
