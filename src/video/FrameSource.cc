#include "FrameSource.hh"

#include "LineScalers.hh"

#include "MemoryOps.hh"
#include "aligned.hh"
#include "inplace_buffer.hh"
#include "ranges.hh"
#include "unreachable.hh"

#include <array>
#include <span>

namespace openmsx {

using Pixel = FrameSource::Pixel;

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
		blendLines(line0, line1, buf0); // possibly line0 == buf0
		return buf0;
	}
}

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
		blendLines(line0, line1, buf0); // possibly line0 == buf0
		return buf0;
	} else {
		assert(getHeight() == 240);
		auto r = getLine(line / 3, std::span<Pixel>(buf0));
		assert(r.size() == 960);
		return std::span<const Pixel, 960>(r);
	}
}

void FrameSource::scaleLine(
	std::span<const Pixel> in, std::span<Pixel> out) const
{
	inplace_buffer<Pixel, 1280> tmpBuf(uninitialized_tag{}, in.size());
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
		case    1: out[0] = in[0]; break;
		case  213: UNREACHABLE;
		case  320: scale_2on3(in, out); break;
		case  426: scale_1on2(in, out); break;
		case  640: scale_1on3(in, out); break;
		case  853: scale_1on4(in, out); break;
		case  960: scale_2on9(in, out); break;
		case 1280: scale_1on6(in, out); break;
		default: UNREACHABLE;
		}
		break;
	case 320:
		switch (out.size()) {
		case    1: out[0] = in[0]; break;
		case  213: scale_3on2(in, out); break;
		case  320: UNREACHABLE;
		case  426: scale_3on4(in, out); break;
		case  640: scale_1on2(in, out); break;
		case  853: scale_3on8(in, out); break;
		case  960: scale_1on3(in, out); break;
		case 1280: scale_1on4(in, out); break;
		default: UNREACHABLE;
		}
		break;
	case 426:
		switch (out.size()) {
		case    1: out[0] = in[0]; break;
		case  213: scale_2on1(in, out); break;
		case  320: scale_4on3(in, out); break;
		case  426: UNREACHABLE;
		case  640: scale_2on3(in, out); break;
		case  853: scale_1on2(in, out); break;
		case  960: scale_4on9(in, out); break;
		case 1280: scale_1on3(in, out); break;
		default: UNREACHABLE;
		}
		break;
	case 640:
		switch (out.size()) {
		case    1: out[0] = in[0]; break;
		case  213: scale_3on1(in, out); break;
		case  320: scale_2on1(in, out); break;
		case  426: scale_3on2(in, out); break;
		case  640: UNREACHABLE;
		case  853: scale_3on4(in, out); break;
		case  960: scale_2on3(in, out); break;
		case 1280: scale_1on2(in, out); break;
		default: UNREACHABLE;
		}
		break;
	case 853:
		switch (out.size()) {
		case    1: out[0] = in[0]; break;
		case  213: scale_4on1(in, out); break;
		case  320: scale_8on3(in, out); break;
		case  426: scale_2on1(in, out); break;
		case  640: scale_4on3(in, out); break;
		case  853: UNREACHABLE;
		case  960: scale_8on9(in, out); break;
		case 1280: scale_2on3(in, out); break;
		default: UNREACHABLE;
		}
		break;
	case 1280:
		switch (out.size()) {
		case    1: out[0] = in[0]; break;
		case  213: scale_6on1(in, out); break;
		case  320: scale_4on1(in, out); break;
		case  426: scale_3on1(in, out); break;
		case  640: scale_2on1(in, out); break;
		case  853: scale_3on2(in, out); break;
		case  960: scale_4on3(in, out); break;
		case 1280: UNREACHABLE;
		default: UNREACHABLE;
		}
		break;
	default:
		UNREACHABLE;
	}
}

} // namespace openmsx
