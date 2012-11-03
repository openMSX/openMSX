// $Id$

#include "FBPostProcessor.hh"
#include "RawFrame.hh"
#include "StretchScalerOutput.hh"
#include "ScalerOutput.hh"
#include "RenderSettings.hh"
#include "Scaler.hh"
#include "ScalerFactory.hh"
#include "OutputSurface.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "EnumSetting.hh"
#include "HostCPU.hh"
#include "Math.hh"
#include "aligned.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

static const unsigned NOISE_SHIFT = 8192;
static const unsigned NOISE_BUF_SIZE = 2 * NOISE_SHIFT;
ALIGNED(static signed char noiseBuf[NOISE_BUF_SIZE], 16);

// Assembly functions
#ifdef _MSC_VER
extern "C"
{
	void __cdecl FBPostProcessor_drawNoiseLine_4_SSE2(
		void* in, void* out, void* noise, unsigned long width);
}
#endif

template <class Pixel>
void FBPostProcessor<Pixel>::preCalcNoise(double factor)
{
	// We skip noise drawing if the factor is 0, so there is no point in
	// initializing the random data in that case.
	if (factor == 0) return;

	// for 32bpp groups of 4 consecutive noiseBuf elements (starting at
	// 4 element boundaries) must have the same value. Later optimizations
	// depend on it.

	double scale[4];
	if (sizeof(Pixel) == 4) {
		// 32bpp
		// TODO ATM we compensate for big endian here. A better
		// alternative is to turn noiseBuf into an array of ints (it's
		// now bytes) and in the 16bpp code extract R,G,B components
		// from those ints
		const Pixel p = Pixel(OPENMSX_BIGENDIAN ? 0x00010203
		                                        : 0x03020100);
		// TODO we can also fill the array with 'factor' and only set
		// 'alpha' to 0.0. But PixelOperations doesn't offer a simple
		// way to get the position of the alpha byte (yet).
		scale[0] = scale[1] = scale[2] = scale[3] = 0.0;
		scale[pixelOps.red  (p)] = factor;
		scale[pixelOps.green(p)] = factor;
		scale[pixelOps.blue (p)] = factor;
	} else {
		// 16bpp
		scale[0] = (pixelOps.getMaxRed()   / 255.0) * factor;
		scale[1] = (pixelOps.getMaxGreen() / 255.0) * factor;
		scale[2] = (pixelOps.getMaxBlue()  / 255.0) * factor;
		scale[3] = 0.0;
	}

	for (unsigned i = 0; i < NOISE_BUF_SIZE; i += 8) {
		double r1, r2;
		Math::gaussian2(r1, r2);
		noiseBuf[i + 0] = Math::clip<-128, 127>(r1, scale[0]);
		noiseBuf[i + 1] = Math::clip<-128, 127>(r1, scale[1]);
		noiseBuf[i + 2] = Math::clip<-128, 127>(r1, scale[2]);
		noiseBuf[i + 3] = Math::clip<-128, 127>(r1, scale[3]);
		noiseBuf[i + 4] = Math::clip<-128, 127>(r2, scale[0]);
		noiseBuf[i + 5] = Math::clip<-128, 127>(r2, scale[1]);
		noiseBuf[i + 6] = Math::clip<-128, 127>(r2, scale[2]);
		noiseBuf[i + 7] = Math::clip<-128, 127>(r2, scale[3]);
	}
}

/** Add noise to the given pixel.
 * @param p contains 4 8-bit unsigned components, so components have range [0, 255]
 * @param n contains 4 8-bit   signed components, so components have range [-128, 127]
 * @result per component result of clip<0, 255>(p + n)
 */
