#include "FBPostProcessor.hh"
#include "RawFrame.hh"
#include "StretchScalerOutput.hh"
#include "ScalerOutput.hh"
#include "RenderSettings.hh"
#include "Scaler.hh"
#include "ScalerFactory.hh"
#include "SDLOutputSurface.hh"
#include "aligned.hh"
#include "checked_cast.hh"
#include "endian.hh"
#include "random.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <numeric>
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx {

constexpr unsigned NOISE_SHIFT = 8192;
constexpr unsigned NOISE_BUF_SIZE = 2 * NOISE_SHIFT;
ALIGNAS_SSE static signed char noiseBuf[NOISE_BUF_SIZE];

template<std::unsigned_integral Pixel>
void FBPostProcessor<Pixel>::preCalcNoise(float factor)
{
	// We skip noise drawing if the factor is 0, so there is no point in
	// initializing the random data in that case.
	if (factor == 0.0f) return;

	// for 32bpp groups of 4 consecutive noiseBuf elements (starting at
	// 4 element boundaries) must have the same value. Later optimizations
	// depend on it.

	float scale[4];
	if constexpr (sizeof(Pixel) == 4) {
		// 32bpp
		// TODO ATM we compensate for big endian here. A better
		// alternative is to turn noiseBuf into an array of ints (it's
		// now bytes) and in the 16bpp code extract R,G,B components
		// from those ints
		const auto p = Pixel(Endian::BIG ? 0x00010203 : 0x03020100);
		// TODO we can also fill the array with 'factor' and only set
		// 'alpha' to 0.0. But PixelOperations doesn't offer a simple
		// way to get the position of the alpha byte (yet).
		scale[0] = scale[1] = scale[2] = scale[3] = 0.0f;
		scale[pixelOps.red  (p)] = factor;
		scale[pixelOps.green(p)] = factor;
		scale[pixelOps.blue (p)] = factor;
	} else {
		// 16bpp
		scale[0] = (pixelOps.getMaxRed()   / 255.0f) * factor;
		scale[1] = (pixelOps.getMaxGreen() / 255.0f) * factor;
		scale[2] = (pixelOps.getMaxBlue()  / 255.0f) * factor;
		scale[3] = 0.0f;
	}

	auto& generator = global_urng(); // fast (non-cryptographic) random numbers
	std::normal_distribution<float> distribution(0.0f, 1.0f);
	for (unsigned i = 0; i < NOISE_BUF_SIZE; i += 4) {
		float r = distribution(generator);
		noiseBuf[i + 0] = std::clamp(int(roundf(r * scale[0])), -128, 127);
		noiseBuf[i + 1] = std::clamp(int(roundf(r * scale[1])), -128, 127);
		noiseBuf[i + 2] = std::clamp(int(roundf(r * scale[2])), -128, 127);
		noiseBuf[i + 3] = std::clamp(int(roundf(r * scale[3])), -128, 127);
	}
}

#ifdef __SSE2__
static inline void drawNoiseLineSse2(uint32_t* buf_, signed char* noise, size_t width)
{
	// To each of the RGBA color components (a value in range [0..255]) we
	// want to add a signed noise value (in range [-128..127]) and also clip
	// the result to the range [0..255]. There is no SSE instruction that
	// directly performs this operation. But we can:
	//   - subtract 128 from the RGBA component to get a signed byte
	//   - perform the addition with signed saturation
	//   - add 128 to the result to get back to the unsigned byte range
	// For 8-bit values the following 3 expressions are equivalent:
	//   x + 128 == x - 128 == x ^ 128
	// So the expression becomes:
	//   signed_add_sat(value ^ 128, noise) ^ 128
	// The following loop does just that, though it processes 64 bytes per
	// iteration.
	ptrdiff_t x = width * sizeof(uint32_t);
	assert((x & 63) == 0);
	assert((uintptr_t(buf_) & 15) == 0);

	char* buf = reinterpret_cast<char*>(buf_)  + x;
	char* nse = reinterpret_cast<char*>(noise) + x;
	x = -x;

	__m128i b7 = _mm_set1_epi8(-128); // 0x80
	do {
		__m128i i0 = _mm_load_si128(reinterpret_cast<__m128i*>(buf + x +  0));
		__m128i i1 = _mm_load_si128(reinterpret_cast<__m128i*>(buf + x + 16));
		__m128i i2 = _mm_load_si128(reinterpret_cast<__m128i*>(buf + x + 32));
		__m128i i3 = _mm_load_si128(reinterpret_cast<__m128i*>(buf + x + 48));
		__m128i n0 = _mm_load_si128(reinterpret_cast<__m128i*>(nse + x +  0));
		__m128i n1 = _mm_load_si128(reinterpret_cast<__m128i*>(nse + x + 16));
		__m128i n2 = _mm_load_si128(reinterpret_cast<__m128i*>(nse + x + 32));
		__m128i n3 = _mm_load_si128(reinterpret_cast<__m128i*>(nse + x + 48));
		__m128i o0 = _mm_xor_si128(_mm_adds_epi8(_mm_xor_si128(i0, b7), n0), b7);
		__m128i o1 = _mm_xor_si128(_mm_adds_epi8(_mm_xor_si128(i1, b7), n1), b7);
		__m128i o2 = _mm_xor_si128(_mm_adds_epi8(_mm_xor_si128(i2, b7), n2), b7);
		__m128i o3 = _mm_xor_si128(_mm_adds_epi8(_mm_xor_si128(i3, b7), n3), b7);
		_mm_store_si128(reinterpret_cast<__m128i*>(buf + x +  0), o0);
		_mm_store_si128(reinterpret_cast<__m128i*>(buf + x + 16), o1);
		_mm_store_si128(reinterpret_cast<__m128i*>(buf + x + 32), o2);
		_mm_store_si128(reinterpret_cast<__m128i*>(buf + x + 48), o3);
		x += 4 * sizeof(__m128i);
	} while (x < 0);
}
#endif

/** Add noise to the given pixel.
 * @param p contains 4 8-bit unsigned components, so components have range [0, 255]
 * @param n contains 4 8-bit   signed components, so components have range [-128, 127]
 * @result per component result of clip<0, 255>(p + n)
 */
static constexpr uint32_t addNoise4(uint32_t p, uint32_t n)
{
	// unclipped result (lower 8 bits of each component)
	// alternative:
	//   uint32_t s20 = ((p & 0x00FF00FF) + (n & 0x00FF00FF)) & 0x00FF00FF;
	//   uint32_t s31 = ((p & 0xFF00FF00) + (n & 0xFF00FF00)) & 0xFF00FF00;
	//   uint32_t s = s20 | s31;
	uint32_t s0 = p + n;                     // carry spills to neighbors
	uint32_t ci = (p ^ n ^ s0) & 0x01010100; // carry-in bits of prev sum
	uint32_t s  = s0 - ci;                   // subtract carry bits again

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
	uint32_t t = (p ^ n) & (p ^ s) & 0x80808080;
	uint32_t u1 = t & s; // underflow   (alternative: u1 = t & n)
	// alternative1: uint32_t u2 = u1 | (u1 >> 1);
	//               uint32_t u4 = u2 | (u2 >> 2);
	//               uint32_t u8 = u4 | (u4 >> 4);
	// alternative2: uint32_t u8 = (u1 >> 7) * 0xFF;
	uint32_t u8 = (u1 << 1) - (u1 >> 7);

	uint32_t o1 = t & p; // overflow
	uint32_t o8 = (o1 << 1) - (o1 >> 7);

	// clip result
	return (s & (~u8)) | o8;
}

template<std::unsigned_integral Pixel>
void FBPostProcessor<Pixel>::drawNoiseLine(
		Pixel* buf, signed char* noise, size_t width)
{
#ifdef __SSE2__
	if constexpr (sizeof(Pixel) == 4) {
		// cast to avoid compilation error in case of 16bpp (even
		// though this code is dead in that case).
		auto* buf32 = reinterpret_cast<uint32_t*>(buf);
		drawNoiseLineSse2(buf32, noise, width);
		return;
	}
#endif
	// c++ version
	if constexpr (sizeof(Pixel) == 4) {
		// optimized version for 32bpp
		auto* noise4 = reinterpret_cast<uint32_t*>(noise);
		for (auto i : xrange(width)) {
			buf[i] = addNoise4(buf[i], noise4[i]);
		}
	} else {
		int mr = pixelOps.getMaxRed();
		int mg = pixelOps.getMaxGreen();
		int mb = pixelOps.getMaxBlue();
		for (auto i : xrange(width)) {
			Pixel p = buf[i];
			int r = pixelOps.red(p);
			int g = pixelOps.green(p);
			int b = pixelOps.blue(p);

			r += noise[4 * i + 0];
			g += noise[4 * i + 1];
			b += noise[4 * i + 2];

			r = std::clamp(r, 0, mr);
			g = std::clamp(g, 0, mg);
			b = std::clamp(b, 0, mb);

			buf[i] = pixelOps.combine(r, g, b);
		}
	}
}

template<std::unsigned_integral Pixel>
void FBPostProcessor<Pixel>::drawNoise(OutputSurface& output_)
{
	if (renderSettings.getNoise() == 0.0f) return;

	auto& output = checked_cast<SDLOutputSurface&>(output_);
	auto [w, h] = output.getLogicalSize();
	auto pixelAccess = output.getDirectPixelAccess();
	for (auto y : xrange(h)) {
		auto* buf = pixelAccess.getLinePtr<Pixel>(y);
		drawNoiseLine(buf, &noiseBuf[noiseShift[y]], w);
	}
}

template<std::unsigned_integral Pixel>
void FBPostProcessor<Pixel>::update(const Setting& setting) noexcept
{
	VideoLayer::update(setting);
	auto& noiseSetting = renderSettings.getNoiseSetting();
	if (&setting == &noiseSetting) {
		preCalcNoise(noiseSetting.getDouble());
	}
}


template<std::unsigned_integral Pixel>
FBPostProcessor<Pixel>::FBPostProcessor(MSXMotherBoard& motherBoard_,
	Display& display_, OutputSurface& screen_, const std::string& videoSource,
	unsigned maxWidth_, unsigned height_, bool canDoInterlace_)
	: PostProcessor(
		motherBoard_, display_, screen_, videoSource, maxWidth_, height_,
		canDoInterlace_)
	, scaleAlgorithm(RenderSettings::NO_SCALER)
	, scaleFactor(unsigned(-1))
	, stretchWidth(unsigned(-1))
	, noiseShift(screen.getLogicalHeight())
	, pixelOps(screen.getPixelFormat())
{
	auto& noiseSetting = renderSettings.getNoiseSetting();
	noiseSetting.attach(*this);
	preCalcNoise(noiseSetting.getDouble());
	assert((screen.getLogicalWidth() * sizeof(Pixel)) < NOISE_SHIFT);
}

template<std::unsigned_integral Pixel>
FBPostProcessor<Pixel>::~FBPostProcessor()
{
	renderSettings.getNoiseSetting().detach(*this);
}

template<std::unsigned_integral Pixel>
void FBPostProcessor<Pixel>::paint(OutputSurface& output_)
{
	auto& output = checked_cast<SDLOutputSurface&>(output_);
	if (renderSettings.getInterleaveBlackFrame()) {
		interleaveCount ^= 1;
		if (interleaveCount) {
			output.clearScreen();
			return;
		}
	}

	if (!paintFrame) return;

	// New scaler algorithm selected? Or different horizontal stretch?
	auto algo = renderSettings.getScaleAlgorithm();
	unsigned factor = renderSettings.getScaleFactor();
	unsigned inWidth = lrintf(renderSettings.getHorizontalStretch());
	if ((scaleAlgorithm != algo) || (scaleFactor != factor) ||
	    (inWidth != stretchWidth) || (lastOutput != &output)) {
		scaleAlgorithm = algo;
		scaleFactor = factor;
		stretchWidth = inWidth;
		lastOutput = &output;
		currScaler = ScalerFactory<Pixel>::createScaler(
			PixelOperations<Pixel>(output.getPixelFormat()),
			renderSettings);
		stretchScaler = StretchScalerOutputFactory<Pixel>::create(
			output, pixelOps, inWidth);
	}

	// Scale image.
	const unsigned srcHeight = paintFrame->getHeight();
	const unsigned dstHeight = output.getLogicalHeight();

	unsigned g = std::gcd(srcHeight, dstHeight);
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
		//        srcStartY, srcEndY, lineWidth);
		currScaler->scaleImage(
			*paintFrame, superImposeVideoFrame,
			srcStartY, srcEndY, lineWidth, // source
			*stretchScaler, dstStartY, dstEndY); // dest

		// next region
		srcStartY = srcEndY;
		dstStartY = dstEndY;
	}

	drawNoise(output);

	output.flushFrameBuffer();
}

template<std::unsigned_integral Pixel>
std::unique_ptr<RawFrame> FBPostProcessor<Pixel>::rotateFrames(
	std::unique_ptr<RawFrame> finishedFrame, EmuTime::param time)
{
	auto& generator = global_urng(); // fast (non-cryptographic) random numbers
	std::uniform_int_distribution<int> distribution(0, NOISE_SHIFT / 16 - 1);
	for (auto y : xrange(screen.getLogicalHeight())) {
		noiseShift[y] = distribution(generator) * 16;
	}

	return PostProcessor::rotateFrames(std::move(finishedFrame), time);
}


// Force template instantiation.
#if HAVE_16BPP
template class FBPostProcessor<uint16_t>;
#endif
#if HAVE_32BPP
template class FBPostProcessor<uint32_t>;
#endif

} // namespace openmsx
