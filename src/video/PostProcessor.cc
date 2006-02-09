// $Id$

#include "PostProcessor.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "Scaler.hh"
#include "ScalerFactory.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "VisibleSurface.hh"
#include "DeinterlacedFrame.hh"
#include "DoubledFrame.hh"
#include "RawFrame.hh"
#include "PixelOperations.hh"
#include <cassert>

#include "HostCPU.hh"
#include <cstdlib>
#include <cmath>

namespace openmsx {

typedef unsigned char uint8;
typedef signed   char sint8;

static const unsigned NOISE_SHIFT = 8192;
static const unsigned NOISE_BUF_SIZE = 2 * NOISE_SHIFT;
static sint8 noiseBuf[NOISE_BUF_SIZE];

static void gaussian2(double& r1, double& r2)
{
	static const double S = 2.0 / RAND_MAX;
	double x1, x2, w;
	do {
		x1 = S * rand() - 1.0;
		x2 = S * rand() - 1.0;
		w = x1 * x1 + x2 * x2;
	} while (w >= 1.0);
	w = sqrt((-2.0 * log(w)) / w);
	r1 = x1 * w;
	r2 = x2 * w;
}
static sint8 clip(double r, double factor)
{
	int a = (int)round(r * factor);
	if (a < -128) a = -128;
	else if (a > 127) a = 127;
	return a;
}
static void preCalcNoise(double factor)
{
	for (unsigned i = 0; i < NOISE_BUF_SIZE; i += 8) {
		double r1, r2;
		gaussian2(r1, r2);
		sint8 n1 = clip(r1, factor);
		noiseBuf[i + 0] = n1;
		noiseBuf[i + 1] = n1;
		noiseBuf[i + 2] = n1;
		noiseBuf[i + 3] = n1;
		sint8 n2 = clip(r1, factor);
		noiseBuf[i + 4] = n2;
		noiseBuf[i + 5] = n2;
		noiseBuf[i + 6] = n2;
		noiseBuf[i + 7] = n2;
	}
}

static void drawNoiseLine(uint8* in, uint8* out, sint8* noise, unsigned width)
{
	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if (cpu.hasMMXEXT()) {
		// extended-MMX 32bpp
		assert((width % 32) == 0);
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
			"movq     24(%0, %3), %%mm3;"
			"pxor     %%mm7, %%mm0;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"pxor     %%mm7, %%mm3;"
			"paddsb     (%2, %3), %%mm0;"
			"paddsb    8(%2, %3), %%mm1;"
			"paddsb   16(%2, %3), %%mm2;"
			"paddsb   24(%2, %3), %%mm3;"
			"pxor     %%mm7, %%mm0;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"pxor     %%mm7, %%mm3;"
			"movq     %%mm0,   (%1, %3);"
			"movq     %%mm1,  8(%1, %3);"
			"movq     %%mm2, 16(%1, %3);"
			"movq     %%mm3, 24(%1, %3);"
			"addl     $32, %3;"
			"jnz      0b;"
			"emms;"

			: // no output
			: "r" (in    + width) // 0
			, "r" (out   + width) // 1
			, "r" (noise + width) // 2
			, "r" (-width)        // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3", "mm7"
			#endif
		);
		return;
	}
	if (cpu.hasMMX()) {
		// MMX 32bpp
		assert((width % 32) == 0);
		asm (
			"pcmpeqb  %%mm7, %%mm7;"
			"psllw    $15, %%mm7;"
			"packsswb %%mm7, %%mm7;"
			".p2align 4,,15;"
		"0:"
			"movq       (%0, %3), %%mm0;"
			"movq      8(%0, %3), %%mm1;"
			"movq     16(%0, %3), %%mm2;"
			"movq     24(%0, %3), %%mm3;"
			"pxor     %%mm7, %%mm0;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"pxor     %%mm7, %%mm3;"
			"paddsb     (%2, %3), %%mm0;"
			"paddsb    8(%2, %3), %%mm1;"
			"paddsb   16(%2, %3), %%mm2;"
			"paddsb   24(%2, %3), %%mm3;"
			"pxor     %%mm7, %%mm0;"
			"pxor     %%mm7, %%mm1;"
			"pxor     %%mm7, %%mm2;"
			"pxor     %%mm7, %%mm3;"
			"movq     %%mm0,   (%1, %3);"
			"movq     %%mm1,  8(%1, %3);"
			"movq     %%mm2, 16(%1, %3);"
			"movq     %%mm3, 24(%1, %3);"
			"addl     $32, %3;"
			"jnz      0b;"
			"emms;"

			: // no output
			: "r" (in    + width) // 0
			, "r" (out   + width) // 1
			, "r" (noise + width) // 2
			, "r" (-width)        // 3
			#ifdef __MMX__
			: "mm0", "mm1", "mm2", "mm3", "mm7"
			#endif
		);
		return;
	}
	#endif