static inline unsigned addNoise4(unsigned p, unsigned n)
{
	// unclipped result (lower 8 bits of each component)
	// alternative:
	//   unsigned s20 = ((p & 0x00FF00FF) + (n & 0x00FF00FF)) & 0x00FF00FF;
	//   unsigned s31 = ((p & 0xFF00FF00) + (n & 0xFF00FF00)) & 0xFF00FF00;
	//   unsigned s = s20 | s31;
	unsigned s0 = p + n;                     // carry spills to neighbors
	unsigned ci = (p ^ n ^ s0) & 0x01010100; // carry-in bits of prev sum
	unsigned s  = s0 - ci;                   // subtract carry bits again

	// Underflow of a component happens ONLY
	//   WHEN input  component is in range [0, 127]
	//   AND  noise  component is negative
	//   AND  result component is in range [128, 255]
	// Overflow of a component happens ONLY
	//   WHEN input  component in in range [128, 255]
	//   AND  noise  component is positive
	//   AND  result component is in range [0, 127]
	// Create a mask per component containing 00 for no under/overflow,
	//                                        FF for    under/overflow
	// ((~p & n & s) | (p & ~n & ~s)) == ((p ^ n) & (p ^ s))
	unsigned t = (p ^ n) & (p ^ s) & 0x80808080;
	unsigned u1 = t & s; // underflow   (alternative: u1 = t & n)
	// alternative1: unsigned u2 = u1 | (u1 >> 1);
	//               unsigned u4 = u2 | (u2 >> 2);
	//               unsigned u8 = u4 | (u4 >> 4);
	// alternative2: unsigned u8 = (u1 >> 7) * 0xFF;
	unsigned u8 = (u1 << 1) - (u1 >> 7);

	unsigned o1 = t & p; // overflow
	unsigned o8 = (o1 << 1) - (o1 >> 7);

	// clip result
	return (s & (~u8)) | o8;
}

