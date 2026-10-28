#include "ImGuiVDPUtils.hh"

#include "ImGuiUtils.hh"

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

void renderSpritePatterns8(function_ref<uint8_t(unsigned)> pat, std::span<uint32_t> output)
{
	auto zero = getColor(imColor::TRANSPARENT);
	auto one  = getColor(imColor::TEXT);
	for (auto row : xrange(8)) {
		for (auto column : xrange(32)) {
			auto patNum = 32 * row + column;
			auto offset = 8 * patNum;
			for (auto y : xrange(8)) {
				auto pattern = pat(offset + y);
				auto out = subspan<8>(output, (8 * row + y) * 256 + 8 * column);
				drawPatternRow8(pattern, one, zero, out);
			}
		}
	}
}

void renderSpritePatterns16(function_ref<uint8_t(unsigned)> pat, std::span<uint32_t> output)
{
	auto zero = getColor(imColor::TRANSPARENT);
	auto one  = getColor(imColor::TEXT);
	for (auto row : xrange(4)) {
		for (auto column : xrange(16)) {
			auto patNum = 4 * (16 * row + column);
			auto offset = 8 * patNum;
			for (auto y : xrange(16)) {
				auto patternA = pat(offset + y +  0);
				auto patternB = pat(offset + y + 16);
				auto outA = subspan<8>(output, (16 * row + y) * 256 + 16 * column +  0);
				auto outB = subspan<8>(output, (16 * row + y) * 256 + 16 * column +  8);
				drawPatternRow8(patternA, one, zero, outA);
				drawPatternRow8(patternB, one, zero, outB);
			}
		}
	}
}

std::pair<gl::vec2, gl::vec2> spritePatternUV(unsigned pattern, int size)
{
	auto cells = spritePatternCells(size);
	auto cell = (size == 16) ? (pattern / 4) : pattern;
	auto perRow = unsigned(cells.x);
	auto recipCells = recip(cells);
	gl::vec2 uv1 = gl::vec2{float(cell % perRow), float(cell / perRow)} * recipCells;
	return {uv1, uv1 + recipCells};
}

} // namespace openmsx
