#include "Deflicker.hh"

#include "PixelOperations.hh"
#include "RawFrame.hh"

#include "inplace_buffer.hh"
#include "xrange.hh"

#include <bit>
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx {

using Pixel = uint32_t;

Deflicker::Deflicker(std::span<std::unique_ptr<RawFrame>, 4> lastFrames_)
	: lastFrames(lastFrames_)
{
}

Deflicker::~Deflicker() = default;

void Deflicker::init()
{
	FrameSource::init(FieldType::NONINTERLACED);
	setHeight(lastFrames[0]->getHeight());
}

unsigned Deflicker::getLineWidth(unsigned line) const
{
	return lastFrames[0]->getLineWidthDirect(line);
}

#ifdef __SSE2__
static __m128i blend(__m128i x, __m128i y)
{
	// 32bpp
	return _mm_avg_epu8(x, y);
}

static __m128i uload(const Pixel* ptr, ptrdiff_t byteOffst)
{
	const auto* p8   = std::bit_cast<const   char *>(ptr);
	const auto* p128 = std::bit_cast<const __m128i*>(p8 + byteOffst);
	return _mm_loadu_si128(p128);
}

static void ustore(Pixel* ptr, ptrdiff_t byteOffst, __m128i val)
{
	auto* p8   = std::bit_cast<  char *>(ptr);
	auto* p128 = std::bit_cast<__m128i*>(p8 + byteOffst);
	return _mm_storeu_si128(p128, val);
}

static __m128i compare(__m128i x, __m128i y)
{
	// 32bpp
	return _mm_cmpeq_epi32(x, y);
}
#endif

std::span<const Pixel> Deflicker::getUnscaledLine(
	unsigned line, std::span<Pixel> helpBuf) const
{
	unsigned width0 = lastFrames[0]->getLineWidthDirect(line);
	unsigned width1 = lastFrames[1]->getLineWidthDirect(line);
	unsigned width2 = lastFrames[2]->getLineWidthDirect(line);
	unsigned width3 = lastFrames[3]->getLineWidthDirect(line);
	const Pixel* line0 = lastFrames[0]->getLineDirect(line).data();
	const Pixel* line1 = lastFrames[1]->getLineDirect(line).data();
	const Pixel* line2 = lastFrames[2]->getLineDirect(line).data();
	const Pixel* line3 = lastFrames[3]->getLineDirect(line).data();
	if ((width0 != width3) || (width0 != width2) || (width0 != width1)) {
		// Not all the same width.
		return std::span{line0, width0};
	}

	// Prefer to write directly to the output buffer, if that's not
	// possible store the intermediate result in a temp buffer.
	inplace_buffer<Pixel, 1280> buf2(uninitialized_tag{}, width0);
	auto* buf = helpBuf.data();
	Pixel* out = (width0 <= helpBuf.size()) ? buf : buf2.data();

	// Detect pixels that alternate between two different color values and
	// replace those with the average color. We search for an alternating
	// sequence with length (at least) 4. Or IOW we look for "A B A B".
	// The implementation below also detects a constant pixel value
	// "A A A A" as alternating between "A" and "A", but that's fine.
	Pixel* dst = out;
	size_t remaining = width0;
#ifdef __SSE2__
	size_t pixelsPerSSE = sizeof(__m128i) / sizeof(Pixel);
	size_t widthSSE = remaining & ~(pixelsPerSSE - 1); // rounded down to a multiple of pixels in a SSE register
	line0 += widthSSE;
	line1 += widthSSE;
	line2 += widthSSE;
	line3 += widthSSE;
	dst   += widthSSE;
	auto byteOffst = -ptrdiff_t(widthSSE * sizeof(Pixel));

	while (byteOffst < 0) {
		__m128i a0 = uload(line0, byteOffst);
		__m128i a1 = uload(line1, byteOffst);
		__m128i a2 = uload(line2, byteOffst);
		__m128i a3 = uload(line3, byteOffst);

		__m128i e02 = compare(a0, a2); // a0 == a2
		__m128i e13 = compare(a1, a3); // a1 == a3
		__m128i cnd = _mm_and_si128(e02, e13); // (a0==a2) && (a1==a3)

		__m128i a01 = blend(a0, a1);
		__m128i p = _mm_xor_si128(a0, a01);
		__m128i q = _mm_and_si128(p, cnd);
		__m128i r = _mm_xor_si128(q, a0); // select(a0, a01, cnd)

		ustore(dst, byteOffst, r);
		byteOffst += sizeof(__m128i);
	}
	remaining &= pixelsPerSSE - 1;
#endif
	PixelOperations pixelOps;
	for (auto x : xrange(remaining)) {
		dst[x] = ((line0[x] == line2[x]) && (line1[x] == line3[x]))
		       ? pixelOps.template blend<1, 1>(line0[x], line1[x])
	               : line0[x];
	}

	if (width0 <= helpBuf.size()) {
		// If it already fits, we're done
		return std::span{buf, width0};
	} else {
		// Otherwise scale so that it does fit.
		scaleLine(std::span{out, width0}, helpBuf);
		return helpBuf;
	}
}

} // namespace openmsx
