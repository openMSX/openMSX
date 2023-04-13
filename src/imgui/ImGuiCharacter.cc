#include "ImGuiCharacter.hh"

#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "DisplayMode.hh"
#include "MSXMotherBoard.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"

#include "one_of.hh"
#include "ranges.hh"

#include <imgui.h>

namespace openmsx {

ImGuiCharacter::ImGuiCharacter(ImGuiManager& manager_)
	: manager(manager_)
{
}

void ImGuiCharacter::save(ImGuiTextBuffer& buf)
{
	buf.appendf("show=%d\n", show);
}

void ImGuiCharacter::loadLine(std::string_view name, zstring_view value)
{
	if (name == "show") {
		show = StringOp::stringToBool(value);
	}
}

void ImGuiCharacter::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;
	VDP* vdp = dynamic_cast<VDP*>(motherBoard->findDevice("VDP")); // TODO name based OK?
	if (!vdp) return;
	const auto& vram_ = vdp->getVRAM();
	const auto& vram = vram_.getData();

	if (!ImGui::Begin("Tile viewer", &show)) {
		ImGui::End();
		return;
	}

	int vdpMode = [&] {
		auto base = vdp->getDisplayMode().getBase();
		if (base == DisplayMode::TEXT1) return TEXT40;
		if (base == DisplayMode::TEXT2) return TEXT80;
		if (base == DisplayMode::GRAPHIC1) return SCR1;
		if (base == DisplayMode::GRAPHIC2) return SCR2;
		if (base == DisplayMode::GRAPHIC3) return SCR2;
		if (base == DisplayMode::MULTICOLOR) return SCR3;
		return OTHER;
	}();
	auto modeToStr = [](int mode) {
		if (mode == TEXT40) return "screen 0, width 40";
		if (mode == TEXT80) return "screen 0, width 80";
		if (mode == SCR1)   return "screen 1";
		if (mode == SCR2)   return "screen 2 or 4";
		if (mode == SCR3)   return "screen 3";
		if (mode == OTHER)  return "non-character";
		assert(false); return "ERROR";
	};
	int vdpFgCol = vdp->getForegroundColor() & 15;
	int vdpBgCol = vdp->getBackgroundColor() & 15;
	int vdpFgBlink = vdp->getBlinkForegroundColor() & 15;
	int vdpBgBlink = vdp->getBlinkBackgroundColor() & 15;
	int vdpPatBase = vdp->getPatternTableBase();
	int vdpColBase = vdp->getColorTableBase();
	int vdpNamBase = vdp->getNameTableBase();
	int vdpColor0 = vdp->getTransparency() ? vdpBgCol
	                                       : 16; // no replacement