template <class Pixel>
void FBPostProcessor<Pixel>::drawNoiseLine(
		Pixel* in, Pixel* out, signed char* noise, unsigned long width)
{
	#if ASM_X86
	if ((sizeof(Pixel) == 4) && HostCPU::hasSSE2()) {
		// SSE2 32bpp
		assert(((4 * width) % 64) == 0);
	#ifdef _MSC_VER
		FBPostProcessor_drawNoiseLine_4_SSE2(in, out, noise, width);
		return;
	}
	#else
		unsigned long dummy;
		asm volatile (
			"pcmpeqb  %%xmm7, %%xmm7;"
			"psllw    $15, %%xmm7;"
			"packsswb %%xmm7, %%xmm7;"
			".p2align 4,,15;"
		"0:"
			"movdqa     (%[IN], %[CNT]), %%xmm0;"
			"movdqa   16(%[IN], %[CNT]), %%xmm1;"
			"movdqa   32(%[IN], %[CNT]), %%xmm2;"
			"pxor     %%xmm7, %%xmm0;"
			"movdqa   48(%[IN], %[CNT]), %%xmm3;"
			"pxor     %%xmm7, %%xmm1;"
			"pxor     %%xmm7, %%xmm2;"
			"paddsb     (%[NOISE], %[CNT]), %%xmm0;"
			"pxor     %%xmm7, %%xmm3;"
			"paddsb   16(%[NOISE], %[CNT]), %%xmm1;"
			"paddsb   32(%[NOISE], %[CNT]), %%xmm2;"
			"pxor     %%xmm7, %%xmm0;"
			"paddsb   48(%[NOISE], %[CNT]), %%xmm3;"
			"pxor     %%xmm7, %%xmm1;"
			"pxor     %%xmm7, %%xmm2;"
			"movdqa   %%xmm0,   (%[OUT], %[CNT]);"
			"pxor     %%xmm7, %%xmm3;"
			"movdqa   %%xmm1, 16(%[OUT], %[CNT]);"
			"movdqa   %%xmm2, 32(%[OUT], %[CNT]);"
			"movdqa   %%xmm3, 48(%[OUT], %[CNT]);"
			"add      $64, %[CNT];"
			"jnz      0b;"

			: [CNT]   "=r"    (dummy)
			: [IN]    "r"     (in    + width)
			, [OUT]   "r"     (out   + width)
			, [NOISE] "r"     (noise + 4 * width)
			,         "[CNT]" (-4 * width)
			: "memory"
			#ifdef __SSE__
			, "xmm0", "xmm1", "xmm2", "xmm3", "xmm7"
			#endif
		);
		return;
	}
	if ((sizeof(Pixel) == 4) && HostCPU::hasSSE()) {
		// extended-MMX 32bpp
		assert(((4 * width) % 32) == 0);
		unsigned long dummy;
		asm volatile (
			"pcmpeqb  %%mm7, %%mm7;"
			"psllw    $15, %%mm7;"
			"packsswb %%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"prefetchnta 320(%[IN], %[CNT]);"
			"movq       (%[IN], %[CNT]), %%mm0;"
			"movq      8(%[IN], %[CNT]), %%mm1;"
			"movq     16(%[IN], %[CNT]), %%mm2;"
			"pxor     %%mm7, %%mm0;"
			"movq     24(%[IN], %[CNT]), %%mm3;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"paddsb     (%[NOISE], %[CNT]), %%mm0;"
			"pxor     %%mm7, %%mm3;"
			"paddsb    8(%[NOISE], %[CNT]), %%mm1;"
			"paddsb   16(%[NOISE], %[CNT]), %%mm2;"
			"pxor     %%mm7, %%mm0;"
			"paddsb   24(%[NOISE], %[CNT]), %%mm3;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"movq     %%mm0,   (%[OUT], %[CNT]);"
			"pxor     %%mm7, %%mm3;"
			"movq     %%mm1,  8(%[OUT], %[CNT]);"
			"movq     %%mm2, 16(%[OUT], %[CNT]);"
			"movq     %%mm3, 24(%[OUT], %[CNT]);"
			"add      $32, %[CNT];"
			"jnz      0b;"
			"emms;"

			: [CNT]   "=r"    (dummy)
			: [IN]    "r"     (in    + width)
			, [OUT]   "r"     (out   + width)
			, [NOISE] "r"     (noise + 4 * width)
			,         "[CNT]" (-4 * width)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3", "mm7"
			#endif
		);
		return;
	}
	if ((sizeof(Pixel) == 4) && HostCPU::hasMMX()) {
		// MMX 32bpp
		assert((4 * width % 32) == 0);
		unsigned long dummy;
		asm volatile (
			"pcmpeqb  %%mm7, %%mm7;"
			"psllw    $15, %%mm7;"
			"packsswb %%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq       (%[IN], %[CNT]), %%mm0;"
			"movq      8(%[IN], %[CNT]), %%mm1;"
			"movq     16(%[IN], %[CNT]), %%mm2;"
			"pxor     %%mm7, %%mm0;"
			"movq     24(%[IN], %[CNT]), %%mm3;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"paddsb     (%[NOISE], %[CNT]), %%mm0;"
			"pxor     %%mm7, %%mm3;"
			"paddsb    8(%[NOISE], %[CNT]), %%mm1;"
			"paddsb   16(%[NOISE], %[CNT]), %%mm2;"
			"pxor     %%mm7, %%mm0;"
			"paddsb   24(%[NOISE], %[CNT]), %%mm3;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"movq     %%mm0,   (%[OUT], %[CNT]);"
			"pxor     %%mm7, %%mm3;"
			"movq     %%mm1,  8(%[OUT], %[CNT]);"
			"movq     %%mm2, 16(%[OUT], %[CNT]);"
			"movq     %%mm3, 24(%[OUT], %[CNT]);"
			"add      $32, %[CNT];"
			"jnz      0b;"
			"emms;"

			: [CNT]   "=r"    (dummy)
			: [IN]    "r"     (in    + width)
			, [OUT]   "r"     (out   + width)
			, [NOISE] "r"     (noise + 4 * width)
			,         "[CNT]" (-4 * width)
			: "memory"
			#ifdef __MMX__
			, "mm0", "mm1", "mm2", "mm3", "mm7"
			#endif
		);
		return;
	}
	#endif
	#endif

	// c++ version
	if (sizeof(Pixel) == 4) {
		// optimized version for 32bpp
		unsigned* noise4 = reinterpret_cast<unsigned*>(noise);
		for (unsigned i = 0; i < width; ++i) {
			out[i] = addNoise4(in[i], noise4[i]);
		}
	} else {
		int mr = pixelOps.getMaxRed();
		int mg = pixelOps.getMaxGreen();
		int mb = pixelOps.getMaxBlue();
		for (unsigned i = 0; i < width; ++i) {
			Pixel p = in[i];
			int r = pixelOps.red(p);
			int g = pixelOps.green(p);
			int b = pixelOps.blue(p);

			r += noise[4 * i + 0];
			g += noise[4 * i + 1];
			b += noise[4 * i + 2];

			r = std::min(std::max(r, 0), mr);
			g = std::min(std::max(g, 0), mg);
			b = std::min(std::max(b, 0), mb);

			out[i] = pixelOps.combine(r, g, b);
		}
	}
}

template <class Pixel>
void FBPostProcessor<Pixel>::drawNoise(OutputSurface& output)
{
	if (renderSettings.getNoise().getValue() == 0) return;

	unsigned height = output.getHeight();
	unsigned width = output.getWidth();
	output.lock();
	for (unsigned y = 0; y < height; ++y) {
		Pixel* buf = output.getLinePtrDirect<Pixel>(y);
		drawNoiseLine(buf, buf, &noiseBuf[noiseShift[y]], width);
	}
}

template <class Pixel>
void FBPostProcessor<Pixel>::update(const Setting& setting)
{
	VideoLayer::update(setting);
	FloatSetting& noiseSetting = renderSettings.getNoise();
	if (&setting == &noiseSetting) {
		preCalcNoise(noiseSetting.getValue());
	}
}