	// c++ version
	for (unsigned i = 0; i < width; ++i) {
		int t = in[i] + noise[i];
		if (t > 255) t = 255;
		else if (t < 0) t = 0;
		out[i] = t;
	}
}

template <class Pixel>
void PostProcessor<Pixel>::drawNoise()
{
	if (sizeof(Pixel) != 4) return; // TODO only 32bpp atm

	if (renderSettings.getNoise().getValue() == 0) return;

	unsigned height = screen.getHeight();
	unsigned width = screen.getWidth() * sizeof(Pixel);
	for (unsigned y = 0; y < height; ++y) {
		uint8* dummy = 0;
		uint8* buf = screen.getLinePtr(y, dummy);
		drawNoiseLine(buf, buf, &noiseBuf[noiseShift[y]], width);
	}
}

template <class Pixel>
void PostProcessor<Pixel>::update(const Setting& setting)
{
	VideoLayer::update(setting);
	FloatSetting& noiseSetting = renderSettings.getNoise();
	if (&setting == &noiseSetting) {
		preCalcNoise(noiseSetting.getValue());
	}
}


template <class Pixel>
PostProcessor<Pixel>::PostProcessor(CommandController& commandController,
	Display& display, VisibleSurface& screen_, VideoSource videoSource,
	unsigned maxWidth, unsigned height)
	: VideoLayer(videoSource, commandController, display)
	, renderSettings(display.getRenderSettings())
	, screen(screen_)
	, noiseShift(screen.getHeight())
{
	scaleAlgorithm = (RenderSettings::ScaleAlgorithm)-1; // not a valid scaler
	scaleFactor = (unsigned)-1;

	currFrame = new RawFrame(screen.getFormat(), maxWidth, height);
	prevFrame = new RawFrame(screen.getFormat(), maxWidth, height);
	deinterlacedFrame = new DeinterlacedFrame(screen.getFormat());
	interlacedFrame   = new DoubledFrame     (screen.getFormat());

	FloatSetting& noiseSetting = renderSettings.getNoise();
	noiseSetting.attach(*this);
	preCalcNoise(noiseSetting.getValue());
	assert((screen.getWidth() * sizeof(Pixel)) < NOISE_SHIFT);
}

template <class Pixel>
PostProcessor<Pixel>::~PostProcessor()
{
	FloatSetting& noiseSetting = renderSettings.getNoise();
	noiseSetting.detach(*this);

	delete currFrame;
	delete prevFrame;
	delete deinterlacedFrame;
	delete interlacedFrame;
}

/** Calculate greatest common divider of two strictly positive integers.
  * Classical implementation is like this:
  *    while (unsigned t = b % a) { b = a; a = t; }
  *    return a;
  * The following implementation avoids the costly modulo operation. It
  * is about 40% faster on my machine.
  *
  * require: a != 0  &&  b != 0
  */
