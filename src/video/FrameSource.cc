#include "FrameSource.hh"
#include "LineScalers.hh"
#include "MemoryOps.hh"
#include "PixelOperations.hh"
#include "aligned.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "vla.hh"
#include <array>
#include <cstdint>
#include <span>

namespace openmsx {

FrameSource::FrameSource(const PixelFormat& format)
	: pixelFormat(format)
{
}

template<std::unsigned_integral Pixel>
std::span<const Pixel, 320> FrameSource::getLinePtr320_240(unsigned line, std::span<Pixel, 320> buf0) const
{
	if (getHeight() == 240) {
		auto r = getLine(line, std::span<Pixel>{buf0});
		assert(r.size() == 320);
		return std::span<const Pixel, 320>(r);
	} else {
		assert(getHeight() == 480);
		ALIGNAS_SSE std::array<Pixel, 320> buf1;
		auto line0 = getLine(2 * line + 0, std::span<Pixel>(buf0));
		auto line1 = getLine(2 * line + 1, std::span<Pixel>(buf1));
		PixelOperations<Pixel> pixelOps(pixelFormat);
		BlendLines<Pixel> blend(pixelOps);
		blend(line0, line1, buf0); // possibly line0 == buf0
		return buf0;
	}
}

template<std::unsigned_integral Pixel>
std::span<const Pixel, 640> FrameSource::getLinePtr640_480(unsigned line, std::span<Pixel, 640> buf) const
{
	if (getHeight() == 480) {
		auto r = getLine(line, std::span<Pixel>(buf));
		assert(r.size() == 640);
		return std::span<const Pixel, 640>(r);
	} else {
		assert(getHeight() == 240);
		auto r = getLine(line / 2, std::span<Pixel>(buf));
		assert(r.size() == 640);
		return std::span<const Pixel, 640>(r);
	}
}

template<std::unsigned_integral Pixel>
std::span<const Pixel, 960> FrameSource::getLinePtr960_720(unsigned line, std::span<Pixel, 960> buf0) const
{
	if (getHeight() == 480) {
		unsigned l2 = (2 * line) / 3;
		auto line0 = getLine(l2 + 0, std::span<Pixel>(buf0));
		if ((line % 3) != 1) {
			assert(line0.size() == 960);
			return std::span<const Pixel, 960>(line0);
		}
		ALIGNAS_SSE std::array<Pixel, 960> buf1;
		auto line1 = getLine(l2 + 1, std::span<Pixel>(buf1));
		PixelOperations<Pixel> pixelOps(pixelFormat);
		BlendLines<Pixel> blend(pixelOps);
		blend(line0, line1, buf0); // possibly line0 == buf0
		return buf0;
	} else {
		assert(getHeight() == 240);
		auto r = getLine(line / 3, std::span<Pixel>(buf0));
		assert(r.size() == 960);
		return std::span<const Pixel, 960>(r);
	}
}

template<std::unsigned_integral Pixel>
void FrameSource::scaleLine(
	std::span<const Pixel> in, std::span<Pixel> out) const
{
	PixelOperations<Pixel> pixelOps(pixelFormat);

	VLA_SSE_ALIGNED(Pixel, tmpBuf, in.size());
	if (in.data() == out.data()) [[unlikely]] {
		// Only happens in case getLineInfo() already used buf.
		// E.g. when a line of a SuperImposedFrame also needs to be
		// scaled.
		// TODO If the LineScaler routines can work in-place then this
		//      copy can be avoided.
		ranges::copy(in, tmpBuf);
		in = tmpBuf;
	}

	// TODO is there a better way to implement this?
	switch (in.size()) {
	case 1:  // blank
		MemoryOps::MemSet<Pixel> memset;
		memset(out, in[0]);
		break;
	case 213:
		switch (out.size()) {
		case 1:
			out[0] = in[0];
			break;
		case 213:
			UNREACHABLE;
		case 320: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 426: {
			Scale_1on2<Pixel> scale;
			scale(in, out);
			break;
		}
		case 640: {
			Scale_1on3<Pixel> scale;
			scale(in, out);
			break;
		}
		case 853: {
			Scale_1on4<Pixel> scale;
			scale(in, out);
			break;
		}
		case 960: {
			Scale_2on9<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 1280: {
			Scale_1on6<Pixel> scale;
			scale(in, out);
			break;
		}
		default:
			UNREACHABLE;
		}
		break;
	case 320:
		switch (out.size()) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_3on2<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 320:
			UNREACHABLE;
		case 426: {
			Scale_3on4<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 640: {
			Scale_1on2<Pixel> scale;
			scale(in, out);
			break;
		}
		case 853: {
			Scale_3on8<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 960: {
			Scale_1on3<Pixel> scale;
			scale(in, out);
			break;
		}
		case 1280: {
			Scale_1on4<Pixel> scale;
			scale(in, out);
			break;
		}
		default:
			UNREACHABLE;
		}
		break;
	case 426:
		switch (out.size()) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 320: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 426:
			UNREACHABLE;
		case 640: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 853: {
			Scale_1on2<Pixel> scale;
			scale(in, out);
			break;
		}
		case 960: {
			Scale_4on9<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 1280: {
			Scale_1on3<Pixel> scale;
			scale(in, out);
			break;
		}
		default:
			UNREACHABLE;
		}
		break;
	case 640:
		switch (out.size()) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_3on1<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 320: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 426: {
			Scale_3on2<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 640:
			UNREACHABLE;
		case 853: {
			Scale_3on4<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 960: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 1280: {
			Scale_1on2<Pixel> scale;
			scale(in, out);
			break;
		}
		default:
			UNREACHABLE;
		}
		break;
	case 853:
		switch (out.size()) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_4on1<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 320: {
			Scale_8on3<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 426: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 640: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 853:
			UNREACHABLE;
		case 960: {
			Scale_8on9<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 1280: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		default:
			UNREACHABLE;
		}
		break;
	case 1280:
		switch (out.size()) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_6on1<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 320: {
			Scale_4on1<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 426: {
			Scale_3on1<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 640: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 853: {
			Scale_3on2<Pixel> scale(pixelOps);
			scale(in, out);
			break;
		}
		case 960: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out);
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
template std::span<const uint32_t, 320> FrameSource::getLinePtr320_240<uint32_t>(unsigned, std::span<uint32_t, 320>) const;
template std::span<const uint32_t, 640> FrameSource::getLinePtr640_480<uint32_t>(unsigned, std::span<uint32_t, 640>) const;
template std::span<const uint32_t, 960> FrameSource::getLinePtr960_720<uint32_t>(unsigned, std::span<uint32_t, 960>) const;
template void FrameSource::scaleLine<uint32_t>(std::span<const uint32_t>, std::span<uint32_t>) const;

} // namespace openmsx
