#include "ImGuiVDPUtils.hh"

#include "ImGuiUtils.hh"

#include "ranges.hh"
#include "xrange.hh"

#include <imgui.h>

#include <algorithm>
#include <string_view>

namespace openmsx {

void zoomAndGridSettings(int& zoom, bool& grid, gl::vec4& gridColor)
{
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
	ImGui::Combo("Zoom", &zoom, "1x\0002x\0003x\0004x\0005x\0006x\0007x\0008x\000");
	ImGui::Checkbox("grid", &grid);
	ImGui::SameLine();
	im::Disabled(!grid, [&]{
		ImGui::ColorEdit4("Grid color", gridColor.data(),
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
	});
}

void rasterBeamSettings(bool& rasterBeam, gl::vec4& color, gl::ivec2 pos, ImFont* font, std::string_view help)
{
	ImGui::Checkbox("beam", &rasterBeam);
	ImGui::SameLine();
	im::Disabled(!rasterBeam, [&]{
		ImGui::ColorEdit4("raster beam color", color.data(),
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		ImGui::SameLine();
		im::Font(font, [&]{
			ImGui::StrCat('(', dec_string<4>(pos.x), ',', dec_string<4>(pos.y), ')');
		});
	});
	HelpMarker(help);
}

void addRasterBeam(ImDrawList* drawList, gl::vec2 scrnPos, gl::vec2 zoom, gl::vec2 pos, gl::vec4 color)
{
	auto center = scrnPos + (pos + gl::vec2{0.5f}) * zoom;
	addCrosshair(drawList, center, zoom, ImGui::ColorConvertFloat4ToU32(color), zoom.y * 0.5f);
}

void checkerBoardSettings(std::string_view toolTip, gl::vec4& color1, gl::vec4& color2, int& size)
{
	ImGui::TextUnformatted("Checkerboard:"sv);
	simpleToolTip(toolTip);
	ImGui::SameLine();
	colorEditWithReset("checkerboard color1", color1, defaultCheckerBoardColor1);
	ImGui::SameLine();
	colorEditWithReset("checkerboard color2", color2, defaultCheckerBoardColor2);
	im::Indent([&]{
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
		if (ImGui::InputInt("size", &size)) {
			size = std::clamp(size, 0, maxCheckerBoardSize);
		}
		simpleToolTip("0 = no checkerboard");
	});
}

void comboFgBgColor(int& colorValue)
{
	int fg = (colorValue >> 4) & 0x0f;
	int bg = (colorValue >> 0) & 0x0f;
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
	if (ImGui::Combo("fg", &fg, colorItems)) {
		colorValue = (fg << 4) | bg;
	}
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
	ImGui::SameLine();
	if (ImGui::Combo("bg", &bg, colorItems)) {
		colorValue = (fg << 4) | bg;
	}
}

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
