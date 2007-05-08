// $Id$

#include "FBPostProcessor.hh"
#include "RenderSettings.hh"
#include "Scaler.hh"
#include "ScalerFactory.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "VisibleSurface.hh"
#include "HostCPU.hh"
#include "Math.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

static const unsigned NOISE_SHIFT = 8192;
static const unsigned NOISE_BUF_SIZE = 2 * NOISE_SHIFT;
static signed char noiseBuf[NOISE_BUF_SIZE] __attribute__((__aligned__(16)));

template <class Pixel>
void FBPostProcessor<Pixel>::preCalcNoise(double factor)
{
	// for 32bpp groups of 4 consecutive noiseBuf elements (starting at
	// 4 element boundaries) must have the same value. Later optimizations
	// depend on it.
	double scaleR = pixelOps.getMaxRed() / 255.0;
	double scaleG = pixelOps.getMaxGreen() / 255.0;
	double scaleB = pixelOps.getMaxBlue() / 255.0;
	for (unsigned i = 0; i < NOISE_BUF_SIZE; i += 8) {
		double r1, r2;
		Math::gaussian2(r1, r2);
		noiseBuf[i + 0] = Math::clip<-128, 127>(r1, factor * scaleR);
		noiseBuf[i + 1] = Math::clip<-128, 127>(r1, factor * scaleG);
		noiseBuf[i + 2] = Math::clip<-128, 127>(r1, factor * scaleB);
		noiseBuf[i + 3] = Math::clip<-128, 127>(r1, factor);
		noiseBuf[i + 4] = Math::clip<-128, 127>(r2, factor * scaleR);
		noiseBuf[i + 5] = Math::clip<-128, 127>(r2, factor * scaleG);
		noiseBuf[i + 6] = Math::clip<-128, 127>(r2, factor * scaleB);
		noiseBuf[i + 7] = Math::clip<-128, 127>(r2, factor);
	}
}

