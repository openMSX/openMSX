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
	~PolyLineScaler() {}
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
{
	#if ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	#ifndef _MSC_VER
	if ((sizeof(Pixel) == 2) && streaming && cpu.hasSSE()) {
		// extended-MMX routine 16bpp
		assert((width % 32) == 0);
		asm (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%0,%2), %%mm0;"
			"movq	 8(%0,%2), %%mm2;"
			"movq	16(%0,%2), %%mm4;"
			"movq	24(%0,%2), %%mm6;"
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
			"movntq	%%mm0,   (%1,%2,2);"
			"movntq	%%mm1,  8(%1,%2,2);"
			"movntq	%%mm2, 16(%1,%2,2);"
			"movntq	%%mm3, 24(%1,%2,2);"
			"movntq	%%mm4, 32(%1,%2,2);"
			"movntq	%%mm5, 40(%1,%2,2);"
			"movntq	%%mm6, 48(%1,%2,2);"
			"movntq	%%mm7, 56(%1,%2,2);"
			// Increment.
			"add	$32, %2;"
			"jnz	0b;"
			"emms;"

			: // no output
			: "r" (in  + width / 2) // 0
			, "r" (out + width) // 1
			, "r" (-width) // 2
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3",
			  "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}

	if ((sizeof(Pixel) == 2) && cpu.hasMMX()) {
		// MMX routine 16bpp
		assert((width % 32) == 0);
		asm (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%0,%2), %%mm0;"
			"movq	 8(%0,%2), %%mm2;"
			"movq	16(%0,%2), %%mm4;"
			"movq	24(%0,%2), %%mm6;"
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
			"movq	%%mm0,   (%1,%2,2);"
			"movq	%%mm1,  8(%1,%2,2);"
			"movq	%%mm2, 16(%1,%2,2);"
			"movq	%%mm3, 24(%1,%2,2);"
			"movq	%%mm4, 32(%1,%2,2);"
			"movq	%%mm5, 40(%1,%2,2);"
			"movq	%%mm6, 48(%1,%2,2);"
			"movq	%%mm7, 56(%1,%2,2);"
			// Increment.
			"add	$32, %2;"
			"jnz	0b;"
			"emms;"

			: // no output
			: "r" (in  + width / 2) // 0
			, "r" (out + width) // 1
			, "r" (-width) // 2
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3",
			  "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}

	if ((sizeof(Pixel) == 4) && streaming && cpu.hasSSE()) {
		// extended-MMX routine 32bpp
		assert(((2 * width) % 32) == 0);
		asm (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%0,%2), %%mm0;"
			"movq	 8(%0,%2), %%mm2;"
			"movq	16(%0,%2), %%mm4;"
			"movq	24(%0,%2), %%mm6;"
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
			"movntq	%%mm0,   (%1,%2,2);"
			"movntq	%%mm1,  8(%1,%2,2);"
			"movntq	%%mm2, 16(%1,%2,2);"
			"movntq	%%mm3, 24(%1,%2,2);"
			"movntq	%%mm4, 32(%1,%2,2);"
			"movntq	%%mm5, 40(%1,%2,2);"
			"movntq	%%mm6, 48(%1,%2,2);"
			"movntq	%%mm7, 56(%1,%2,2);"
			// Increment.
			"add	$32, %2;"
			"jnz	0b;"
			"emms;"

			: // no output
			: "r" (in  + width / 2) // 0
			, "r" (out + width) // 1
			, "r" (-2 * width) // 2
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3",
			  "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}
	#endif
	if ((sizeof(Pixel) == 4) && cpu.hasMMX()) {
		// MMX routine 32bpp
		assert(((2 * width) % 32) == 0);
	#ifdef _MSC_VER
		Scale_1on2_4_MMX(in, out, width);
	#else
		asm (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%0,%2), %%mm0;"
			"movq	 8(%0,%2), %%mm2;"
			"movq	16(%0,%2), %%mm4;"
			"movq	24(%0,%2), %%mm6;"
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
			"movq	%%mm0,   (%1,%2,2);"
			"movq	%%mm1,  8(%1,%2,2);"
			"movq	%%mm2, 16(%1,%2,2);"
			"movq	%%mm3, 24(%1,%2,2);"
			"movq	%%mm4, 32(%1,%2,2);"
			"movq	%%mm5, 40(%1,%2,2);"
			"movq	%%mm6, 48(%1,%2,2);"
			"movq	%%mm7, 56(%1,%2,2);"
			// Increment.
			"add	$32, %2;"
			"jnz	0b;"
			"emms;"

			: // no output
			: "r" (in  + width / 2) // 0
			, "r" (out + width) // 1
			, "r" (-2 * width) // 2
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3",
			  "mm4", "mm5", "mm6", "mm7"
			#endif
		);
	#endif
		return;
	}
	#endif

	for (unsigned x = 0; x < width / 2; x++) {
		out[x * 2] = out[x * 2 + 1] = in[x];
	}
}

template <typename Pixel, bool streaming>
void Scale_1on1<Pixel, streaming>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
{
	unsigned long nBytes = width * sizeof(Pixel);
	#if ASM_X86
	assert((nBytes % 64) == 0);
	const HostCPU& cpu = HostCPU::getInstance();
	if (streaming && cpu.hasSSE()) {
		// extended-MMX routine (both 16bpp and 32bpp)
	#ifdef _MSC_VER
		Scale_1on1_SSE(in, out, nBytes);
		return;
	}
	#else
		asm (
			".p2align 4,,15;"
		"0:"
			// Load.
			"movq	  (%0,%2), %%mm0;"
			"movq	 8(%0,%2), %%mm1;"
			"movq	16(%0,%2), %%mm2;"
			"movq	24(%0,%2), %%mm3;"
			"movq	32(%0,%2), %%mm4;"
			"movq	40(%0,%2), %%mm5;"
			"movq	48(%0,%2), %%mm6;"
			"movq	56(%0,%2), %%mm7;"
			// Store.
			"movntq	%%mm0,   (%1,%2);"
			"movntq	%%mm1,  8(%1,%2);"
			"movntq	%%mm2, 16(%1,%2);"
			"movntq	%%mm3, 24(%1,%2);"
			"movntq	%%mm4, 32(%1,%2);"
			"movntq	%%mm5, 40(%1,%2);"
			"movntq	%%mm6, 48(%1,%2);"
			"movntq	%%mm7, 56(%1,%2);"
			// Increment.
			"add	$64, %2;"
			"jnz	0b;"
			"emms;"

			: // no output
			: "r" (reinterpret_cast<const char*>(in)  + nBytes) // 0
			, "r" (reinterpret_cast<char*      >(out) + nBytes) // 1
			, "r" (-nBytes) // 2
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
			"movq	  (%0,%2), %%mm0;"
			"movq	 8(%0,%2), %%mm1;"
			"movq	16(%0,%2), %%mm2;"
			"movq	24(%0,%2), %%mm3;"
			"movq	32(%0,%2), %%mm4;"
			"movq	40(%0,%2), %%mm5;"
			"movq	48(%0,%2), %%mm6;"
			"movq	56(%0,%2), %%mm7;"
			// Store.
			"movq	%%mm0,   (%1,%2);"
			"movq	%%mm1,  8(%1,%2);"
			"movq	%%mm2, 16(%1,%2);"
			"movq	%%mm3, 24(%1,%2);"
			"movq	%%mm4, 32(%1,%2);"
			"movq	%%mm5, 40(%1,%2);"
			"movq	%%mm6, 48(%1,%2);"
			"movq	%%mm7, 56(%1,%2);"
			// Increment.
			"add	$64, %2;"
			"jnz	0b;"
			"emms;"

			: // no output
			: "r" (reinterpret_cast<const char*>(in)  + nBytes) // 0
			, "r" (reinterpret_cast<char*      >(out) + nBytes) // 1
			, "r" (-nBytes) // 2
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3",
			  "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}
	#endif
	#endif

#ifdef __arm__
	assert(nBytes > 0);
	assert((nBytes % 64) == 0);
	assert((long(in)  & 3) == 0);
	assert((long(out) & 3) == 0);

	asm volatile (
	"0:\n\t"
		"ldmia	%1!,{r0,r1,r2,r3,r4,r5,r6,r7};\n\t"
		"stmia	%2!,{r0,r1,r2,r3,r4,r5,r6,r7};\n\t"
		"ldmia	%1!,{r0,r1,r2,r3,r4,r5,r6,r7};\n\t"
		"stmia	%2!,{r0,r1,r2,r3,r4,r5,r6,r7};\n\t"
		"subs	%0,%0,#64;\n\t"
		"bne	0b;\n\t"

		: // no output
		: "r" (nBytes)
		, "r" (in)
		, "r" (out)
		: "r0","r1","r2","r3","r4","r5","r6","r7"
	);
	return;
#endif

	memcpy(out, in, nBytes);
}


template <typename Pixel>
Scale_2on1<Pixel>::Scale_2on1(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel>
void Scale_2on1<Pixel>::operator()(
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
{
	#if ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if ((sizeof(Pixel) == 4) && cpu.hasSSE()) {
		// extended-MMX routine, 32bpp
		assert(((4 * width) % 16) == 0);
	#ifdef _MSC_VER
		Scale_2on1_SSE(in, out, width);
		return;
	}
	#else
		asm volatile (
			".p2align 4,,15;"
		"0:"
			"movq	  (%0,%2,2), %%mm0;"    // 0 = AB
			"movq	 8(%0,%2,2), %%mm1;"    // 1 = CD
			"movq	16(%0,%2,2), %%mm2;"    // 2 = EF
			"movq	24(%0,%2,2), %%mm3;"    // 3 = GH
			"movq	%%mm0, %%mm4;"          // 4 = AB
			"punpckhdq	%%mm1, %%mm0;"  // 0 = BD
			"punpckldq	%%mm1, %%mm4;"  // 4 = AC
			"movq	%%mm2, %%mm5;"          // 5 = EF
			"punpckhdq	%%mm3, %%mm2;"  // 2 = FH
			"punpckldq	%%mm3, %%mm5;"  // 5 = EG
			"pavgb	%%mm0, %%mm4;"          // 4 = ab cd
			"movntq	%%mm4,  (%1,%2);"
			"pavgb	%%mm2, %%mm5;"          // 5 = ef gh
			"movntq	%%mm5, 8(%1,%2);"
			"add	$16, %2;"
			"jnz	0b;"
			"emms;"

			: // no output
			: "r" (in  + 2 * width) // 0
			, "r" (out +     width) // 1
			, "r" (-4 * width)      // 2
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5"
			#endif
		);
		return;
	}
	if ((sizeof(Pixel) == 4) && cpu.hasMMX()) {
		// MMX routine, 32bpp
		assert(((4 * width) % 16) == 0);
		asm volatile (
			"pxor	%%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq	  (%0,%2,2), %%mm0;"    // 0 = AB
			"movq	%%mm0, %%mm4;"          // 4 = AB
			"punpckhbw	%%mm7, %%mm0;"  // 0 = 0B
			"movq	 8(%0,%2,2), %%mm1;"    // 1 = CD
			"movq	16(%0,%2,2), %%mm2;"    // 2 = EF
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
			"movq	24(%0,%2,2), %%mm3;"    // 3 = GH
			"movq	%%mm3, %%mm5;"          // 5 = GH
			"punpckhbw	%%mm7, %%mm3;"  // 3 = 0H
			"packuswb	%%mm1, %%mm0;"  // 0 = ab cd
			"punpcklbw	%%mm7, %%mm5;"  // 5 = 0G
			"psrlw	$1, %%mm2;"             // 2 = (E + F) / 2
			"paddw	%%mm5, %%mm3;"          // 3 = G + H
			"psrlw	$1, %%mm3;"             // 3 = (G + H) / 2
			"packuswb	%%mm3, %%mm2;"  // 2 = ef gh
			"movq	%%mm0,  (%1,%2);"
			"movq	%%mm2, 8(%1,%2);"
			"add	$16, %2;"
			"jnz	0b;"
			"emms;"

			: // no output
			: "r" (in  + 2 * width) // 0
			, "r" (out +     width) // 1
			, "r" (-4 * width)      // 2
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}
	if ((sizeof(Pixel) == 2) && cpu.hasSSE()) {
		// extended-MMX routine, 16bpp
		assert(((2 * width) % 16) == 0);
		unsigned mask = ~pixelOps.getBlendMask();
		mask = ~(mask | (mask << 16));
		asm volatile (
			"movd	%2, %%mm7;"
			"punpckldq	%%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq	  (%0,%3,2), %%mm0;"    // 0 = ABCD
			"movq	 8(%0,%3,2), %%mm1;"    // 1 = EFGH
			"movq	%%mm0, %%mm4;"          // 4 = ABCD
			"movq	16(%0,%3,2), %%mm2;"    // 2 = IJKL
			"punpcklwd	%%mm1, %%mm0;"  // 0 = AEBF
			"punpckhwd	%%mm1, %%mm4;"  // 4 = CGDH
			"movq	%%mm0, %%mm6;"          // 6 = AEBF
			"movq	24(%0,%3,2), %%mm3;"    // 3 = MNOP
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
			"movntq	%%mm3,  (%1,%3);"
			"movntq	%%mm6, 8(%1,%3);"
			"add	$16, %3;"
			"jnz	0b;"
			"emms;"

			: // no output
			: "r" (in  + 2 * width) // 0
			, "r" (out +     width) // 1
			, "r" (mask)             // 2
			, "r" (-2 * width)      // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}
	if ((sizeof(Pixel) == 2) && cpu.hasMMX()) {
		// MMX routine, 16bpp
		assert(((2 * width) % 16) == 0);
		unsigned mask = ~pixelOps.getBlendMask();
		mask = ~(mask | (mask << 16));
		asm volatile (
			"movd	%2, %%mm7;"
			"punpckldq	%%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq	  (%0,%3,2), %%mm0;"    // 0 = ABCD
			"movq	 8(%0,%3,2), %%mm1;"    // 1 = EFGH
			"movq	%%mm0, %%mm4;"          // 4 = ABCD
			"movq	16(%0,%3,2), %%mm2;"    // 2 = IJKL
			"punpcklwd	%%mm1, %%mm0;"  // 0 = AEBF
			"punpckhwd	%%mm1, %%mm4;"  // 4 = CGDH
			"movq	%%mm0, %%mm6;"          // 6 = AEBF
			"movq	24(%0,%3,2), %%mm3;"    // 3 = MNOP
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
			"movq	%%mm3,  (%1,%3);"
			"movq	%%mm6, 8(%1,%3);"
			"add	$16, %3;"
			"jnz	0b;"
			"emms;"

			: // no output
			: "r" (in  + 2 * width) // 0
			, "r" (out +     width) // 1
			, "r" (mask)             // 2
			, "r" (-2 * width)      // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3"
			, "mm4", "mm5", "mm6", "mm7"
			#endif
		);
		return;
	}
	#endif
	#endif

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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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
	const Pixel* __restrict in, Pixel* __restrict out, unsigned long width)
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


template <typename Pixel, unsigned w1, unsigned w2>
BlendLines<Pixel, w1, w2>::BlendLines(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template <typename Pixel, unsigned w1, unsigned w2>
void BlendLines<Pixel, w1, w2>::operator()(
	const Pixel* __restrict in1, const Pixel* __restrict in2,
	Pixel* __restrict out, unsigned width)
{
	// TODO MMX/SSE optimizations
	// pure C++ version
	for (unsigned i = 0; i < width; ++i) {
		out[i] = pixelOps.template blend<w1, w2>(in1[i], in2[i]);
	}
}

} // namespace openmsx

#endif
