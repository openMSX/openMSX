// $Id$

#ifndef LINESCALERS_HH
#define LINESCALERS_HH

#include "PixelOperations.hh"
#include "HostCPU.hh"
#include "build-info.hh"
#include <cstring>
#include <cassert>

namespace openmsx {

// Meta programming infrastructure for tagging

template <typename T> struct Tag : public T {};
template <typename T> struct NoTag          {};

template <bool, typename T> struct TagIf           { typedef Tag  <T> type; };
template <      typename T> struct TagIf<false, T> { typedef NoTag<T> type; };

template <typename T> struct IsTaggedHelper {
	static char test(T*);
	static int  test(...);
};
template <typename S, typename T> struct IsTagged {
	enum { result = sizeof(IsTaggedHelper<T>::test(static_cast<S*>(0))) == 1 };
};

// Tag classes

struct Streaming {};
struct X86Streaming
#if ASM_X86
: public Streaming
#endif
{};
struct Copy {};

// Scalers

/**  Scale_XonY functors
 * Transforms an input line of pixel to an output line (possibly) with
 * a different width. X input pixels are mapped on Y output pixels.
 * @param in Input line
 * @param out Output line
 * @param width Width of the output line in pixels
 */
template <typename Pixel> class Scale_1on3
{
public:
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
};

template <typename Pixel> class Scale_1on4
{
public:
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
};

template <typename Pixel> class Scale_1on6
{
public:
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
};

template <typename Pixel, bool streaming = true> class Scale_1on2
	: public TagIf<streaming, X86Streaming>::type
{
public:
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
};

template <typename Pixel, bool streaming = true> class Scale_1on1
	: public TagIf<streaming, X86Streaming>::type, public Tag<Copy>
{
public:
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
};

template <typename Pixel> class Scale_2on1 : public Tag<X86Streaming>
{
public:
	explicit Scale_2on1(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_6on1
{
public:
	explicit Scale_6on1(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_4on1
{
public:
	explicit Scale_4on1(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_3on1
{
public:
	explicit Scale_3on1(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_3on2
{
public:
	explicit Scale_3on2(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_3on4
{
public:
	explicit Scale_3on4(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_3on8
{
public:
	explicit Scale_3on8(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_2on3
{
public:
	explicit Scale_2on3(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_4on3
{
public:
	explicit Scale_4on3(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_8on3
{
public:
	explicit Scale_8on3(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_2on9
{
public:
	Scale_2on9(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_4on9
{
public:
	explicit Scale_4on9(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_8on9
{
public:
	explicit Scale_8on9(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_4on5
{
public:
	explicit Scale_4on5(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_7on8
{
public:
	explicit Scale_7on8(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_17on20
{
public:
	explicit Scale_17on20(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};

template <typename Pixel> class Scale_9on10
{
public:
	explicit Scale_9on10(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in, Pixel* out, unsigned long width);
private:
	PixelOperations<Pixel> pixelOps;
};


/**  BlendLines functor
 * Generate an output line that is an iterpolation of two input lines.
 * @param in1 First input line
 * @param in2 Second input line
 * @param out Output line
 * @param width Width of the lines in pixels
 */
template <typename Pixel, unsigned w1 = 1, unsigned w2 = 1> class BlendLines
{
public:
	explicit BlendLines(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in1, const Pixel* in2,
	                Pixel* out, unsigned width);
private:
	PixelOperations<Pixel> pixelOps;
};

/** Stretch (or zoom) a given input line to a wider output line.
 */
template<typename Pixel>
class ZoomLine
{
public:
	explicit ZoomLine(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in,  unsigned inWidth,
	                      Pixel* out, unsigned outWidth) const;
private:
	PixelOperations<Pixel> pixelOps;
};


/**  AlphaBlendLines functor
 * Generate an output line that is a per-pixel-alpha-blend of the two input
 * lines. The first input line contains the alpha-value per pixel.
 * @param in1 First input line
 * @param in2 Second input line
 * @param out Output line
 * @param width Width of the lines in pixels
 */
template <typename Pixel> class AlphaBlendLines
{
public:
	explicit AlphaBlendLines(PixelOperations<Pixel> pixelOps);
	void operator()(const Pixel* in1, const Pixel* in2,
	                Pixel* out, unsigned width);
	void operator()(Pixel in1, const Pixel* in2,
	                Pixel* out, unsigned width);
private:
	PixelOperations<Pixel> pixelOps;
};


/** Polymorphic line scaler.
 * Abstract base class for line scalers. Can be used when one basic algorithm
 * should work in combination with multiple line scalers (e.g. several
 * Scale_XonY variants).
 * A line scaler takes one line of input pixels and outputs a different line
 * of pixels. The input and output don't necessarily have the same number of
 * pixels.
 * An alternative (which we used in the past) is to templatize that algorithm
 * on the LineScaler type. In theory this results in a faster routine, but in
 * practice this performance benefit is often not measurable while it does
 * result in bigger code size.
 */
template<typename Pixel>
class PolyLineScaler
{
public:
	/** Actually scale a line.
	 * @param in Pointer to buffer containing input line.
	 * @param out Pointer to buffer that should be filled with output.
	 * @param outWidth The number of pixels that should be output.
	 * Note: The number of input pixels is not explicitly specified. This
	 *       depends on the actual scaler, for example Scale_2on1 requires
	 *       twice as many pixels in the input than the output.
	 */
	virtual void operator()(const Pixel* in, Pixel* out, unsigned long outWidth) = 0;

	/** Is this scale operation actually a copy?
	 * This info can be used to (in a multi-step scale operation) immediately
	 * produce the output of the previous step in this step's output buffer,
	 * so effectively skipping this step.
	 */
	virtual bool isCopy() const = 0;

	/** Does this scaler use streaming stores?
	 * This means that if you need the output of this scaler more than once
	 * (e.g. in two consecutive output lines) it's most likely cheaper to
	 * run this scaler multiple times compared to copying the output of one
	 * run to multiple locations.
	 */
	virtual bool isStreaming() const = 0;

protected:
	virtual ~PolyLineScaler() {}
};

/** Polymorphic wrapper around another line scaler.
 * This version directly contains (and thus constructs) the wrapped Line Scaler.
 */
template<typename Pixel, typename Scaler>
class PolyScale : public PolyLineScaler<Pixel>
{
public:
	PolyScale()
		: scaler()
	{
	}
	explicit PolyScale(PixelOperations<Pixel> pixelOps)
		: scaler(pixelOps)
	{
	}
	virtual void operator()(const Pixel* in, Pixel* out, unsigned long outWidth)
	{
		scaler(in, out, outWidth);
	}
	virtual bool isCopy() const
	{
		return IsTagged<Scaler, Copy>::result;
	}
	virtual bool isStreaming() const
	{
		return IsTagged<Scaler, Streaming>::result;
	}
private:
	Scaler scaler;
};

/** Like PolyScale above, but instead keeps a reference to the actual scaler.
 * Can be used when the actual scaler is expensive to construct (e.g. Blur_1on3).
 */
template<typename Pixel, typename Scaler>
class PolyScaleRef : public PolyLineScaler<Pixel>
{
public:
	explicit PolyScaleRef(Scaler& scaler_)
		: scaler(scaler_)
	{
	}
	virtual void operator()(const Pixel* in, Pixel* out, unsigned long outWidth)
	{
		scaler(in, out, outWidth);
	}
	virtual bool isCopy() const
	{
		return IsTagged<Scaler, Copy>::result;
	}
	virtual bool isStreaming() const
	{
		return IsTagged<Scaler, Streaming>::result;
	}
private:
	Scaler& scaler;
};


// implementation

// Assembly functions
#ifdef _MSC_VER
extern "C"
{
	void __cdecl Scale_1on2_4_MMX(const void* in, void* out, unsigned long width);
	void __cdecl Scale_1on1_SSE(const void* in, void* out, unsigned long nBytes);
	void __cdecl Scale_2on1_SSE(const void* in, void* out, unsigned long width);
}
#endif

template <typename Pixel, unsigned N>
static inline void scale_1onN(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
{
	unsigned i = 0, j = 0;
	for (/* */; i < (width - (N - 1)); i += N, j += 1) {
		Pixel pix = in[j];
		for (unsigned k = 0; k < N; ++k) {
			out[i + k] = pix;
		}
	}
	for (unsigned k = 0; k < (N - 1); ++k) {
		if ((i + k) < width) out[i + k] = 0;
	}
}

template <typename Pixel>
void Scale_1on3<Pixel>::operator()(const Pixel* in, Pixel* out, unsigned long width)
{
	scale_1onN<Pixel, 3>(in, out, width);
}

template <typename Pixel>
void Scale_1on4<Pixel>::operator()(const Pixel* in, Pixel* out, unsigned long width)
{
	scale_1onN<Pixel, 4>(in, out, width);
}

template <typename Pixel>
void Scale_1on6<Pixel>::operator()(const Pixel* in, Pixel* out, unsigned long width)
{
	scale_1onN<Pixel, 6>(in, out, width);
}


template <typename Pixel, bool streaming>
void Scale_1on2<Pixel, streaming>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned long width2 = 0;

	#if ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();

	#ifdef _MSC_VER

	if ((sizeof(Pixel) == 4) && cpu.hasMMX()) {
		// MMX routine 32bpp
		width2 = width & ~15;
		assert(((2 * width2) % 32) == 0);
		Scale_1on2_4_MMX(in, out, width2);
	}

	#else

	if ((sizeof(Pixel) == 2) && streaming && cpu.hasSSE()) {
		// extended-MMX routine 16bpp
		width2 = width & ~31;
		assert((width2 % 32) == 0);
		unsigned long dummy;
		asm volatile (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%[IN],%[CNT]), %%mm0;"
			"movq	 8(%[IN],%[CNT]), %%mm2;"
			"movq	16(%[IN],%[CNT]), %%mm4;"
			"movq	24(%[IN],%[CNT]), %%mm6;"
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
			"movntq	%%mm0,   (%[OUT],%[CNT],2);"
			"movntq	%%mm1,  8(%[OUT],%[CNT],2);"
			"movntq	%%mm2, 16(%[OUT],%[CNT],2);"
			"movntq	%%mm3, 24(%[OUT],%[CNT],2);"
			"movntq	%%mm4, 32(%[OUT],%[CNT],2);"
			"movntq	%%mm5, 40(%[OUT],%[CNT],2);"
			"movntq	%%mm6, 48(%[OUT],%[CNT],2);"
			"movntq	%%mm7, 56(%[OUT],%[CNT],2);"
			// Increment.
			"add	$32, %[CNT];"
			"jnz	0b;"
			"emms;"

			: [CNT] "=r"    (dummy)
			: [IN]  "r"     (in  + width2 / 2)
			, [OUT] "r"     (out + width2)
			,       "[CNT]" (-width2)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);

	} else if ((sizeof(Pixel) == 2) && cpu.hasMMX()) {
		// MMX routine 16bpp
		width2 = width & ~31;
		assert((width2 % 32) == 0);
		unsigned long dummy;
		asm volatile (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%[IN],%[CNT]), %%mm0;"
			"movq	 8(%[IN],%[CNT]), %%mm2;"
			"movq	16(%[IN],%[CNT]), %%mm4;"
			"movq	24(%[IN],%[CNT]), %%mm6;"
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
			"movq	%%mm0,   (%[OUT],%[CNT],2);"
			"movq	%%mm1,  8(%[OUT],%[CNT],2);"
			"movq	%%mm2, 16(%[OUT],%[CNT],2);"
			"movq	%%mm3, 24(%[OUT],%[CNT],2);"
			"movq	%%mm4, 32(%[OUT],%[CNT],2);"
			"movq	%%mm5, 40(%[OUT],%[CNT],2);"
			"movq	%%mm6, 48(%[OUT],%[CNT],2);"
			"movq	%%mm7, 56(%[OUT],%[CNT],2);"
			// Increment.
			"add	$32, %[CNT];"
			"jnz	0b;"
			"emms;"

			: [CNT] "=r"    (dummy)
			: [IN]  "r"     (in  + width2 / 2)
			, [OUT] "r"     (out + width2)
			,       "[CNT]" (-width2)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);

	} else if ((sizeof(Pixel) == 4) && streaming && cpu.hasSSE()) {
		// extended-MMX routine 32bpp
		width2 = width & ~15;
		assert(((2 * width2) % 32) == 0);
		unsigned long dummy;
		asm volatile (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%[IN],%[CNT]), %%mm0;"
			"movq	 8(%[IN],%[CNT]), %%mm2;"
			"movq	16(%[IN],%[CNT]), %%mm4;"
			"movq	24(%[IN],%[CNT]), %%mm6;"
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
			"movntq	%%mm0,   (%[OUT],%[CNT],2);"
			"movntq	%%mm1,  8(%[OUT],%[CNT],2);"
			"movntq	%%mm2, 16(%[OUT],%[CNT],2);"
			"movntq	%%mm3, 24(%[OUT],%[CNT],2);"
			"movntq	%%mm4, 32(%[OUT],%[CNT],2);"
			"movntq	%%mm5, 40(%[OUT],%[CNT],2);"
			"movntq	%%mm6, 48(%[OUT],%[CNT],2);"
			"movntq	%%mm7, 56(%[OUT],%[CNT],2);"
			// Increment.
			"add	$32, %[CNT];"
			"jnz	0b;"
			"emms;"

			: [CNT] "=r"    (dummy)
			: [IN]  "r"     (in  + width2 / 2)
			, [OUT] "r"     (out + width2)
			,       "[CNT]" (-2 * width2)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);

	} else if ((sizeof(Pixel) == 4) && cpu.hasMMX()) {
		// MMX routine 32bpp
		width2 = width & ~15;
		assert(((2 * width2) % 32) == 0);
		unsigned long dummy;
		asm volatile (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%[IN],%[CNT]), %%mm0;"
			"movq	 8(%[IN],%[CNT]), %%mm2;"
			"movq	16(%[IN],%[CNT]), %%mm4;"
			"movq	24(%[IN],%[CNT]), %%mm6;"
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
			"movq	%%mm0,   (%[OUT],%[CNT],2);"
			"movq	%%mm1,  8(%[OUT],%[CNT],2);"
			"movq	%%mm2, 16(%[OUT],%[CNT],2);"
			"movq	%%mm3, 24(%[OUT],%[CNT],2);"
			"movq	%%mm4, 32(%[OUT],%[CNT],2);"
			"movq	%%mm5, 40(%[OUT],%[CNT],2);"
			"movq	%%mm6, 48(%[OUT],%[CNT],2);"
			"movq	%%mm7, 56(%[OUT],%[CNT],2);"
			// Increment.
			"add	$32, %[CNT];"
			"jnz	0b;"
			"emms;"

			: [CNT] "=r"    (dummy)
			: [IN]  "r"     (in  + width2 / 2)
			, [OUT] "r"     (out + width2)
			,       "[CNT]" (-2 * width2)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);
	}

	#endif
	#endif

	in += width2;
	out += width2;
	width -= width2;

	for (unsigned x = 0; x < width / 2; x++) {
		out[x * 2] = out[x * 2 + 1] = in[x];
	}
}

template <typename Pixel, bool streaming>
void Scale_1on1<Pixel, streaming>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned long nBytes = width * sizeof(Pixel);
	unsigned long nBytes2 = 0;

	#if ASM_X86

	nBytes2 = nBytes & ~63;
	assert((nBytes2 % 64) == 0);
	const HostCPU& cpu = HostCPU::getInstance();

	#ifdef _MSC_VER

	if (streaming && cpu.hasSSE()) {
		// extended-MMX routine (both 16bpp and 32bpp)
		Scale_1on1_SSE(in, out, nBytes2);
	}

	#else

	if (streaming && cpu.hasSSE()) {
		unsigned long dummy;
		asm volatile (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%[IN],%[CNT]), %%mm0;"
			"movq	 8(%[IN],%[CNT]), %%mm1;"
			"movq	16(%[IN],%[CNT]), %%mm2;"
			"movq	24(%[IN],%[CNT]), %%mm3;"
			"movq	32(%[IN],%[CNT]), %%mm4;"
			"movq	40(%[IN],%[CNT]), %%mm5;"
			"movq	48(%[IN],%[CNT]), %%mm6;"
			"movq	56(%[IN],%[CNT]), %%mm7;"
			// Store.
			"movntq	%%mm0,   (%[OUT],%[CNT]);"
			"movntq	%%mm1,  8(%[OUT],%[CNT]);"
			"movntq	%%mm2, 16(%[OUT],%[CNT]);"
			"movntq	%%mm3, 24(%[OUT],%[CNT]);"
			"movntq	%%mm4, 32(%[OUT],%[CNT]);"
			"movntq	%%mm5, 40(%[OUT],%[CNT]);"
			"movntq	%%mm6, 48(%[OUT],%[CNT]);"
			"movntq	%%mm7, 56(%[OUT],%[CNT]);"
			// Increment.
			"add	$64, %[CNT];"
			"jnz	0b;"
			"emms;"

			: [CNT] "=r"    (dummy)
			: [IN]  "r"     (reinterpret_cast<const char*>(in)  + nBytes2)
			, [OUT] "r"     (reinterpret_cast<char*      >(out) + nBytes2)
			,       "[CNT]" (-nBytes2)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);

	} else if (cpu.hasMMX()) {
		// MMX routine (both 16bpp and 32bpp)
		unsigned long dummy;
		asm volatile (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%[IN],%[CNT]), %%mm0;"
			"movq	 8(%[IN],%[CNT]), %%mm1;"
			"movq	16(%[IN],%[CNT]), %%mm2;"
			"movq	24(%[IN],%[CNT]), %%mm3;"
			"movq	32(%[IN],%[CNT]), %%mm4;"
			"movq	40(%[IN],%[CNT]), %%mm5;"
			"movq	48(%[IN],%[CNT]), %%mm6;"
			"movq	56(%[IN],%[CNT]), %%mm7;"
			// Store.
			"movq	%%mm0,   (%[OUT],%[CNT]);"
			"movq	%%mm1,  8(%[OUT],%[CNT]);"
			"movq	%%mm2, 16(%[OUT],%[CNT]);"
			"movq	%%mm3, 24(%[OUT],%[CNT]);"
			"movq	%%mm4, 32(%[OUT],%[CNT]);"
			"movq	%%mm5, 40(%[OUT],%[CNT]);"
			"movq	%%mm6, 48(%[OUT],%[CNT]);"
			"movq	%%mm7, 56(%[OUT],%[CNT]);"
			// Increment.
			"add	$64, %[CNT];"
			"jnz	0b;"
			"emms;"

			: [CNT] "=r"    (dummy)
			: [IN]  "r"     (reinterpret_cast<const char*>(in)  + nBytes2)
			, [OUT] "r"     (reinterpret_cast<char*      >(out) + nBytes2)
			,       "[CNT]" (-nBytes2)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);
	}

	#endif
	#endif

#ifdef __arm__
	nBytes2 = nBytes & ~63;
	assert(nBytes2 > 0);
	assert((nBytes2 % 64) == 0);
	assert((long(in)  & 3) == 0);
	assert((long(out) & 3) == 0);

	asm volatile (
	"0:\n\t"
		"ldmia	%[IN]! ,{r3,r4,r5,r6,r8,r9,r10,r12};\n\t"
		"stmia	%[OUT]!,{r3,r4,r5,r6,r8,r9,r10,r12};\n\t"
		"ldmia	%[IN]! ,{r3,r4,r5,r6,r8,r9,r10,r12};\n\t"
		"stmia	%[OUT]!,{r3,r4,r5,r6,r8,r9,r10,r12};\n\t"
		"subs	%[NUM],%[NUM],#64;\n\t"
		"bne	0b;\n\t"

		: [NUM] "=r"    (nBytes2)
		, [IN]  "=r"    (in)
		, [OUT] "=r"    (out)
		:       "[NUM]" (nBytes2)
		,       "[IN]"  (in)
		,       "[OUT]" (out)
		: "r3","r4","r5","r6","r8","r9","r10","r12"
	);
	nBytes2 = 0;  // in,out-pointers are already updated
	nBytes &= 63; // remaining bytes
#endif

	      char* out2 = reinterpret_cast<      char*>(out) + nBytes2;
	const char* in2  = reinterpret_cast<const char*>(in ) + nBytes2;
	nBytes -= nBytes2;
	if (nBytes) {
		memcpy(out2, in2, nBytes);
	}
}


template <typename Pixel>
Scale_2on1<Pixel>::Scale_2on1(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_2on1<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned long width2 = 0;

	#if ASM_X86

	const HostCPU& cpu = HostCPU::getInstance();

	#ifdef _MSC_VER

	if ((sizeof(Pixel) == 4) && cpu.hasSSE()) {
		// extended-MMX routine, 32bpp
		width2 = width & ~3;
		assert(((4 * width2) % 16) == 0);
		Scale_2on1_SSE(in, out, width2);
	}

	#else

	if ((sizeof(Pixel) == 4) && cpu.hasSSE()) {
		// extended-MMX routine, 32bpp
		width2 = width & ~3;
		assert(((4 * width2) % 16) == 0);
		unsigned long dummy;
		asm volatile (
			".p2align 4,,15;"
		"0:"
			"movq	  (%[IN],%[CNT],2), %%mm0;" // 0 = AB
			"movq	 8(%[IN],%[CNT],2), %%mm1;" // 1 = CD
			"movq	16(%[IN],%[CNT],2), %%mm2;" // 2 = EF
			"movq	24(%[IN],%[CNT],2), %%mm3;" // 3 = GH
			"movq	%%mm0, %%mm4;"              // 4 = AB
			"punpckhdq	%%mm1, %%mm0;"      // 0 = BD
			"punpckldq	%%mm1, %%mm4;"      // 4 = AC
			"movq	%%mm2, %%mm5;"              // 5 = EF
			"punpckhdq	%%mm3, %%mm2;"      // 2 = FH
			"punpckldq	%%mm3, %%mm5;"      // 5 = EG
			"pavgb	%%mm0, %%mm4;"              // 4 = ab cd
			"movntq	%%mm4,  (%[OUT],%[CNT]);"
			"pavgb	%%mm2, %%mm5;"              // 5 = ef gh
			"movntq	%%mm5, 8(%[OUT],%[CNT]);"
			"add	$16, %[CNT];"
			"jnz	0b;"
			"emms;"

			: [CNT] "=r"    (dummy)
			: [IN]  "r"     (in  + 2 * width2)
			, [OUT] "r"     (out +     width2)
			,       "[CNT]" (-4 * width2)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3", "mm4", "mm5"
			#endif
		);

	} else if ((sizeof(Pixel) == 4) && cpu.hasMMX()) {
		// MMX routine, 32bpp
		width2 = width & ~3;
		assert(((4 * width2) % 16) == 0);
		unsigned long dummy;
		asm volatile (
			"pxor	%%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq	  (%[IN],%[CNT],2), %%mm0;" // 0 = AB
			"movq	%%mm0, %%mm4;"              // 4 = AB
			"punpckhbw	%%mm7, %%mm0;"      // 0 = 0B
			"movq	 8(%[IN],%[CNT],2), %%mm1;" // 1 = CD
			"movq	16(%[IN],%[CNT],2), %%mm2;" // 2 = EF
			"punpcklbw	%%mm7, %%mm4;"      // 4 = 0A
			"movq	%%mm1, %%mm5;"              // 5 = CD
			"paddw	%%mm4, %%mm0;"              // 0 = A + B
			"punpckhbw	%%mm7, %%mm1;"      // 1 = 0D
			"punpcklbw	%%mm7, %%mm5;"      // 5 = 0C
			"psrlw	$1, %%mm0;"                 // 0 = (A + B) / 2
			"paddw	%%mm5, %%mm1;"              // 1 = C + D
			"movq	%%mm2, %%mm4;"              // 4 = EF
			"punpckhbw	%%mm7, %%mm2;"      // 2 = 0F
			"punpcklbw	%%mm7, %%mm4;"      // 4 = 0E
			"psrlw	$1, %%mm1;"                 // 1 = (C + D) / 2
			"paddw	%%mm4, %%mm2;"              // 2 = E + F
			"movq	24(%[IN],%[CNT],2), %%mm3;" // 3 = GH
			"movq	%%mm3, %%mm5;"              // 5 = GH
			"punpckhbw	%%mm7, %%mm3;"      // 3 = 0H
			"packuswb	%%mm1, %%mm0;"      // 0 = ab cd
			"punpcklbw	%%mm7, %%mm5;"      // 5 = 0G
			"psrlw	$1, %%mm2;"                 // 2 = (E + F) / 2
			"paddw	%%mm5, %%mm3;"              // 3 = G + H
			"psrlw	$1, %%mm3;"                 // 3 = (G + H) / 2
			"packuswb	%%mm3, %%mm2;"      // 2 = ef gh
			"movq	%%mm0,  (%[OUT],%[CNT]);"
			"movq	%%mm2, 8(%[OUT],%[CNT]);"
			"add	$16, %[CNT];"
			"jnz	0b;"
			"emms;"

			: [CNT] "=r"    (dummy)
			: [IN]  "r"     (in  + 2 * width2)
			, [OUT] "r"     (out +     width2)
			,       "[CNT]" (-4 * width2)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);

	} else if ((sizeof(Pixel) == 2) && cpu.hasSSE()) {
		// extended-MMX routine, 16bpp
		width2 = width & ~7;
		assert(((2 * width2) % 16) == 0);
		unsigned mask = ~pixelOps.getBlendMask();
		mask = ~(mask | (mask << 16));
		unsigned long dummy;
		asm volatile (
			"movd	%[MASK], %%mm7;"
			"punpckldq	%%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq	  (%[IN],%[CNT],2), %%mm0;" // 0 = ABCD
			"movq	 8(%[IN],%[CNT],2), %%mm1;" // 1 = EFGH
			"movq	%%mm0, %%mm4;"              // 4 = ABCD
			"movq	16(%[IN],%[CNT],2), %%mm2;" // 2 = IJKL
			"punpcklwd	%%mm1, %%mm0;"      // 0 = AEBF
			"punpckhwd	%%mm1, %%mm4;"      // 4 = CGDH
			"movq	%%mm0, %%mm6;"              // 6 = AEBF
			"movq	24(%[IN],%[CNT],2), %%mm3;" // 3 = MNOP
			"movq	%%mm2, %%mm5;"              // 5 = IJKL
			"punpckhwd	%%mm4, %%mm0;"      // 0 = BDFH
			"punpcklwd	%%mm4, %%mm6;"      // 6 = ACEG
			"punpcklwd	%%mm3, %%mm2;"      // 2 = IMJN
			"punpckhwd	%%mm3, %%mm5;"      // 5 = KOLP
			"movq	%%mm2, %%mm1;"              // 1 = IMJN
			"movq	%%mm6, %%mm3;"              // 3 = ACEG
			"movq	%%mm7, %%mm4;"              // 4 = M
			"punpckhwd	%%mm5, %%mm2;"      // 2 = JLNP
			"punpcklwd	%%mm5, %%mm1;"      // 1 = IKMO
			"pandn	%%mm6, %%mm4;"              // 4 = ACEG & ~M
			"pand	%%mm7, %%mm3;"              // 3 = ACEG & M
			"pand	%%mm7, %%mm2;"              // 2 = JLNP & M
			"pand	%%mm7, %%mm0;"              // 0 = BDFH & M
			"movq	%%mm1, %%mm6;"              // 6 = IKMO
			"movq	%%mm7, %%mm5;"              // 5 = M
			"psrlw	$1, %%mm3;"                 // 3 = (ACEG & M) >> 1
			"psrlw	$1, %%mm2;"                 // 2 = (JLNP & M) >> 1
			"pand	%%mm7, %%mm6;"              // 6 = IKMO & M
			"psrlw	$1, %%mm0;"                 // 0 = (BDFH & M) >> 1
			"pandn	%%mm1, %%mm5;"              // 5 = IKMO & ~M
			"psrlw	$1, %%mm6;"                 // 6 = (IKMO & M) >> 1
			"paddw	%%mm4, %%mm3;"              // 3 = ACEG & M  +  ACEG & ~M
			"paddw	%%mm2, %%mm6;"              // 6 = IKMO & M  +  JLNP & M
			"paddw	%%mm0, %%mm3;"              // 3 = ab cd ef gh
			"paddw	%%mm5, %%mm6;"              // 6 = ij kl mn op
			"movntq	%%mm3,  (%[OUT],%[CNT]);"
			"movntq	%%mm6, 8(%[OUT],%[CNT]);"
			"add	$16, %[CNT];"
			"jnz	0b;"
			"emms;"

			: [CNT]  "=r"    (dummy)
			: [IN]   "r"     (in  + 2 * width2)
			, [OUT]  "r"     (out +     width2)
			, [MASK] "r"     (mask)
			,        "[CNT]" (-2 * width2)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);

	} else if ((sizeof(Pixel) == 2) && cpu.hasMMX()) {
		// MMX routine, 16bpp
		width2 = width & ~7;
		assert(((2 * width2) % 16) == 0);
		unsigned mask = ~pixelOps.getBlendMask();
		mask = ~(mask | (mask << 16));
		unsigned long dummy;
		asm volatile (
			"movd	%[MASK], %%mm7;"
			"punpckldq	%%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq	  (%[IN],%[CNT],2), %%mm0;" // 0 = ABCD
			"movq	 8(%[IN],%[CNT],2), %%mm1;" // 1 = EFGH
			"movq	%%mm0, %%mm4;"              // 4 = ABCD
			"movq	16(%[IN],%[CNT],2), %%mm2;" // 2 = IJKL
			"punpcklwd	%%mm1, %%mm0;"      // 0 = AEBF
			"punpckhwd	%%mm1, %%mm4;"      // 4 = CGDH
			"movq	%%mm0, %%mm6;"              // 6 = AEBF
			"movq	24(%[IN],%[CNT],2), %%mm3;" // 3 = MNOP
			"movq	%%mm2, %%mm5;"              // 5 = IJKL
			"punpckhwd	%%mm4, %%mm0;"      // 0 = BDFH
			"punpcklwd	%%mm4, %%mm6;"      // 6 = ACEG
			"punpcklwd	%%mm3, %%mm2;"      // 2 = IMJN
			"punpckhwd	%%mm3, %%mm5;"      // 5 = KOLP
			"movq	%%mm2, %%mm1;"              // 1 = IMJN
			"movq	%%mm6, %%mm3;"              // 3 = ACEG
			"movq	%%mm7, %%mm4;"              // 4 = M
			"punpckhwd	%%mm5, %%mm2;"      // 2 = JLNP
			"punpcklwd	%%mm5, %%mm1;"      // 1 = IKMO
			"pandn	%%mm6, %%mm4;"              // 4 = ACEG & ~M
			"pand	%%mm7, %%mm3;"              // 3 = ACEG & M
			"pand	%%mm7, %%mm2;"              // 2 = JLNP & M
			"pand	%%mm7, %%mm0;"              // 0 = BDFH & M
			"movq	%%mm1, %%mm6;"              // 6 = IKMO
			"movq	%%mm7, %%mm5;"              // 5 = M
			"psrlw	$1, %%mm3;"                 // 3 = (ACEG & M) >> 1
			"psrlw	$1, %%mm2;"                 // 2 = (JLNP & M) >> 1
			"pand	%%mm7, %%mm6;"              // 6 = IKMO & M
			"psrlw	$1, %%mm0;"                 // 0 = (BDFH & M) >> 1
			"pandn	%%mm1, %%mm5;"              // 5 = IKMO & ~M
			"psrlw	$1, %%mm6;"                 // 6 = (IKMO & M) >> 1
			"paddw	%%mm4, %%mm3;"              // 3 = ACEG & M  +  ACEG & ~M
			"paddw	%%mm2, %%mm6;"              // 6 = IKMO & M  +  JLNP & M
			"paddw	%%mm0, %%mm3;"              // 3 = ab cd ef gh
			"paddw	%%mm5, %%mm6;"              // 6 = ij kl mn op
			"movq	%%mm3,  (%[OUT],%[CNT]);"
			"movq	%%mm6, 8(%[OUT],%[CNT]);"
			"add	$16, %[CNT];"
			"jnz	0b;"
			"emms;"

			: [CNT]  "=r"    (dummy)
			: [IN]   "r"     (in  + 2 * width2)
			, [OUT]  "r"     (out +     width2)
			, [MASK] "r"     (mask)
			,        "[CNT]" (-2 * width2)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);
	}
	#endif
	#endif

	in += width2;
	out += width2;
	width -= width2;

	// pure C++ version
	for (unsigned i = 0; i < width; ++i) {
		out[i] = pixelOps.template blend<1, 1>(
			in[2 * i + 0], in[2 * i + 1]);
	}
}


template <typename Pixel>
Scale_6on1<Pixel>::Scale_6on1(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_6on1<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	for (unsigned i = 0; i < width; ++i) {
		out[i] = pixelOps.template blend6<1, 1, 1, 1, 1, 1>(&in[6 * i]);
	}
}


template <typename Pixel>
Scale_4on1<Pixel>::Scale_4on1(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_4on1<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	for (unsigned i = 0; i < width; ++i) {
		out[i] = pixelOps.template blend4<1, 1, 1, 1>(&in[4 * i]);
	}
}


template <typename Pixel>
Scale_3on1<Pixel>::Scale_3on1(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_3on1<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	for (unsigned i = 0; i < width; ++i) {
		out[i] = pixelOps.template blend3<1, 1, 1>(&in[3 * i]);
	}
}


template <typename Pixel>
Scale_3on2<Pixel>::Scale_3on2(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_3on2<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned i = 0, j = 0;
	for (/* */; i < (width - 1); i += 2, j += 3) {
		out[i + 0] = pixelOps.template blend2<2, 1>(&in[j + 0]);
		out[i + 1] = pixelOps.template blend2<1, 2>(&in[j + 1]);
	}
	if (i < width) out[i] = 0;
}


template <typename Pixel>
Scale_3on4<Pixel>::Scale_3on4(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_3on4<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned i = 0, j = 0;
	for (/* */; i < (width - 3); i += 4, j += 3) {
		out[i + 0] =                                 in[j + 0];
		out[i + 1] = pixelOps.template blend2<1, 2>(&in[j + 0]);
		out[i + 2] = pixelOps.template blend2<2, 1>(&in[j + 1]);
		out[i + 3] =                                 in[j + 2];
	}
	for (unsigned k = 0; k < (4 - 1); ++k) {
		if ((i + k) < width) out[i + k] = 0;
	}
}


template <typename Pixel>
Scale_3on8<Pixel>::Scale_3on8(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_3on8<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned i = 0, j = 0;
	for (/* */; i < (width - 7); i += 8, j += 3) {
		out[i + 0] =                                 in[j + 0];
		out[i + 1] =                                 in[j + 0];
		out[i + 2] = pixelOps.template blend2<2, 1>(&in[j + 0]);
		out[i + 3] =                                 in[j + 1];
		out[i + 4] =                                 in[j + 1];
		out[i + 5] = pixelOps.template blend2<1, 2>(&in[j + 1]);
		out[i + 6] =                                 in[j + 2];
		out[i + 7] =                                 in[j + 2];
	}
	for (unsigned k = 0; k < (8 - 1); ++k) {
		if ((i + k) < width) out[i + k] = 0;
	}
}


template <typename Pixel>
Scale_2on3<Pixel>::Scale_2on3(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_2on3<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned i = 0, j = 0;
	for (/* */; i < (width - 2); i += 3, j += 2) {
		out[i + 0] =                                 in[j + 0];
		out[i + 1] = pixelOps.template blend2<1, 1>(&in[j + 0]);
		out[i + 2] =                                 in[j + 1];
	}
	if ((i + 0) < width) out[i + 0] = 0;
	if ((i + 1) < width) out[i + 1] = 0;
}


template <typename Pixel>
Scale_4on3<Pixel>::Scale_4on3(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_4on3<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned i = 0, j = 0;
	for (/* */; i < (width - 2); i += 3, j += 4) {
		out[i + 0] = pixelOps.template blend2<3, 1>(&in[j + 0]);
		out[i + 1] = pixelOps.template blend2<1, 1>(&in[j + 1]);
		out[i + 2] = pixelOps.template blend2<1, 3>(&in[j + 2]);
	}
	if ((i + 0) < width) out[i + 0] = 0;
	if ((i + 1) < width) out[i + 1] = 0;
}


template <typename Pixel>
Scale_8on3<Pixel>::Scale_8on3(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_8on3<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned i = 0, j = 0;
	for (/* */; i < (width - 2); i += 3, j += 8) {
		out[i + 0] = pixelOps.template blend3<3, 3, 2>   (&in[j + 0]);
		out[i + 1] = pixelOps.template blend4<1, 3, 3, 1>(&in[j + 2]);
		out[i + 2] = pixelOps.template blend3<2, 3, 3>   (&in[j + 5]);
	}
	if ((i + 0) < width) out[i + 0] = 0;
	if ((i + 1) < width) out[i + 1] = 0;
}


template <typename Pixel>
Scale_2on9<Pixel>::Scale_2on9(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_2on9<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned i = 0, j = 0;
	for (/* */; i < (width - 8); i += 9, j += 2) {
		out[i + 0] =                                 in[j + 0];
		out[i + 1] =                                 in[j + 0];
		out[i + 2] =                                 in[j + 0];
		out[i + 3] =                                 in[j + 0];
		out[i + 4] = pixelOps.template blend2<1, 1>(&in[j + 0]);
		out[i + 5] =                                 in[j + 1];
		out[i + 6] =                                 in[j + 1];
		out[i + 7] =                                 in[j + 1];
		out[i + 8] =                                 in[j + 1];
	}
	if ((i + 0) < width) out[i + 0] = 0;
	if ((i + 1) < width) out[i + 1] = 0;
	if ((i + 2) < width) out[i + 2] = 0;
	if ((i + 3) < width) out[i + 3] = 0;
	if ((i + 4) < width) out[i + 4] = 0;
	if ((i + 5) < width) out[i + 5] = 0;
	if ((i + 6) < width) out[i + 6] = 0;
	if ((i + 7) < width) out[i + 7] = 0;
}


template <typename Pixel>
Scale_4on9<Pixel>::Scale_4on9(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_4on9<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned i = 0, j = 0;
	for (/* */; i < (width - 8); i += 9, j += 4) {
		out[i + 0] =                                 in[j + 0];
		out[i + 1] =                                 in[j + 0];
		out[i + 2] = pixelOps.template blend2<1, 3>(&in[j + 0]);
		out[i + 3] =                                 in[j + 1];
		out[i + 4] = pixelOps.template blend2<1, 1>(&in[j + 1]);
		out[i + 5] =                                 in[j + 2];
		out[i + 6] = pixelOps.template blend2<3, 1>(&in[j + 2]);
		out[i + 7] =                                 in[j + 3];
		out[i + 8] =                                 in[j + 3];
	}
	if ((i + 0) < width) out[i + 0] = 0;
	if ((i + 1) < width) out[i + 1] = 0;
	if ((i + 2) < width) out[i + 2] = 0;
	if ((i + 3) < width) out[i + 3] = 0;
	if ((i + 4) < width) out[i + 4] = 0;
	if ((i + 5) < width) out[i + 5] = 0;
	if ((i + 6) < width) out[i + 6] = 0;
	if ((i + 7) < width) out[i + 7] = 0;
}


template <typename Pixel>
Scale_8on9<Pixel>::Scale_8on9(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_8on9<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	unsigned i = 0, j = 0;
	for (/* */; i < width; i += 9, j += 8) {
		out[i + 0] =                                 in[j + 0];
		out[i + 1] = pixelOps.template blend2<1, 7>(&in[j + 0]);
		out[i + 2] = pixelOps.template blend2<1, 3>(&in[j + 1]);
		out[i + 3] = pixelOps.template blend2<3, 5>(&in[j + 2]);
		out[i + 4] = pixelOps.template blend2<1, 1>(&in[j + 3]);
		out[i + 5] = pixelOps.template blend2<5, 3>(&in[j + 4]);
		out[i + 6] = pixelOps.template blend2<3, 1>(&in[j + 5]);
		out[i + 7] = pixelOps.template blend2<7, 1>(&in[j + 6]);
		out[i + 8] =                                 in[j + 7];
	}
	if ((i + 0) < width) out[i + 0] = 0;
	if ((i + 1) < width) out[i + 1] = 0;
	if ((i + 2) < width) out[i + 2] = 0;
	if ((i + 3) < width) out[i + 3] = 0;
	if ((i + 4) < width) out[i + 4] = 0;
	if ((i + 5) < width) out[i + 5] = 0;
	if ((i + 6) < width) out[i + 6] = 0;
	if ((i + 7) < width) out[i + 7] = 0;
}


template <typename Pixel>
Scale_4on5<Pixel>::Scale_4on5(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_4on5<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	assert((width % 5) == 0);
	for (unsigned i = 0, j = 0; i < width; i += 5, j += 4) {
		out[i + 0] =                                 in[j + 0];
		out[i + 1] = pixelOps.template blend2<1, 3>(&in[j + 0]);
		out[i + 2] = pixelOps.template blend2<1, 1>(&in[j + 1]);
		out[i + 3] = pixelOps.template blend2<3, 1>(&in[j + 2]);
		out[i + 4] =                                 in[j + 3];
	}
}


template <typename Pixel>
Scale_7on8<Pixel>::Scale_7on8(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_7on8<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	assert((width % 8) == 0);
	for (unsigned i = 0, j = 0; i < width; i += 8, j += 7) {
		out[i + 0] =                                 in[j + 0];
		out[i + 1] = pixelOps.template blend2<1, 6>(&in[j + 0]);
		out[i + 2] = pixelOps.template blend2<2, 5>(&in[j + 1]);
		out[i + 3] = pixelOps.template blend2<3, 4>(&in[j + 2]);
		out[i + 4] = pixelOps.template blend2<4, 3>(&in[j + 3]);
		out[i + 5] = pixelOps.template blend2<5, 2>(&in[j + 4]);
		out[i + 6] = pixelOps.template blend2<6, 1>(&in[j + 5]);
		out[i + 7] =                                 in[j + 6];
	}
}


template <typename Pixel>
Scale_17on20<Pixel>::Scale_17on20(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_17on20<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	assert((width % 20) == 0);
	for (unsigned i = 0, j = 0; i < width; i += 20, j += 17) {
		out[i +  0] =                                   in[j +  0];
		out[i +  1] = pixelOps.template blend2< 3, 14>(&in[j +  0]);
		out[i +  2] = pixelOps.template blend2< 6, 11>(&in[j +  1]);
		out[i +  3] = pixelOps.template blend2< 9,  8>(&in[j +  2]);
		out[i +  4] = pixelOps.template blend2<12,  5>(&in[j +  3]);
		out[i +  5] = pixelOps.template blend2<15,  2>(&in[j +  4]);
		out[i +  6] =                                   in[j +  5];
		out[i +  7] = pixelOps.template blend2< 1, 16>(&in[j +  5]);
		out[i +  8] = pixelOps.template blend2< 4, 13>(&in[j +  6]);
		out[i +  9] = pixelOps.template blend2< 7, 10>(&in[j +  7]);
		out[i + 10] = pixelOps.template blend2<10,  7>(&in[j +  8]);
		out[i + 11] = pixelOps.template blend2<13,  4>(&in[j +  9]);
		out[i + 12] = pixelOps.template blend2<16,  1>(&in[j + 10]);
		out[i + 13] =                                   in[j + 11];
		out[i + 14] = pixelOps.template blend2< 2, 15>(&in[j + 11]);
		out[i + 15] = pixelOps.template blend2< 5, 12>(&in[j + 12]);
		out[i + 16] = pixelOps.template blend2< 8,  9>(&in[j + 13]);
		out[i + 17] = pixelOps.template blend2<11,  6>(&in[j + 14]);
		out[i + 18] = pixelOps.template blend2<14,  3>(&in[j + 15]);
		out[i + 19] =                                   in[j + 16];
	}
}


template <typename Pixel>
Scale_9on10<Pixel>::Scale_9on10(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_9on10<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width) __restrict
{
	assert((width % 10) == 0);
	for (unsigned i = 0, j = 0; i < width; i += 10, j += 9) {
		out[i + 0] =                                 in[j + 0];
		out[i + 1] = pixelOps.template blend2<1, 8>(&in[j + 0]);
		out[i + 2] = pixelOps.template blend2<2, 7>(&in[j + 1]);
		out[i + 3] = pixelOps.template blend2<3, 6>(&in[j + 2]);
		out[i + 4] = pixelOps.template blend2<4, 5>(&in[j + 3]);
		out[i + 5] = pixelOps.template blend2<5, 4>(&in[j + 4]);
		out[i + 6] = pixelOps.template blend2<6, 3>(&in[j + 5]);
		out[i + 7] = pixelOps.template blend2<7, 2>(&in[j + 6]);
		out[i + 8] = pixelOps.template blend2<8, 1>(&in[j + 7]);
		out[i + 9] =                                 in[j + 8];
	}
}


template <typename Pixel, unsigned w1, unsigned w2>
BlendLines<Pixel, w1, w2>::BlendLines(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel, unsigned w1, unsigned w2>
void BlendLines<Pixel, w1, w2>::operator()(
	const Pixel* __restrict in1, const Pixel* __restrict in2,
	Pixel* __restrict out, unsigned width) __restrict
{
	// TODO MMX/SSE optimizations
	// pure C++ version
	for (unsigned i = 0; i < width; ++i) {
		out[i] = pixelOps.template blend<w1, w2>(in1[i], in2[i]);
	}
}


template<typename Pixel>
ZoomLine<Pixel>::ZoomLine(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<typename Pixel>
void ZoomLine<Pixel>::operator()(
	const Pixel* in,  unsigned inWidth,
	      Pixel* out, unsigned outWidth) const
{
	static const unsigned FACTOR = 256;

	unsigned step = FACTOR * inWidth / outWidth;
	unsigned i = 0 * FACTOR;
	for (unsigned o = 0; o < outWidth; ++o) {
		Pixel p0 = in[(i / FACTOR) + 0];
		Pixel p1 = in[(i / FACTOR) + 1];
		out[o] = pixelOps.lerp(p0, p1, i % FACTOR);
		i += step;
	}
}


template <typename Pixel>
AlphaBlendLines<Pixel>::AlphaBlendLines(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void AlphaBlendLines<Pixel>::operator()(
	const Pixel* __restrict in1, const Pixel* __restrict in2,
	Pixel* __restrict out, unsigned width) __restrict
{
	for (unsigned i = 0; i < width; ++i) {
		out[i] = pixelOps.alphaBlend(in1[i], in2[i]);
	}
}

template <typename Pixel>
void AlphaBlendLines<Pixel>::operator()(
	Pixel in1, const Pixel* __restrict in2,
	Pixel* __restrict out, unsigned width) __restrict
{
	// ATM this routine is only called when 'in1' is not fully opaque nor
	// fully transparent. This cannot happen in 16bpp modes.
	assert(sizeof(Pixel) == 4);

	unsigned alpha = pixelOps.alpha(in1);

	// When one of the two colors is loop-invariant, using the
	// pre-multiplied-alpha-blending equation is a tiny bit more efficient
	// than using alphaBlend() or even lerp().
	//    for (unsigned i = 0; i < width; ++i) {
	//        out[i] = pixelOps.lerp(in1, in2[i], alpha);
	//    }
	Pixel in1M = pixelOps.multiply(in1, alpha);
	unsigned alpha2 = 256 - alpha;
	for (unsigned i = 0; i < width; ++i) {
		out[i] = in1M + pixelOps.multiply(in2[i], alpha2);
	}
}

} // namespace openmsx

#endif