	if (ImGui::TreeNodeEx("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::BeginGroup();
		ImGui::RadioButton("Use VDP settings", &manual, 0);
		ImGui::BeginDisabled(manual != 0);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Screen mode: %s", modeToStr(vdpMode));
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Foreground color: %d", vdpFgCol);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Background color: %d", vdpBgCol);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Foreground blink color: %d", vdpFgBlink);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Background blink color: %d", vdpBgBlink);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Name table: %04x", 0x0000); // TODO
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Pattern table: %04x", 0x1800); // TODO
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Color table: %04x", 0x2000); // TODO
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Visible rows: %d", 24);
		ImGui::AlignTextToFramePadding();
		static const char* const color0Str = "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000none\000";
		ImGui::Text("Replace color 0: %s", getComboString(vdpColor0, color0Str));
		ImGui::EndDisabled();
		ImGui::EndGroup();

		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::RadioButton("Manual override", &manual, 1);
		ImGui::BeginDisabled(manual != 1);
		ImGui::PushItemWidth(ImGui::GetFontSize() * 9.0f);
		int dummy = 0;
		ImGui::Combo("##mode", &scrnMode, "screen 0,40\000screen 0,80\000screen 1\000screen 2\000screen 3");
		ImGui::Combo("##fgCol", &manualFgCol, "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000");
		ImGui::Combo("##bgCol", &manualBgCol, "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000");
		ImGui::Combo("##fgBlink", &manualFgBlink, "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000");
		ImGui::Combo("##bgBlink", &manualBgBlink, "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000");
		ImGui::Combo("##name", &dummy, "0000\0000800\00010000\000");
		ImGui::Combo("##pattern", &dummy, "0000\0000800\00010000\000");
		ImGui::Combo("##color", &dummy, "0000\0000800\00010000\000");
		ImGui::Combo("##rows", &dummy, "24\00026.5\00032\000");
		ImGui::Combo("##Color 0 replacement", &manualColor0, color0Str);
		ImGui::PopItemWidth();
		ImGui::EndDisabled();
		ImGui::EndGroup();

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(25, 1));
		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
		ImGui::Combo("Palette", &manager.palette.whichPalette, "VDP\000Custom\000Fixed\000");
		if (ImGui::Button("Open palette editor")) {
			manager.palette.show = true;
		}
		ImGui::Separator();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
		ImGui::Combo("Zoom", &zoom, "1x\0002x\0003x\0004x\0005x\0006x\0007x\0008x\000");
		ImGui::Checkbox("grid", &grid);
		ImGui::SameLine();
		ImGui::BeginDisabled(!grid);
		ImGui::ColorEdit4("Grid color", &gridColor[0],
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		ImGui::EndDisabled();
		ImGui::EndGroup();
		ImGui::TreePop();
	}

	int mode = manual ? scrnMode : vdpMode;
	int patBase = manual ? manualPatBase : vdpPatBase;
	int colBase = manual ? manualColBase : vdpColBase;
	int namBase = manual ? manualNamBase : vdpNamBase;
	int color0 = manual ? manualColor0 : vdpColor0;

	std::array<uint32_t, 16> palette;
	auto msxPalette = manager.palette.getPalette(vdp);
	ranges::transform(msxPalette, palette.data(),
		[](uint16_t msx) { return ImGuiPalette::toRGBA(msx); });
	if (color0 < 16) palette[0] = palette[color0];

	auto fgCol = manual ? manualFgCol : vdpFgCol;
	auto bgCol = manual ? manualBgCol : vdpBgCol;
	auto fgBlink = manual ? manualFgBlink : vdpFgBlink;
	auto bgBlink = manual ? manualBgBlink : vdpBgBlink;

	int zx = (1 + zoom) * (mode == TEXT80 ? 1 : 2);
	int zy = (1 + zoom) * 2;

	// Render the patterns to a texture, we need this both to display the name-table and the pattern-table
	auto patternTexSize = [&]() -> gl::ivec2 {
		if (mode == one_of(TEXT40, TEXT80)) return {192, 64}; // 8 rows of 32 characters, each 6x8 pixels
		if (mode == SCR1) return {256,  64}; // 8 rows of 32 characters, each 8x8 pixels
		if (mode == SCR2) return {256, 256}; // 8 rows of 32 characters, each 8x8 pixels, all this 4 times
		if (mode == SCR3) return { 64,  64}; // 8 rows of 32 characters, each 2x2 pixels, all this 4 times
		return {1, 1}; // OTHER -> dummy
	}();
	auto patternDisplaySize = [&]() -> gl::ivec2 {
		if (mode == one_of(TEXT40, TEXT80)) return {192, 64};
		if (mode == SCR1) return {256,  64};
		return {256, 256}; // SRC2, SRC3, OTHER
	}();
	std::array<uint32_t, 256 * 256> pixels; // max size for SCR2
	renderPatterns(mode, vram, palette, fgCol, bgCol, fgBlink, bgBlink, patBase, colBase, pixels);
	if (!patternTex.get()) {
		patternTex = gl::Texture(false, false); // no interpolation, no wrapping
	}
	patternTex.bind();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, patternTexSize[0], patternTexSize[1], 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

	// create grid texture
	auto charWidth = mode == one_of(TEXT40, TEXT80) ? 6 : 8;
	if (grid) {
		auto gColor = ImGui::ColorConvertFloat4ToU32(gridColor);
		auto gridWidth = charWidth * zx;
		auto gridHeight = 8 * zy;
		for (auto y : xrange(gridHeight)) {
			auto* line = &pixels[y * gridWidth];
			for (auto x : xrange(gridWidth)) {
				line[x] = (x == 0 || y == 0) ? gColor : 0;
			}
		}
		if (!gridTex.get()) {
			gridTex = gl::Texture(false, true); // no interpolation, with wrapping
		}
		gridTex.bind();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gridWidth, gridHeight, 0,
		             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	}

	ImGui::Separator();
	if (ImGui::TreeNodeEx("Pattern Table")) {
		auto pos = ImGui::GetCursorPos();
		ImVec2 size(float(patternDisplaySize[0] * zx), float(patternDisplaySize[1] * zy));
		ImGui::Image(reinterpret_cast<void*>(patternTex.get()), size);
		if (grid) {
			ImGui::SetCursorPos(pos);
			ImGui::Image(reinterpret_cast<void*>(gridTex.get()), size,
			             ImVec2(0.0f, 0.0f),
			             ImVec2(float(patternDisplaySize[0] / charWidth),
			                    float(patternDisplaySize[1] / 8)));
		}
		ImGui::TreePop();
	}
	ImGui::Separator();
	if (ImGui::TreeNodeEx("Name Table")) {
		auto pos = ImGui::GetCursorPos();
		gl::vec2 scrn = ImGui::GetCursorScreenPos();

		auto nameSize = [&]() -> gl::vec2 {
			if (mode == TEXT40) return {240, 192};
			if (mode == TEXT80) return {480, 192};
			return {256, 192};
		}();
		gl::vec2 zoom{float(zx), float(zy)};
		auto size = nameSize * zoom;
		ImGui::Dummy(size);
		auto* drawList = ImGui::GetWindowDrawList();
		drawList->PushTextureID(reinterpret_cast<void*>(patternTex.get()));

		auto numChars = [&] {
			if (mode == OTHER) return 1;
			if (mode == TEXT40) return 40 * 24;
			if (mode == TEXT80) return 80 * 24;
			return 32 * 24;
		}();
		drawList->PrimReserve(6 * numChars, 4 * numChars);

		auto charZoom = zoom * gl::vec2{float(charWidth), 8.0f};

		// TODO reduce code duplication
		switch (mode) {
		case TEXT40:
			for (auto row : xrange(24)) {
				for (auto column : xrange(40)) {
					auto pattern = vram[namBase + 40 * row + column];
					auto patRow = pattern / 32;
					auto patCol = pattern % 32;
					gl::vec2 p1 = scrn + charZoom * gl::vec2{float(column), float(row)};
					gl::vec2 p2 = p1 + charZoom;
					gl::vec2 uv1{float(patCol + 0) / 32.0f, float(patRow + 0) / 8.0f};
					gl::vec2 uv2{float(patCol + 1) / 32.0f, float(patRow + 1) / 8.0f};
					drawList->PrimRectUV(p1, p2, uv1, uv2, 0xffffffff);
				}
			}
			break;
		case TEXT80:
			// TODO blink
			namBase &= ~0xfff;
			for (auto row : xrange(24)) {
				for (auto column : xrange(80)) {
					auto pattern = vram[namBase + 80 * row + column];
					auto patRow = pattern / 32;
					auto patCol = pattern % 32;
					gl::vec2 p1 = scrn + charZoom * gl::vec2{float(column), float(row)};
					gl::vec2 p2 = p1 + charZoom;
					gl::vec2 uv1{float(patCol + 0) / 32.0f, float(patRow + 0) / 8.0f};
					gl::vec2 uv2{float(patCol + 1) / 32.0f, float(patRow + 1) / 8.0f};
					drawList->PrimRectUV(p1, p2, uv1, uv2, 0xffffffff);
				}
			}
			break;
		case SCR1:
			for (auto row : xrange(24)) {
				for (auto column : xrange(32)) {
					auto pattern = vram[namBase + 32 * row + column];
					auto patRow = pattern / 32;
					auto patCol = pattern % 32;
					gl::vec2 p1 = scrn + charZoom * gl::vec2{float(column), float(row)};
					gl::vec2 p2 = p1 + charZoom;
					gl::vec2 uv1{float(patCol + 0) / 32.0f, float(patRow + 0) / 8.0f};
					gl::vec2 uv2{float(patCol + 1) / 32.0f, float(patRow + 1) / 8.0f};
					drawList->PrimRectUV(p1, p2, uv1, uv2, 0xffffffff);
				}
			}
			break;
		case SCR2:
			for (auto group : xrange(3)) {
				auto patOffset = 256 * group;
				for (auto subRow : xrange(8)) {
					auto row = 8 * group + subRow;
					for (auto column : xrange(32)) {
						auto pattern = vram[namBase + 32 * row + column] + patOffset;
						auto patRow = pattern / 32;
						auto patCol = pattern % 32;
						gl::vec2 p1 = scrn + charZoom * gl::vec2{float(column), float(row)};
						gl::vec2 p2 = p1 + charZoom;
						gl::vec2 uv1{float(patCol + 0) / 32.0f, float(patRow + 0) / 32.0f};
						gl::vec2 uv2{float(patCol + 1) / 32.0f, float(patRow + 1) / 32.0f};
						drawList->PrimRectUV(p1, p2, uv1, uv2, 0xffffffff);
					}
				}
			}
			break;
		case SCR3:
			for (auto row : xrange(24)) {
				auto patOffset = 256 * (row % 4);
				for (auto column : xrange(32)) {
					auto pattern = vram[namBase + 32 * row + column] + patOffset;
					auto patRow = pattern / 32;
					auto patCol = pattern % 32;
					gl::vec2 p1 = scrn + charZoom * gl::vec2{float(column), float(row)};
					gl::vec2 p2 = p1 + charZoom;
					gl::vec2 uv1{float(patCol + 0) / 32.0f, float(patRow + 0) / 32.0f};
					gl::vec2 uv2{float(patCol + 1) / 32.0f, float(patRow + 1) / 32.0f};
					drawList->PrimRectUV(p1, p2, uv1, uv2, 0xffffffff);
				}
			}
			break;
		default:
			drawList->PrimRectUV(scrn, scrn + size, {0.0f, 0.0f}, {1.0f, 1.0f}, 0xffffffff);
			break;
		}
		if (grid) {
			ImGui::SetCursorPos(pos);
			ImGui::Image(reinterpret_cast<void*>(gridTex.get()), size,
			             ImVec2(0.0f, 0.0f),
			             ImVec2(float(nameSize[0] / charWidth),
			                    float(nameSize[1] / 8)));
		}
		ImGui::TreePop();
	}
	ImGui::End();
}

