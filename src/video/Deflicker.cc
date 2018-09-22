#include "Deflicker.hh"
#include "RawFrame.hh"
#include "PixelOperations.hh"
#include "unreachable.hh"
#include "vla.hh"
#include "build-info.hh"
#include <cassert>
#include <memory>
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx {

template<typename Pixel> class DeflickerImpl final : public Deflicker
{
public:
	DeflickerImpl(const SDL_PixelFormat& format,
	              std::unique_ptr<RawFrame>* lastFrames);

private:
	const void* getLineInfo(
		unsigned line, unsigned& width,
		void* buf, unsigned bufWidth) const override;

	PixelOperations<Pixel> pixelOps;
};


std::unique_ptr<Deflicker> Deflicker::create(
	const SDL_PixelFormat& format,
        std::unique_ptr<RawFrame>* lastFrames)
{
#if HAVE_16BPP
	if (format.BitsPerPixel == 15 || format.BitsPerPixel == 16) {
		return std::make_unique<DeflickerImpl<uint16_t>>(format, lastFrames);
	}
#endif
#if HAVE_32BPP
	if (format.BitsPerPixel == 32) {
		return std::make_unique<DeflickerImpl<uint32_t>>(format, lastFrames);
	}
#endif
	UNREACHABLE; return nullptr; // avoid warning
}


Deflicker::Deflicker(const SDL_PixelFormat& format,
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


template<typename Pixel>
DeflickerImpl<Pixel>::DeflickerImpl(const SDL_PixelFormat& format,
                                    std::unique_ptr<RawFrame>* lastFrames_)
	: Deflicker(format, lastFrames_)
	, pixelOps(format)
{
}

#ifdef __SSE2__
template<typename Pixel>
static inline __m128i blend(__m128i x, __m128i y, Pixel mask)
{
	if (sizeof(Pixel) == 4) {
		// 32bpp
		return _mm_avg_epu8(x, y);
	} else {
		// 16bpp,  (x & y) + (((x ^ y) & mask) >> 1)
		__m128i m = _mm_set1_epi16(mask);
		__m128i a = _mm_and_si128(x, y);
		__m128i b = _mm_xor_si128(x, y);
		__m128i c = _mm_and_si128(b, m);
		__m128i d = _mm_srli_epi16(c, 1);
		return _mm_add_epi16(a, d);
	}
}
#endif

template<typename Pixel>
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
#ifdef __SSE2__
	if (width0 != 1) {
		size_t numBytes = width0 * sizeof(Pixel);
		assert((numBytes % sizeof(__m128i)) == 0);
		assert(numBytes != 0);

		assert((reinterpret_cast<size_t>(line0) % sizeof(__m128i)) == 0);
		assert((reinterpret_cast<size_t>(line1) % sizeof(__m128i)) == 0);
		assert((reinterpret_cast<size_t>(line2) % sizeof(__m128i)) == 0);
		assert((reinterpret_cast<size_t>(line3) % sizeof(__m128i)) == 0);
		assert((reinterpret_cast<size_t>(out  ) % sizeof(__m128i)) == 0);
		auto* in0 = reinterpret_cast<const char*>(line0) + numBytes;
		auto* in1 = reinterpret_cast<const char*>(line1) + numBytes;
		auto* in2 = reinterpret_cast<const char*>(line2) + numBytes;
		auto* in3 = reinterpret_cast<const char*>(line3) + numBytes;
		auto* dst = reinterpret_cast<      char*>(out  ) + numBytes;

		Pixel mask = pixelOps.getBlendMask();
		auto x = -ptrdiff_t(numBytes);
		do {
			__m128i a0 = _mm_load_si128(reinterpret_cast<const __m128i*>(in0 + x));
			__m128i a1 = _mm_load_si128(reinterpret_cast<const __m128i*>(in1 + x));
			__m128i a2 = _mm_load_si128(reinterpret_cast<const __m128i*>(in2 + x));
			__m128i a3 = _mm_load_si128(reinterpret_cast<const __m128i*>(in3 + x));

			__m128i e02 = _mm_cmpeq_epi32(a0, a2); // a0 == a2
			__m128i e13 = _mm_cmpeq_epi32(a1, a3); // a1 == a3
			__m128i cnd = _mm_and_si128(e02, e13); // (a0==a2) && (a1==a3)

			__m128i a01 = blend(a0, a1, mask);
			__m128i p = _mm_xor_si128(a0, a01);
			__m128i q = _mm_and_si128(p, cnd);
			__m128i r = _mm_xor_si128(q, a0); // select(a0, a01, cnd)

			_mm_store_si128(reinterpret_cast<__m128i*>(dst + x), r);
			x += sizeof(__m128i);
		} while (x < 0);
		goto end;
	}
#endif
	for (unsigned x = 0; x < width0; ++x) {
		out[x] = ((line0[x] == line2[x]) && (line1[x] == line3[x]))
		       ? pixelOps.template blend<1, 1>(line0[x], line1[x])
	               : line0[x];
	}
#ifdef __SSE2__
end:
#endif
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