static inline unsigned gcd(unsigned a, unsigned b)
{
	unsigned k = 0;
	while (((a & 1) == 0) && ((b & 1) == 0)) {
		a >>= 1; b >>= 1; ++k;
	}

	// either a or b (or both) is odd
	while ((a & 1) == 0) a >>= 1;
	while ((b & 1) == 0) b >>= 1;

	// both a and b odd
	while (a != b) {
		if (a >= b) {
			a -= b;
			do { a >>= 1; } while ((a & 1) == 0);
		} else {
			b -= a;
			do { b >>= 1; } while ((b & 1) == 0);
		}
	}
	return b << k;
}

static unsigned getLineWidth(FrameSource* frame, unsigned y, unsigned step)
{
	unsigned result = frame->getLineWidth(y);
	for (unsigned i = 1; i < step; ++i) {
		result = std::max(result, frame->getLineWidth(y + i));
	}
	return result;
}

template <class Pixel>
void PostProcessor<Pixel>::paint()
{
	// TODO: When frames are being skipped or if (de)interlace was just
	//       turned on, it's not guaranteed that prevFrame is a
	//       different field from currFrame.
	//       Or in the case of frame skip, it might be the right field,
	//       but from several frames ago.
	FrameSource* frame;
	FrameSource::FieldType field = currFrame->getField();
	if (field != FrameSource::FIELD_NONINTERLACED) {
		if (renderSettings.getDeinterlace().getValue()) {
			// deinterlaced
			if (field == FrameSource::FIELD_ODD) {
				deinterlacedFrame->init(prevFrame, currFrame);
			} else {
				deinterlacedFrame->init(currFrame, prevFrame);
			}
			frame = deinterlacedFrame;
		} else {
			// interlaced
			interlacedFrame->init(currFrame,
				(field == FrameSource::FIELD_ODD) ? 1 : 0);
			frame = interlacedFrame;
		}
	} else {
		// non interlaced
		frame = currFrame;
	}

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
	const unsigned srcHeight = frame->getHeight();
	const unsigned dstHeight = screen.getHeight();

	unsigned g = gcd(srcHeight, dstHeight);
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
		unsigned lineWidth = getLineWidth(frame, srcStartY, srcStep);
		unsigned srcEndY = srcStartY + srcStep;
		unsigned dstEndY = dstStartY + dstStep;
		while ((srcEndY < srcHeight) && (dstEndY < dstHeight) &&
		       (getLineWidth(frame, srcEndY, srcStep) == lineWidth)) {
			srcEndY += srcStep;
			dstEndY += dstStep;
		}

		// fill region
		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	srcStartY, srcEndY, lineWidth );
		currScaler->scaleImage(
			*frame, srcStartY, srcEndY, lineWidth, // source
			screen, dstStartY, dstEndY); // dest
		frame->freeLineBuffers();

		// next region
		srcStartY = srcEndY;
		dstStartY = dstEndY;
	}

	drawNoise();

	// TODO: This statement is the only reason PostProcessor uses "screen"
	//       as a VisibleSurface instead of as an OutputSurface.
	screen.drawFrameBuffer();
}

template <class Pixel>
const std::string& PostProcessor<Pixel>::getName()
{
	static const std::string V99x8_NAME = "V99x8 PostProcessor";
	static const std::string V9990_NAME = "V9990 PostProcessor";
	return (getVideoSource() == VIDEO_GFX9000) ? V9990_NAME : V99x8_NAME;
}

template <class Pixel>
RawFrame* PostProcessor<Pixel>::rotateFrames(
	RawFrame* finishedFrame, FrameSource::FieldType field)
{
	for (unsigned y = 0; y < screen.getHeight(); ++y) {
		noiseShift[y] = rand() & (NOISE_SHIFT - 1) & ~7;
	}
	
	RawFrame* reuseFrame = prevFrame;
	prevFrame = currFrame;
	currFrame = finishedFrame;
	reuseFrame->init(field);
	return reuseFrame;
}


// Force template instantiation.
template class PostProcessor<word>;
template class PostProcessor<unsigned>;

} // namespace openmsx