static void draw6(uint8_t pattern, uint32_t fgCol, uint32_t bgCol, std::span<uint32_t, 6> out)
{
	out[0] = (pattern & 0x80) ? fgCol : bgCol;
	out[1] = (pattern & 0x40) ? fgCol : bgCol;
	out[2] = (pattern & 0x20) ? fgCol : bgCol;
	out[3] = (pattern & 0x10) ? fgCol : bgCol;
	out[4] = (pattern & 0x08) ? fgCol : bgCol;
	out[5] = (pattern & 0x04) ? fgCol : bgCol;
}

static void draw8(uint8_t pattern, uint32_t fgCol, uint32_t bgCol, std::span<uint32_t, 8> out)
{
	out[0] = (pattern & 0x80) ? fgCol : bgCol;
	out[1] = (pattern & 0x40) ? fgCol : bgCol;
	out[2] = (pattern & 0x20) ? fgCol : bgCol;
	out[3] = (pattern & 0x10) ? fgCol : bgCol;
	out[4] = (pattern & 0x08) ? fgCol : bgCol;
	out[5] = (pattern & 0x04) ? fgCol : bgCol;
	out[6] = (pattern & 0x02) ? fgCol : bgCol;
	out[7] = (pattern & 0x01) ? fgCol : bgCol;
}

