#include "Deflicker.hh"
#include "RawFrame.hh"
#include "PixelOperations.hh"
#include "one_of.hh"
#include "unreachable.hh"
#include "vla.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <concepts>
#include <memory>
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx {

template<std::unsigned_integral Pixel> class DeflickerImpl final : public Deflicker
{
public:
	DeflickerImpl(const PixelFormat& format,
	              std::unique_ptr<RawFrame>* lastFrames);

private:
	[[nodiscard]] const void* getLineInfo(
		unsigned line, unsigned& width,
		void* buf, unsigned bufWidth) const override;

private:
	PixelOperations<Pixel> pixelOps;
};


std::unique_ptr<Deflicker> Deflicker::create(
	const PixelFormat& format,
	std::unique_ptr<RawFrame>* lastFrames)
{
#if HAVE_16BPP
	if (format.getBytesPerPixel() == 2) {
		return std::make_unique<DeflickerImpl<uint16_t>>(format, lastFrames);
	}
#endif
#if HAVE_32BPP
	if (format.getBytesPerPixel() == 4) {
		return std::make_unique<DeflickerImpl<uint32_t>>(format, lastFrames);
	}
#endif
	UNREACHABLE; return nullptr; // avoid warning
}


Deflicker::Deflicker(const PixelFormat& format,
                     std::unique_ptr<RawFrame>* lastFrames_)
	: FrameSource(format)
	, lastFrames(lastFrames_)
{
}

void Deflicker::init()
{
	FrameSource::init(FIELD_NONINTERLACED);
	setHeight(lastFrames[0]->getHeight());
}

unsigned Deflicker::getLineWidth(unsigned line) const
{
	return lastFrames[0]->getLineWidthDirect(line);
}


template<std::unsigned_integral Pixel>
DeflickerImpl<Pixel>::DeflickerImpl(const PixelFormat& format,
                                    std::unique_ptr<RawFrame>* lastFrames_)
	: Deflicker(format, lastFrames_)
	, pixelOps(format)
{
}

#ifdef __SSE2__
template<std::unsigned_integral Pixel>
static __m128i blend(__m128i x, __m128i y, Pixel blendMask)
{
	if constexpr (sizeof(Pixel) == 4) {
		// 32bpp
		return _mm_avg_epu8(x, y);
	} else {
		// 16bpp,  (x & y) + (((x ^ y) & blendMask) >> 1)
		__m128i m = _mm_set1_epi16(blendMask);
		__m128i a = _mm_and_si128(x, y);
		__m128i b = _mm_xor_si128(x, y);
		__m128i c = _mm_and_si128(b, m);
		__m128i d = _mm_srli_epi16(c, 1);
		return _mm_add_epi16(a, d);
	}
}

template<std::unsigned_integral Pixel>
static __m128i uload(const Pixel* ptr, ptrdiff_t byteOffst)
{
	const auto* p8   = reinterpret_cast<const   char *>(ptr);
	const auto* p128 = reinterpret_cast<const __m128i*>(p8 + byteOffst);
	return _mm_loadu_si128(p128);
}

template<std::unsigned_integral Pixel>
static void ustore(Pixel* ptr, ptrdiff_t byteOffst, __m128i val)
{
	auto* p8   = reinterpret_cast<  char *>(ptr);
	auto* p128 = reinterpret_cast<__m128i*>(p8 + byteOffst);
	return _mm_storeu_si128(p128, val);
}

template<std::unsigned_integral Pixel>
static __m128i compare(__m128i x, __m128i y)
{
	static_assert(sizeof(Pixel) == one_of(2u, 4u));
	if constexpr (sizeof(Pixel) == 4) {
		return _mm_cmpeq_epi32(x, y);
	} else {
		return _mm_cmpeq_epi16(x, y);
	}
}
#endif

template<std::unsigned_integral Pixel>
const void* DeflickerImpl<Pixel>::getLineInfo(
	unsigned line, unsigned& width, void* buf_, unsigned bufWidth) const
{
	unsigned width0 = lastFrames[0]->getLineWidthDirect(line);
	unsigned width1 = lastFrames[1]->getLineWidthDirect(line);
	unsigned width2 = lastFrames[2]->getLineWidthDirect(line);
	unsigned width3 = lastFrames[3]->getLineWidthDirect(line);
	const Pixel* line0 = lastFrames[0]->template getLinePtrDirect<Pixel>(line);
	const Pixel* line1 = lastFrames[1]->template getLinePtrDirect<Pixel>(line);
	const Pixel* line2 = lastFrames[2]->template getLinePtrDirect<Pixel>(line);
	const Pixel* line3 = lastFrames[3]->template getLinePtrDirect<Pixel>(line);
	if ((width0 != width3) || (width0 != width2) || (width0 != width1)) {
		// Not all the same width.
		width = width0;
		return line0;
	}

	// Prefer to write directly to the output buffer, if that's not
	// possible store the intermediate result in a temp buffer.
	VLA_SSE_ALIGNED(Pixel, buf2, width0);
	auto* buf = static_cast<Pixel*>(buf_);
	Pixel* out = (width0 <= bufWidth) ? buf : buf2;

	// Detect pixels that alternate between two different color values and
	// replace those with the average color. We search for an alternating
	// sequence with length (at least) 4. Or IOW we look for "A B A B".
	// The implementation below also detects a constant pixel value
	// "A A A A" as alternating between "A" and "A", but that's fine.
	Pixel* dst = out;
	unsigned remaining = width0;
#ifdef __SSE2__
	size_t pixelsPerSSE = sizeof(__m128i) / sizeof(Pixel);
	size_t widthSSE = remaining & ~(pixelsPerSSE - 1); // rounded down to a multiple of pixels in a SSE register
	line0 += widthSSE;
	line1 += widthSSE;
	line2 += widthSSE;
	line3 += widthSSE;
	dst   += widthSSE;
	auto byteOffst = -ptrdiff_t(widthSSE * sizeof(Pixel));

	Pixel blendMask = pixelOps.getBlendMask();
	while (byteOffst < 0) {
		__m128i a0 = uload(line0, byteOffst);
		__m128i a1 = uload(line1, byteOffst);
		__m128i a2 = uload(line2, byteOffst);
		__m128i a3 = uload(line3, byteOffst);

		__m128i e02 = compare<Pixel>(a0, a2); // a0 == a2
		__m128i e13 = compare<Pixel>(a1, a3); // a1 == a3
		__m128i cnd = _mm_and_si128(e02, e13); // (a0==a2) && (a1==a3)

		__m128i a01 = blend(a0, a1, blendMask);
		__m128i p = _mm_xor_si128(a0, a01);
		__m128i q = _mm_and_si128(p, cnd);
		__m128i r = _mm_xor_si128(q, a0); // select(a0, a01, cnd)

		ustore(dst, byteOffst, r);
		byteOffst += sizeof(__m128i);
	}
	remaining &= pixelsPerSSE - 1;
#endif
	for (auto x : xrange(remaining)) {
		dst[x] = ((line0[x] == line2[x]) && (line1[x] == line3[x]))
		       ? pixelOps.template blend<1, 1>(line0[x], line1[x])
	               : line0[x];
	}

	if (width0 <= bufWidth) {
		// It it already fits, we're done
		width = width0;
	} else {
		// Otherwise scale so that it does fit.
		width = bufWidth;
		scaleLine(out, buf, width0, bufWidth);
	}
	return buf;
}

} // namespace openmsx
