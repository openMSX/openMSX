#include "ImGuiVDPUtils.hh"

#include "ranges.hh"
#include "xrange.hh"

namespace openmsx {

void renderTextPatterns(function_ref<uint8_t(unsigned)> pat, uint32_t fg, uint32_t bg,
                        std::span<uint32_t> output)
{
	for (auto row : xrange(8)) {
		for (auto column : xrange(32)) {
			auto patNum = 32 * row + column;
			auto offset = 8 * patNum;
			for (auto y : xrange(8)) {
				auto out = subspan<6>(output, (8 * row + y) * 192 + 6 * column);
				drawPatternRow6(pat(offset + y), fg, bg, out);
			}
		}
	}
}

void renderGraphic1Patterns(function_ref<uint8_t(unsigned)> pat, function_ref<uint8_t(unsigned)> col,
                            std::span<const uint32_t, 16> palette, std::span<uint32_t> output)
{
	for (auto row : xrange(8)) {
		for (auto group : xrange(4)) { // 32 columns, split in 4 groups of 8
			auto color = col(4 * row + group);
			auto fg = palette[color >> 4];
			auto bg = palette[color & 15];
			for (auto subColumn : xrange(8)) {
				auto column = 8 * group + subColumn;
				auto patNum = 32 * row + column;
				auto offset = 8 * patNum;
				for (auto y : xrange(8)) {
					auto out = subspan<8>(output, (8 * row + y) * 256 + 8 * column);
					drawPatternRow8(pat(offset + y), fg, bg, out);
				}
			}
		}
	}
}

void renderGraphic2Patterns(function_ref<uint8_t(unsigned)> pat, function_ref<uint8_t(unsigned)> col,
                            std::span<const uint32_t, 16> palette, int banks, std::span<uint32_t> output)
{
	for (auto row : xrange(banks * 8)) {
		for (auto column : xrange(32)) {
			auto patNum = 32 * row + column;
			auto offset = 8 * patNum;
			for (auto y : xrange(8)) {
				auto color = col(offset + y);
				auto fg = palette[color >> 4];
				auto bg = palette[color & 15];
				auto out = subspan<8>(output, (8 * row + y) * 256 + 8 * column);
				drawPatternRow8(pat(offset + y), fg, bg, out);
			}
		}
	}
}

void renderMulticolorPatterns(function_ref<uint8_t(unsigned)> pat,
                              std::span<const uint32_t, 16> palette, std::span<uint32_t> output)
{
	for (auto group : xrange(4)) {
		for (auto row : xrange(8)) {
			for (auto column : xrange(32)) {
				auto patNum = 32 * row + column;
				auto offset = 8 * patNum + 2 * group;
				for (auto y : xrange(2)) {
					auto out = subspan<2>(output, (16 * group + 2 * row + y) * 64 + 2 * column);
					auto pattern = pat(offset + y);
					out[0] = palette[pattern >> 4];
					out[1] = palette[pattern & 15];
				}
			}
		}
	}
}

} // namespace openmsx
