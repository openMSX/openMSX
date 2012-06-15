// $Id$

#include "Simple3xScaler.hh"
#include "SuperImposedFrame.hh"
#include "LineScalers.hh"
#include "RawFrame.hh"
#include "ScalerOutput.hh"
#include "RenderSettings.hh"
#include "Multiply32.hh"
#include "HostCPU.hh"
#include "vla.hh"
#include "build-info.hh"

namespace openmsx {

template <class Pixel> class Blur_1on3
{
public:
	Blur_1on3(const PixelOperations<Pixel>& pixelOps);
	void setBlur(unsigned blur_) { blur = blur_; }
	void operator()(const Pixel* in, Pixel* out, unsigned long dstWidth);
private:
	Multiply32<Pixel> mult0;
	Multiply32<Pixel> mult1;
	Multiply32<Pixel> mult2;
	Multiply32<Pixel> mult3;
	unsigned blur;
};


template <class Pixel>
Simple3xScaler<Pixel>::Simple3xScaler(
		const PixelOperations<Pixel>& pixelOps_,
		const RenderSettings& settings_)
	: Scaler3<Pixel>(pixelOps_)
	, pixelOps(pixelOps_)
	, scanline(pixelOps_)
	, blur_1on3(new Blur_1on3<Pixel>(pixelOps_))
	, settings(settings_)
{
}

template <class Pixel>
Simple3xScaler<Pixel>::~Simple3xScaler()
{
}

template <typename Pixel>
void Simple3xScaler<Pixel>::doScale1(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PolyLineScaler<Pixel>& scale)
{
	int scanlineFactor = settings.getScanlineFactor();
	unsigned dstWidth = dst.getWidth();
	unsigned y = dstStartY;
	const Pixel* srcLine = src.getLinePtr<Pixel>(srcStartY++, srcWidth);
	Pixel* dstLine0 = dst.acquireLine(y + 0);
	scale(srcLine, dstLine0, dstWidth);

	Scale_1on1<Pixel> copy;
	Pixel* dstLine1 = dst.acquireLine(y + 1);
	copy(dstLine0, dstLine1, dstWidth);

	for (/* */; (y + 4) < dstEndY; y += 3, srcStartY += 1) {
		srcLine = src.getLinePtr<Pixel>(srcStartY, srcWidth);
		Pixel* dstLine3 = dst.acquireLine(y + 3);
		scale(srcLine, dstLine3, dstWidth);

		Pixel* dstLine4 = dst.acquireLine(y + 4);
		copy(dstLine3, dstLine4, dstWidth);

		Pixel* dstLine2 = dst.acquireLine(y + 2);
		scanline.draw(dstLine0, dstLine3, dstLine2,
		              scanlineFactor, dstWidth);

		dst.releaseLine(y + 0, dstLine0);
		dst.releaseLine(y + 1, dstLine1);
		dst.releaseLine(y + 2, dstLine2);
		dstLine0 = dstLine3;
		dstLine1 = dstLine4;
	}
	srcLine = src.getLinePtr<Pixel>(srcStartY, srcWidth);
	VLA_ALIGNED(Pixel, buf, dstWidth, 16);
	scale(srcLine, buf, dstWidth);

	Pixel* dstLine2 = dst.acquireLine(y + 2);
	scanline.draw(dstLine0, buf, dstLine2, scanlineFactor, dstWidth);
	dst.releaseLine(y + 0, dstLine0);
	dst.releaseLine(y + 1, dstLine1);
	dst.releaseLine(y + 2, dstLine2);
}

template <typename Pixel>
void Simple3xScaler<Pixel>::doScale2(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY,
	PolyLineScaler<Pixel>& scale)
{
	int scanlineFactor = settings.getScanlineFactor();
	unsigned dstWidth = dst.getWidth();
	for (unsigned srcY = srcStartY, dstY = dstStartY; dstY < dstEndY;
	     srcY += 2, dstY += 3) {
		const Pixel* srcLine0 = src.getLinePtr<Pixel>(srcY + 0, srcWidth);
		Pixel* dstLine0 = dst.acquireLine(dstY + 0);
		scale(srcLine0, dstLine0, dstWidth);

		const Pixel* srcLine1 = src.getLinePtr<Pixel>(srcY + 1, srcWidth);
		Pixel* dstLine2 = dst.acquireLine(dstY + 2);
		scale(srcLine1, dstLine2, dstWidth);

		Pixel* dstLine1 = dst.acquireLine(dstY + 1);
		scanline.draw(dstLine0, dstLine2, dstLine1,
		              scanlineFactor, dstWidth);

		dst.releaseLine(dstY + 0, dstLine0);
		dst.releaseLine(dstY + 1, dstLine1);
		dst.releaseLine(dstY + 2, dstLine2);
	}
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale2x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on9<Pixel> > op(pixelOps);
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale2x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on9<Pixel> > op(pixelOps);
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale1x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (unsigned blur = settings.getBlurFactor() / 3) {
		blur_1on3->setBlur(blur);
		PolyScaleRef<Pixel, Blur_1on3<Pixel> > op(*blur_1on3);
		doScale1(src, srcStartY, srcEndY, srcWidth,
		         dst, dstStartY, dstEndY, op);
	} else {
		// No blurring: this is an optimization but it's also needed
		// for correctness (otherwise there's an overflow in 0.16 fixed
		// point arithmetic).
		PolyScale<Pixel, Scale_1on3<Pixel> > op;
		doScale1(src, srcStartY, srcEndY, srcWidth,
		         dst, dstStartY, dstEndY, op);
	}
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale1x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on3<Pixel> > op;
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale4x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on9<Pixel> > op(pixelOps);
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale4x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on9<Pixel> > op(pixelOps);
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale2x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel> > op(pixelOps);
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale2x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel> > op(pixelOps);
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale8x1to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on9<Pixel> > op(pixelOps);
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale8x2to9x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on9<Pixel> > op(pixelOps);
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale4x1to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel> > op(pixelOps);
	doScale1(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Simple3xScaler<Pixel>::scale4x2to3x3(FrameSource& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel> > op(pixelOps);
	doScale2(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY, op);
}

template <class Pixel>
void Simple3xScaler<Pixel>::scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	int scanlineFactor = settings.getScanlineFactor();

	unsigned dstHeight = dst.getHeight();
	unsigned stopDstY = (dstEndY == dstHeight)
	                  ? dstEndY : dstEndY - 3;
	unsigned srcY = srcStartY, dstY = dstStartY;
	for (/* */; dstY < stopDstY; srcY += 1, dstY += 3) {
		Pixel color0 = src.getLinePtr<Pixel>(srcY)[0];
		Pixel color1 = scanline.darken(color0, scanlineFactor);
		dst.fillLine(dstY + 0, color0);
		dst.fillLine(dstY + 1, color0);
		dst.fillLine(dstY + 2, color1);
	}
	if (dstY != dstHeight) {
		unsigned nextLineWidth = src.getLineWidth(srcY + 1);
		assert(src.getLineWidth(srcY) == 1);
		assert(nextLineWidth != 1);
		this->dispatchScale(src, srcY, srcEndY, nextLineWidth,
		                    dst, dstY, dstEndY);
	}
}

template <class Pixel>
void Simple3xScaler<Pixel>::scaleBlank2to3(
		FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	int scanlineFactor = settings.getScanlineFactor();
	for (unsigned srcY = srcStartY, dstY = dstStartY;
	     dstY < dstEndY; srcY += 2, dstY += 3) {
		Pixel color0 = src.getLinePtr<Pixel>(srcY + 0)[0];
		Pixel color1 = src.getLinePtr<Pixel>(srcY + 1)[0];
		Pixel color01 = scanline.darken(color0, color1, scanlineFactor);
		dst.fillLine(dstY + 0, color0);
		dst.fillLine(dstY + 1, color01);
		dst.fillLine(dstY + 2, color1);
	}
}


// class Blur_1on3

// Assembly functions
#ifdef _MSC_VER
extern "C"
{
	void __cdecl Blur_1on3_4_SSE(const void* in, void* out, unsigned dstWidth, void* c);
}
#endif

template <class Pixel>
Blur_1on3<Pixel>::Blur_1on3(const PixelOperations<Pixel>& pixelOps)
	: mult0(pixelOps)
	, mult1(pixelOps)
	, mult2(pixelOps)
	, mult3(pixelOps)
{
}

template <class Pixel>
void Blur_1on3<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out,
	unsigned long dstWidth)
{
	/* The following code is equivalent to this loop. It is 2x unrolled
	 * and common subexpressions have been eliminated. The last iteration
	 * is also moved outside the for loop.
	 *
	 *  unsigned c0 = blur / 2;
	 *  unsigned c1 = c0 + blur;
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
	#if ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if ((sizeof(Pixel) == 4) && cpu.hasSSE()) {
		// MMX-EXT routine, 32bpp
		unsigned long alpha = blur * 256;
		struct {
			unsigned long long zero; //  0
			unsigned long long c0;   //  8
			unsigned long long c1;   // 16
			unsigned long long c2;   // 24
			unsigned long long c3;   // 32
			unsigned c0_;            // 40
			unsigned c1_;            // 44
			unsigned c2_;            // 48
			unsigned c3_;            // 52
		} c;
		c.c0_ = alpha / 2;
		c.c1_ = alpha + c.c0_;
		c.c2_ = 0x10000 - c.c1_;
		c.c3_ = 0x10000 - 2 * c.c0_;
	#ifdef _MSC_VER
		Blur_1on3_4_SSE(in, out, dstWidth, &c);
		return;
	#else
		void *t0, *t1, *t3;
		long t2;
		asm volatile (
			"pxor      %%mm0, %%mm0;"
			"pshufw    $0,40(%[CNST]),%%mm1;"
			"pshufw    $0,44(%[CNST]),%%mm2;"
			"pshufw    $0,48(%[CNST]),%%mm3;"
			"pshufw    $0,52(%[CNST]),%%mm4;"
			"movq      %%mm0,   (%[CNST]);"    // zero    store constants
			"movq      %%mm1,  8(%[CNST]);"    // c0
			"movq      %%mm2, 16(%[CNST]);"    // c1
			"movq      %%mm3, 24(%[CNST]);"    // c2
			"movq      %%mm4, 32(%[CNST]);"    // c3

			"movq      (%[IN]), %%mm0;"        // in[0] | in[1]
			"movq      %%mm0, %%mm5;"          // in[0] | in[1]
			"punpcklbw (%[CNST]), %%mm0;"      // p0 = unpack(in[0])
			"movq      %%mm0, %%mm2;"          // p0
			"pmulhuw    8(%[CNST]), %%mm2;"    // f0 = c0 * p0
			"movq      %%mm0, %%mm3;"          // p0
			"pmulhuw   16(%[CNST]), %%mm3;"    // f1 = c1 * p0
			"movq      %%mm2, %%mm4;"          // g0 = f0;
			"movq      %%mm3, %%mm6;"          // g1 = f1;

		"0:"
			// Note:  no streaming stores
			"prefetchnta 192( %[IN]);"         //
			"prefetcht0  320(%[OUT],%[Y]);"    //
			"movq      %%mm5, %%mm1;"          // in[x + 1]
			"movq      %%mm0, %%mm7;"          // p0
			"punpckhbw   (%[CNST]), %%mm1;"    // p1 = unpack(in[x + 1])
			"pmulhuw   32(%[CNST]), %%mm7;"    // s0 = c3 * p0
			"paddw     %%mm2, %%mm7;"          // s0 + f0
			"movq      %%mm1, %%mm2;"          // p1
			"pmulhuw   24(%[CNST]), %%mm0;"    // g2 = c2 * p0
			"pmulhuw    8(%[CNST]), %%mm2;"    // t0 = c0 * p1 (= f0)
			"movq      8(%[IN]), %%mm5;"       // in[x + 2] | in[x + 3]
			"paddw     %%mm0, %%mm3;"          // a0 = g2 + f1
			"paddw     %%mm2, %%mm7;"          // a1 = t0 + s0 + f0
			"packuswb  %%mm7, %%mm3;"          // a0 | a1
			"movq      %%mm3, (%[OUT],%[Y]);"  // out[y + 0] = ...
			"movq      %%mm1, %%mm3;"          // p1
			"movq      %%mm1, %%mm7;"          // p1
			"pmulhuw   16(%[CNST]), %%mm3;"    // f1 = c1 * p1
			"pmulhuw   24(%[CNST]), %%mm7;"    // f2 = c2 * p1
			"paddw     %%mm3, %%mm0;"          // a2 = g2 + f1
			"paddw     %%mm7, %%mm6;"          // b0 = f2 + g1
			"packuswb  %%mm6, %%mm0;"          // a2 | b0
			"movq      %%mm0, 8(%[OUT],%[Y]);" // out[y + 2] = ...
			"movq      %%mm5, %%mm0;"          // in[x + 2]
			"pmulhuw   32(%[CNST]), %%mm1;"    // s1 = c3 * p1
			"punpcklbw   (%[CNST]), %%mm0;"    // p0 = unpack(in[x + 2])
			"paddw     %%mm4, %%mm1;"          // s1 + g0
			"movq      %%mm0, %%mm6;"          // p0
			"movq      %%mm0, %%mm4;"          // p0
			"pmulhuw   16(%[CNST]), %%mm6;"    // g1 = c1 * p0
			"pmulhuw    8(%[CNST]), %%mm4;"    // t1 = c0 * p0 (= g0)
			"paddw     %%mm6, %%mm7;"          // b2 = g1 + f2
			"paddw     %%mm4, %%mm1;"          // b1 = t1 + s1 + g0
			"packuswb  %%mm7, %%mm1;"          // b1 | b2
			"add       $8, %[IN];"             // x += 2
			"movq      %%mm1, 16(%[OUT],%[Y]);"// out[y + 4] =
			"add       $24, %[Y];"             // y += 6
			"jnz       0b;"                    //

			"movq      %%mm5, %%mm1;"          // in[x + 1]
			"movq      %%mm0, %%mm7;"          // p0
			"punpckhbw   (%[CNST]), %%mm1;"    // p1 = unpack(in[x + 1])
			"pmulhuw   32(%[CNST]), %%mm7;"    // s0 = c3 * p0
			"paddw     %%mm2, %%mm7;"          // s0 + f0
			"movq      %%mm1, %%mm2;"          // p1
			"pmulhuw   24(%[CNST]), %%mm0;"    // g2 = c2 * p0
			"pmulhuw    8(%[CNST]), %%mm2;"    // t0 = c0 * p1 (= f0)
			"paddw     %%mm0, %%mm3;"          // a0 = g2 + f1
			"paddw     %%mm2, %%mm7;"          // a1 = t0 + s0 + f0
			"packuswb  %%mm7, %%mm3;"          // a0 | a1
			"movq      %%mm3, (%[OUT]);"       // out[y + 0] = ...
			"movq      %%mm1, %%mm3;"          // p1
			"movq      %%mm1, %%mm7;"          // p1
			"pmulhuw   16(%[CNST]), %%mm3;"    // f1 = c1 * p1
			"pmulhuw   24(%[CNST]), %%mm7;"    // f2 = c2 * p1
			"paddw     %%mm3, %%mm0;"          // a2 = g2 + f1
			"paddw     %%mm7, %%mm6;"          // b0 = f2 + g1
			"packuswb  %%mm6, %%mm0;"          // a2 | b0
			"movq      %%mm0, 8(%[OUT]);"      // out[y + 2] = ...
			"movq      %%mm1, %%mm7;"          // p1
			"pmulhuw   32(%[CNST]), %%mm1;"    // s1 = c3 * p1
			"paddw     %%mm4, %%mm1;"          // s1 + g0
			"paddw     %%mm2, %%mm1;"          // b1 = t1 + s1 + g0
			"packuswb  %%mm7, %%mm1;"          // b1 | b2
			"movq      %%mm1, 16(%[OUT]);"     // out[y + 4] =

			"emms;"

			: [IN]   "=r" (t0)
			, [OUT]  "=r" (t1)
			, [Y]    "=r" (t2)
			, [CNST] "=r" (t3)
			// The typecasts are required to avoid a Clang bug.
			//   http://llvm.org/bugs/show_bug.cgi?id=9671
			:        "[IN]"   (in)
			,        "[OUT]"  (static_cast<void *>(out + (dstWidth - 6)))
			,        "[Y]"    (-4 * (dstWidth - 6))
			,        "[CNST]" (static_cast<void *>(&c))
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	#endif
	}
	#endif

	// non-MMX routine, both 16bpp and 32bpp
	unsigned c0 = blur / 2;
	unsigned c1 = blur + c0;
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

template <class Pixel>
void Simple3xScaler<Pixel>::scaleImage(
	FrameSource& src, const RawFrame* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	if (superImpose) {
		SuperImposedFrame<Pixel> sf(src, *superImpose, pixelOps);
		srcWidth = sf.getLineWidth(srcStartY);
		this->dispatchScale(sf,  srcStartY, srcEndY, srcWidth,
		                    dst, dstStartY, dstEndY);
		src.freeLineBuffers();
		superImpose->freeLineBuffers();
	} else {
		this->dispatchScale(src, srcStartY, srcEndY, srcWidth,
		                    dst, dstStartY, dstEndY);
	}
}

// Force template instantiation.
#if HAVE_16BPP
template class Simple3xScaler<word>;
#endif
#if HAVE_32BPP
template class Simple3xScaler<unsigned>;
#endif

} // namespace openmsx