template <class Pixel>
FBPostProcessor<Pixel>::FBPostProcessor(MSXMotherBoard& motherBoard,
	Display& display, OutputSurface& screen_, VideoSource videoSource,
	unsigned maxWidth, unsigned height)
	: PostProcessor(
		motherBoard, display, screen_, videoSource, maxWidth, height)
	, noiseShift(screen.getHeight())
	, pixelOps(screen.getSDLFormat())
{
	scaleAlgorithm = static_cast<RenderSettings::ScaleAlgorithm>(-1); // not a valid scaler
	scaleFactor = unsigned(-1);

	FloatSetting& noiseSetting = renderSettings.getNoise();
	noiseSetting.attach(*this);
	preCalcNoise(noiseSetting.getValue());
	assert((screen.getWidth() * sizeof(Pixel)) < NOISE_SHIFT);
}

template <class Pixel>
FBPostProcessor<Pixel>::~FBPostProcessor()
{
	FloatSetting& noiseSetting = renderSettings.getNoise();
	noiseSetting.detach(*this);
}

template <class Pixel>
void FBPostProcessor<Pixel>::paint(OutputSurface& output)
{
	if (!paintFrame) return;

	// New scaler algorithm selected?
	RenderSettings::ScaleAlgorithm algo =
		renderSettings.getScaleAlgorithm().getValue();
	unsigned factor = renderSettings.getScaleFactor().getValue();
	if ((scaleAlgorithm != algo) || (scaleFactor != factor)) {
		scaleAlgorithm = algo;
		scaleFactor = factor;
		currScaler = ScalerFactory<Pixel>::createScaler(
			PixelOperations<Pixel>(output.getSDLFormat()),
			renderSettings);
	}

	// Scale image.
	const unsigned srcHeight = paintFrame->getHeight();
	const unsigned dstHeight = output.getHeight();

	unsigned g = Math::gcd(srcHeight, dstHeight);
	unsigned srcStep = srcHeight / g;
	unsigned dstStep = dstHeight / g;

	// TODO: Store all MSX lines in RawFrame and only scale the ones that fit
	//       on the PC screen, as a preparation for resizable output window.
	unsigned srcStartY = 0;
	unsigned dstStartY = 0;
	while (dstStartY < dstHeight) {
		// Currently this is true because the source frame height
		// is always >= dstHeight/(dstStep/srcStep).
		assert(srcStartY < srcHeight);

		// get region with equal lineWidth
		unsigned lineWidth = getLineWidth(paintFrame, srcStartY, srcStep);
		unsigned srcEndY = srcStartY + srcStep;
		unsigned dstEndY = dstStartY + dstStep;
		while ((srcEndY < srcHeight) && (dstEndY < dstHeight) &&
		       (getLineWidth(paintFrame, srcEndY, srcStep) == lineWidth)) {
			srcEndY += srcStep;
			dstEndY += dstStep;
		}

		// fill region
		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	srcStartY, srcEndY, lineWidth );
		output.lock();
		double horStretch = renderSettings.getHorizontalStretch().getValue();
		unsigned inWidth = unsigned(horStretch + 0.5);
		std::unique_ptr<ScalerOutput<Pixel>> dst(
			StretchScalerOutputFactory<Pixel>::create(
				output, pixelOps, inWidth));
		currScaler->scaleImage(
			*paintFrame, superImposeVideoFrame,
			srcStartY, srcEndY, lineWidth, // source
			*dst, dstStartY, dstEndY); // dest
		paintFrame->freeLineBuffers();

		// next region
		srcStartY = srcEndY;
		dstStartY = dstEndY;
	}

	drawNoise(output);

	output.flushFrameBuffer(); // for SDLGL-FBxx
}

template <class Pixel>
std::unique_ptr<RawFrame> FBPostProcessor<Pixel>::rotateFrames(
	std::unique_ptr<RawFrame> finishedFrame, FrameSource::FieldType field,
	EmuTime::param time)
{
	for (unsigned y = 0; y < screen.getHeight(); ++y) {
		noiseShift[y] = rand() & (NOISE_SHIFT - 1) & ~15;
	}

	return PostProcessor::rotateFrames(std::move(finishedFrame), field, time);
}


// Force template instantiation.
#if HAVE_16BPP
template class FBPostProcessor<word>;
#endif
#if HAVE_32BPP
template class FBPostProcessor<unsigned>;
#endif

} // namespace openmsx
