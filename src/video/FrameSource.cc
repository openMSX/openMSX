#include "FrameSource.hh"
#include "LineScalers.hh"
#include "MemoryOps.hh"
#include "PixelOperations.hh"
#include "aligned.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "vla.hh"
#include "build-info.hh"
#include "components.hh"
#include <cstdint>
#include <span>

namespace openmsx {

FrameSource::FrameSource(const PixelFormat& format)
	: pixelFormat(format)
{
}

template<std::unsigned_integral Pixel>
const Pixel* FrameSource::getLinePtr320_240(unsigned line, Pixel* buf0) const
{
	if (getHeight() == 240) {
		return getLinePtr(line, 320, buf0);
	} else {
		assert(getHeight() == 480);
		ALIGNAS_SSE Pixel buf1[320];
		auto* line0 = getLinePtr(2 * line + 0, 320, buf0);
		auto* line1 = getLinePtr(2 * line + 1, 320, buf1);
		PixelOperations<Pixel> pixelOps(pixelFormat);
		BlendLines<Pixel> blend(pixelOps);
		blend(line0, line1, buf0, 320); // possibly line0 == buf0
		return buf0;
	}
}

template<std::unsigned_integral Pixel>
const Pixel* FrameSource::getLinePtr640_480(unsigned line, Pixel* buf) const
{
	if (getHeight() == 480) {
		return getLinePtr(line, 640, buf);
	} else {
		assert(getHeight() == 240);
		return getLinePtr(line / 2, 640, buf);
	}
}

template<std::unsigned_integral Pixel>
const Pixel* FrameSource::getLinePtr960_720(unsigned line, Pixel* buf0) const
{
	if (getHeight() == 480) {
		unsigned l2 = (2 * line) / 3;
		auto* line0 = getLinePtr(l2 + 0, 960, buf0);
		if ((line % 3) != 1) {
			return line0;
		}
		ALIGNAS_SSE Pixel buf1[960];
		auto* line1 = getLinePtr(l2 + 1, 960, buf1);
		PixelOperations<Pixel> pixelOps(pixelFormat);
		BlendLines<Pixel> blend(pixelOps);
		blend(line0, line1, buf0, 960); // possibly line0 == buf0
		return buf0;
	} else {
		assert(getHeight() == 240);
		return getLinePtr(line / 3, 960, buf0);
	}
}

template<std::unsigned_integral Pixel>
void FrameSource::scaleLine(
	const Pixel* in, Pixel* out,
	unsigned inWidth, unsigned outWidth) const
{
	PixelOperations<Pixel> pixelOps(pixelFormat);

	VLA_SSE_ALIGNED(Pixel, tmpBuf, inWidth);
	if (in == out) [[unlikely]] {
		// Only happens in case getLineInfo() already used buf.
		// E.g. when a line of a SuperImposedFrame also needs to be
		// scaled.
		// TODO If the LineScaler routines can work in-place then this
		//      copy can be avoided.
		ranges::copy(std::span{in, inWidth}, tmpBuf);
		in = tmpBuf;
	}

	// TODO is there a better way to implement this?
	switch (inWidth) {
	case 1:  // blank
		MemoryOps::MemSet<Pixel> memset;
		memset(out, outWidth, in[0]);
		break;
	case 213:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213:
			UNREACHABLE;
		case 320: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 426: {
			Scale_1on2<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_1on3<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		case 853: {
			Scale_1on4<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_2on9<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 1280: {
			Scale_1on6<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		default:
			UNREACHABLE;
		}
		break;
	case 320:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_3on2<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 320:
			UNREACHABLE;
		case 426: {
			Scale_3on4<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_1on2<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		case 853: {
			Scale_3on8<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_1on3<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		case 1280: {
			Scale_1on4<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		default:
			UNREACHABLE;
		}
		break;
	case 426:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 320: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 426:
			UNREACHABLE;
		case 640: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 853: {
			Scale_1on2<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_4on9<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 1280: {
			Scale_1on3<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		default:
			UNREACHABLE;
		}
		break;
	case 640:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_3on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 320: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 426: {
			Scale_3on2<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640:
			UNREACHABLE;
		case 853: {
			Scale_3on4<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 1280: {
			Scale_1on2<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		default:
			UNREACHABLE;
		}
		break;
	case 853:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_4on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 320: {
			Scale_8on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 426: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 853:
			UNREACHABLE;
		case 960: {
			Scale_8on9<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 1280: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		default:
			UNREACHABLE;
		}
		break;
	case 1280:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_6on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 320: {
			Scale_4on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 426: {
			Scale_3on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 853: {
			Scale_3on2<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 1280:
			UNREACHABLE;
		default:
			UNREACHABLE;
		}
		break;
	default:
		UNREACHABLE;
	}
}


// Force template method instantiation
#if HAVE_16BPP
template const uint16_t* FrameSource::getLinePtr320_240<uint16_t>(unsigned, uint16_t*) const;
template const uint16_t* FrameSource::getLinePtr640_480<uint16_t>(unsigned, uint16_t*) const;
template const uint16_t* FrameSource::getLinePtr960_720<uint16_t>(unsigned, uint16_t*) const;
template void FrameSource::scaleLine<uint16_t>(const uint16_t*, uint16_t*, unsigned, unsigned) const;
#endif
#if HAVE_32BPP || COMPONENT_GL
template const uint32_t* FrameSource::getLinePtr320_240<uint32_t>(unsigned, uint32_t*) const;
template const uint32_t* FrameSource::getLinePtr640_480<uint32_t>(unsigned, uint32_t*) const;
template const uint32_t* FrameSource::getLinePtr960_720<uint32_t>(unsigned, uint32_t*) const;
template void FrameSource::scaleLine<uint32_t>(const uint32_t*, uint32_t*, unsigned, unsigned) const;
#endif

} // namespace openmsx