void ImGuiCharacter::renderPatterns(int mode, std::span<const uint8_t> vram, std::span<const uint32_t, 16> palette,
                                    int fgCol, int bgCol, int fgBlink, int bgBlink,
                                    int patBase, int colBase, std::span<uint32_t> output)
{
	switch (mode) {
	case TEXT40:
	case TEXT80: { // TODO also render with blink colors
		auto fg = palette[fgCol];
		auto bg = palette[bgCol];
		for (auto row : xrange(8)) {
			for (auto column : xrange(32)) {
				auto patNum = 32 * row + column;
				auto addr = patBase + 8 * patNum;
				for (auto y : xrange(8)) {
					auto pattern = vram[addr + y];
					auto out = subspan<6>(output, (8 * row + y) * 192 + 6 * column);
					draw6(pattern, fg, bg, out);
				}
			}
		}
		break;
	}
	case SCR1:
		for (auto row : xrange(8)) {
			for (auto group : xrange(4)) { // 32 columns, split in 4 groups of 8
				auto color = vram[colBase + 4 * row + group];
				auto fg = palette[color >> 4];
				auto bg = palette[color & 15];
				for (auto subColumn : xrange(8)) {
					auto column = 8 * group + subColumn;
					auto patNum = 32 * row + column;
					auto addr = patBase + 8 * patNum;
					for (auto y : xrange(8)) {
						auto pattern = vram[addr + y];
						auto out = subspan<8>(output, (8 * row + y) * 256 + 8 * column);
						draw8(pattern, fg, bg, out);
					}
				}
			}
		}
		break;
	case SCR2:
		patBase &= ~0x1fff;
		colBase &= ~0x1fff;
		for (auto row : xrange(4 * 8)) {
			for (auto column : xrange(32)) {
				auto patNum = 32 * row + column;
				auto patAddr = patBase + 8 * patNum;
				auto colAddr = colBase + 8 * patNum;
				for (auto y : xrange(8)) {
					auto pattern = vram[patAddr + y];
					auto color   = vram[colAddr + y];
					auto fg = palette[color >> 4];
					auto bg = palette[color & 15];
					auto out = subspan<8>(output, (8 * row + y) * 256 + 8 * column);
					draw8(pattern, fg, bg, out);
				}
			}
		}
		break;
	case SCR3:
		for (auto group : xrange(4)) {
			for (auto row : xrange(8)) {
				for (auto column : xrange(32)) {
					auto patNum = 32 * row + column;
					auto patAddr = patBase + 8 * patNum + 2 * group;
					for (auto y : xrange(2)) {
						auto out = subspan<2>(output, (16 * group + 2 * row + y) * 64 + 2 * column);
						auto pattern = vram[patAddr + y];
						out[0] = palette[pattern >> 4];
						out[1] = palette[pattern & 15];
					}
				}
			}
		}
		break;
	default:
		output[0] = 0xFF808080; // gray
		break;
	}
}

} // namespace openmsx