template <class Pixel>
void FBPostProcessor<Pixel>::drawNoiseLine(
		Pixel* in, Pixel* out, signed char* noise, unsigned long width)
{
	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if ((sizeof(Pixel) == 4) && cpu.hasSSE2()) {
		// SSE2 32bpp
		assert(((4 * width) % 64) == 0);
		asm (
			"pcmpeqb  %%xmm7, %%xmm7;"
			"psllw    $15, %%xmm7;"
			"packsswb %%xmm7, %%xmm7;"
			".p2align 4,,15;"
		"0:"
			"movdqa     (%0, %3), %%xmm0;"
			"movdqa   16(%0, %3), %%xmm1;"
			"movdqa   32(%0, %3), %%xmm2;"
			"pxor     %%xmm7, %%xmm0;"
			"movdqa   48(%0, %3), %%xmm3;"
			"pxor     %%xmm7, %%xmm1;"
			"pxor     %%xmm7, %%xmm2;"
			"paddsb     (%2, %3), %%xmm0;"
			"pxor     %%xmm7, %%xmm3;"
			"paddsb   16(%2, %3), %%xmm1;"
			"paddsb   32(%2, %3), %%xmm2;"
			"pxor     %%xmm7, %%xmm0;"
			"paddsb   48(%2, %3), %%xmm3;"
			"pxor     %%xmm7, %%xmm1;"
			"pxor     %%xmm7, %%xmm2;"
			"movdqa   %%xmm0,   (%1, %3);"
			"pxor     %%xmm7, %%xmm3;"
			"movdqa   %%xmm1, 16(%1, %3);"
			"movdqa   %%xmm2, 32(%1, %3);"
			"movdqa   %%xmm3, 48(%1, %3);"
			"add      $64, %3;"
			"jnz      0b;"

			: // no output
			: "r" (in    + width)     // 0
			, "r" (out   + width)     // 1
			, "r" (noise + 4 * width) // 2
			, "r" (-4 * width)        // 3
			#ifdef __SSE__
			: "xmm0", "xmm1", "xmm2", "xmm3", "xmm7"
			#endif
		);
		return;
	}
	if ((sizeof(Pixel) == 4) && cpu.hasSSE()) {
		// extended-MMX 32bpp
		assert(((4 * width) % 32) == 0);
		asm (
			"pcmpeqb  %%mm7, %%mm7;"
			"psllw    $15, %%mm7;"
			"packsswb %%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"prefetchnta 320(%0, %3);"
			"movq       (%0, %3), %%mm0;"
			"movq      8(%0, %3), %%mm1;"
			"movq     16(%0, %3), %%mm2;"
			"pxor     %%mm7, %%mm0;"
			"movq     24(%0, %3), %%mm3;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"paddsb     (%2, %3), %%mm0;"
			"pxor     %%mm7, %%mm3;"
			"paddsb    8(%2, %3), %%mm1;"
			"paddsb   16(%2, %3), %%mm2;"
			"pxor     %%mm7, %%mm0;"
			"paddsb   24(%2, %3), %%mm3;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"movq     %%mm0,   (%1, %3);"
			"pxor     %%mm7, %%mm3;"
			"movq     %%mm1,  8(%1, %3);"
			"movq     %%mm2, 16(%1, %3);"
			"movq     %%mm3, 24(%1, %3);"
			"add      $32, %3;"
			"jnz      0b;"
			"emms;"

			: // no output
			: "r" (in    + width)     // 0
			, "r" (out   + width)     // 1
			, "r" (noise + 4 * width) // 2
			, "r" (-4 * width)        // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3", "mm7"
			#endif
		);
		return;
	}
	if ((sizeof(Pixel) == 4) && cpu.hasMMX()) {
		// MMX 32bpp
		assert((4 * width % 32) == 0);
		asm (
			"pcmpeqb  %%mm7, %%mm7;"
			"psllw    $15, %%mm7;"
			"packsswb %%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq       (%0, %3), %%mm0;"
			"movq      8(%0, %3), %%mm1;"
			"movq     16(%0, %3), %%mm2;"
			"pxor     %%mm7, %%mm0;"
			"movq     24(%0, %3), %%mm3;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"paddsb     (%2, %3), %%mm0;"
			"pxor     %%mm7, %%mm3;"
			"paddsb    8(%2, %3), %%mm1;"
			"paddsb   16(%2, %3), %%mm2;"
			"pxor     %%mm7, %%mm0;"
			"paddsb   24(%2, %3), %%mm3;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"movq     %%mm0,   (%1, %3);"
			"pxor     %%mm7, %%mm3;"
			"movq     %%mm1,  8(%1, %3);"
			"movq     %%mm2, 16(%1, %3);"
			"movq     %%mm3, 24(%1, %3);"
			"add      $32, %3;"
			"jnz      0b;"
			"emms;"

			: // no output
			: "r" (in    + width)     // 0
			, "r" (out   + width)     // 1
			, "r" (noise + 4 * width) // 2
			, "r" (-4 * width)        // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3", "mm7"
			#endif
		);
		return;
	}
	#endif

	// c++ version
	if (sizeof(Pixel) == 4) {
		// optimized version for 32bpp
		for (unsigned i = 0; i < width; ++i) {
			Pixel p = in[i];
			int n = noise[4 * i]; // same for all components
			// Always calculating 4 components is more portable, but doing
			// only 3 is significantly faster (~20%).
			// Typical pixel layout for little endian is ABGR and for big
			// endian is ARGB, so we can use the same computation.
			unsigned c1 = std::min<unsigned>(std::max<int>(
				(p & 0x000000FF) + (n <<  0), 0), 0x000000FF);
			unsigned c2 = std::min<unsigned>(std::max<int>(
				(p & 0x0000FF00) + (n <<  8), 0), 0x0000FF00);
			unsigned c3 = std::min<unsigned>(std::max<int>(
				(p & 0x00FF0000) + (n << 16), 0), 0x00FF0000);
			out[i] = c1 | c2 | c3;
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
void FBPostProcessor<Pixel>::drawNoise()
{
	if (renderSettings.getNoise().getValue() == 0) return;

	unsigned height = screen.getHeight();
	unsigned width = screen.getWidth();
	for (unsigned y = 0; y < height; ++y) {
		Pixel* buf = screen.getLinePtr(y, (Pixel*)0);
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
FBPostProcessor<Pixel>::FBPostProcessor(CommandController& commandController,
	Display& display, VisibleSurface& screen_, VideoSource videoSource,
	unsigned maxWidth, unsigned height)
	: PostProcessor(
		commandController, display, screen_, videoSource, maxWidth, height)
	, noiseShift(screen.getHeight())
	, pixelOps(screen.getFormat())
{
	scaleAlgorithm = (RenderSettings::ScaleAlgorithm)-1; // not a valid scaler
	scaleFactor = (unsigned)-1;

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
void FBPostProcessor<Pixel>::paint()
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
			PixelOperations<Pixel>(screen.getFormat()), renderSettings);
	}

	// Scale image.
	const unsigned srcHeight = paintFrame->getHeight();
	const unsigned dstHeight = screen.getHeight();

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
		currScaler->scaleImage(
			*paintFrame, srcStartY, srcEndY, lineWidth, // source
			screen, dstStartY, dstEndY); // dest
		paintFrame->freeLineBuffers();

		// next region
		srcStartY = srcEndY;
		dstStartY = dstEndY;
	}

	drawNoise();

	// TODO: This statement is the only reason FBPostProcessor uses "screen"
	//       as a VisibleSurface instead of as an OutputSurface.
	screen.drawFrameBuffer();
}

template <class Pixel>
RawFrame* FBPostProcessor<Pixel>::rotateFrames(
	RawFrame* finishedFrame, FrameSource::FieldType field,
	const EmuTime& time)
{
	for (unsigned y = 0; y < screen.getHeight(); ++y) {
		noiseShift[y] = rand() & (NOISE_SHIFT - 1) & ~15;
	}

	return PostProcessor::rotateFrames(finishedFrame, field, time);
}


// Force template instantiation.
template class FBPostProcessor<word>;
template class FBPostProcessor<unsigned>;

} // namespace openmsx
