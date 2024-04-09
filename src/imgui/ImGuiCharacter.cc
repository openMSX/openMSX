#include "ImGuiCharacter.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiPalette.hh"
#include "ImGuiUtils.hh"

#include "DisplayMode.hh"
#include "MSXMotherBoard.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"

#include "one_of.hh"
#include "ranges.hh"
#include "view.hh"

#include <imgui.h>

namespace openmsx {

using namespace std::literals;

void ImGuiCharacter::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiCharacter::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiCharacter::initHexDigits()
{
	smallHexDigits = gl::Texture(false, false); // no interpolation, no wrapping

	// font definition: 16 glyphs 0-9 A-F, each 5 x 8 pixels
	static constexpr int charWidth = 5;
	static constexpr int charHeight = 8;
	static constexpr int totalWidth = 16 * charWidth;
	static constexpr int totalSize = totalWidth * charHeight;
	static constexpr std::string_view glyphs =
		" ...  ...  ................................................  ........ .........."
		".MMM...M. .MMM..MMM..M.M..MMM..MMM..MMM..MMM..MMM..MMM..MM....MM..MM...MMM..MMM."
		".M.M..MM. ...M....M..M.M..M....M......M..M.M..M.M..M.M..M.M..M....M.M..M....M..."
		".M.M...M.  .MM..MMM..M.M..MMM..MMM.  .M..MMM..MMM..MMM..MM...M.  .M.M..MMM..MMM."
		".M.M. .M. .M.. ...M..MMM....M..M.M.  .M..M.M....M..M.M..M.M..M.  .M.M..M....M..."
		".M.M. .M. .M......M....M....M..M.M.  .M..M.M....M..M.M..M.M..M....M.M..M....M.  "
		".MMM. .M. .MMM..MMM.  .M..MMM..MMM.  .M..MMM..MMM..M.M..MM....MM..MM...MMM..M.  "
		" ...  ... ..........  .............  ......................  ........ ........  ";
	static_assert(glyphs.size() == totalSize);

	// transform to 32-bit RGBA
	std::array<uint32_t, totalSize> pixels;
	for (auto [c, p] : view::zip_equal(glyphs, pixels)) {
		p = (c == ' ') ? ImColor(0.0f, 0.0f, 0.0f, 0.0f)  // transparent
		  : (c == '.') ? ImColor(0.0f, 0.0f, 0.0f, 0.7f)  // black semi-transparent outline
		               : ImColor(1.0f, 1.0f, 1.0f, 0.7f); // white semi-transparent
	}

	// and upload as a texture
	smallHexDigits.bind();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, totalWidth, charHeight, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
}

void ImGuiCharacter::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;
	ImGui::SetNextWindowSize({686, 886}, ImGuiCond_FirstUseEver);
	im::Window("Tile viewer", &show, [&]{
		auto* vdp = dynamic_cast<VDP*>(motherBoard->findDevice("VDP")); // TODO name based OK?
		if (!vdp) return;
		const auto& vram = vdp->getVRAM().getData();

		int vdpMode = [&] {
			auto base = vdp->getDisplayMode().getBase();
			if (base == DisplayMode::TEXT1) return TEXT40;
			if (base == DisplayMode::TEXT2) return TEXT80;
			if (base == DisplayMode::GRAPHIC1) return SCR1;
			if (base == DisplayMode::GRAPHIC2) return SCR2;
			if (base == DisplayMode::GRAPHIC3) return SCR4;
			if (base == DisplayMode::MULTICOLOR) return SCR3;
			return OTHER;
		}();
		auto modeToStr = [](int mode) {
			if (mode == TEXT40) return "screen 0, width 40";
			if (mode == TEXT80) return "screen 0, width 80";
			if (mode == SCR1)   return "screen 1";
			if (mode == SCR2)   return "screen 2";
			if (mode == SCR3)   return "screen 3";
			if (mode == SCR4)   return "screen 4";
			if (mode == OTHER)  return "non-character";
			assert(false); return "ERROR";
		};
		auto patMult = [](int mode) { return 1 << (mode == one_of(SCR2, SCR4) ? 13 : 11); };
		auto colMult = [](int mode) { return 1 << (mode == one_of(SCR2, SCR4) ? 13 :
							mode == TEXT80             ?  9 :  6); };
		auto namMult = [](int mode) { return 1 << (mode == TEXT80 ? 12 : 10); };
		int vdpFgCol = vdp->getForegroundColor() & 15;
		int vdpBgCol = vdp->getBackgroundColor() & 15;
		int vdpFgBlink = vdp->getBlinkForegroundColor() & 15;
		int vdpBgBlink = vdp->getBlinkBackgroundColor() & 15;
		bool vdpBlink = vdp->getBlinkState();
		int vdpPatBase = vdp->getPatternTableBase() & ~(patMult(vdpMode) - 1);
		int vdpColBase = vdp->getColorTableBase() & ~(colMult(vdpMode) - 1);
		int vdpNamBase = vdp->getNameTableBase() & ~(namMult(vdpMode) - 1);
		int vdpLines = vdp->getNumberOfLines();
		int vdpColor0 = vdp->getTransparency() ? vdpBgCol
						: 16; // no replacement

		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			static const char* const color0Str = "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000none\000";
			im::Group([&]{
				ImGui::RadioButton("Use VDP settings", &manual, 0);
				im::Disabled(manual != 0, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Screen mode: ", modeToStr(vdpMode));
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Foreground color: ", vdpFgCol);
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Background color: ", vdpBgCol);
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Foreground blink color: ", vdpFgBlink);
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Background blink color: ", vdpBgBlink);
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Blink: ", vdpBlink ? "enabled" : "disabled");
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Pattern table: 0x", hex_string<5>(vdpPatBase));
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Color table: 0x", hex_string<5>(vdpColBase));
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Name table: 0x", hex_string<5>(vdpNamBase));
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Visible rows: ", (vdpLines == 192) ? "24" : "26.5");
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Replace color 0: ", getComboString(vdpColor0, color0Str));
				});
			});
			ImGui::SameLine();
			im::Group([&]{
				ImGui::RadioButton("Manual override", &manual, 1);
				im::Disabled(manual != 1, [&]{
					im::ItemWidth(ImGui::GetFontSize() * 9.0f, [&]{
						ImGui::Combo("##mode", &manualMode, "screen 0,40\000screen 0,80\000screen 1\000screen 2\000screen 3\000screen 4\000");
						static const char* const range0_15 = "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000";
						im::Disabled(manualMode != one_of(TEXT40, TEXT80), [&]{
							ImGui::Combo("##fgCol", &manualFgCol, range0_15);
							ImGui::Combo("##bgCol", &manualBgCol, range0_15);
						});
						im::Disabled(manualMode != TEXT80, [&]{
							ImGui::Combo("##fgBlink", &manualFgBlink, range0_15);
							ImGui::Combo("##bgBlink", &manualBgBlink, range0_15);
							ImGui::Combo("##blink", &manualBlink, "disabled\000enabled\000");
						});
						comboHexSequence<5>("##pattern", &manualPatBase, patMult(manualMode));
						im::Disabled(manualMode == one_of(TEXT40, SCR3), [&]{
							comboHexSequence<5>("##color", &manualColBase, colMult(manualMode));
						});
						comboHexSequence<5>("##name", &manualNamBase, namMult(manualMode));
						ImGui::Combo("##rows", &manualRows, "24\00026.5\00032\000");
						ImGui::Combo("##Color 0 replacement", &manualColor0, color0Str);
					});
				});
			});

			ImGui::SameLine();
			ImGui::Dummy(ImVec2(25, 1));
			ImGui::SameLine();
			im::Group([&]{
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
				ImGui::Combo("Palette", &manager.palette->whichPalette, "VDP\000Custom\000Fixed\000");
				if (ImGui::Button("Open palette editor")) {
					manager.palette->window.raise();
				}
				ImGui::Separator();
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
				ImGui::Combo("Zoom", &zoom, "1x\0002x\0003x\0004x\0005x\0006x\0007x\0008x\000");
				ImGui::Checkbox("grid", &grid);
				ImGui::SameLine();
				im::Disabled(!grid, [&]{
					ImGui::ColorEdit4("Grid color", gridColor.data(),
						ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
				});
				ImGui::Checkbox("Name table overlay", &nameTableOverlay);
			});
		});
		int manualLines = (manualRows == 0) ? 192
				: (manualRows == 1) ? 212
						: 256;

		int mode = manual ? manualMode : vdpMode;
		if (mode == SCR4) mode = SCR2;

		int patBase = manual ? manualPatBase : vdpPatBase;
		int colBase = manual ? manualColBase : vdpColBase;
		int namBase = manual ? manualNamBase : vdpNamBase;
		int lines = manual ? manualLines : vdpLines;
		int color0 = manual ? manualColor0 : vdpColor0;

		assert((patBase % patMult(mode)) == 0);
		assert((colBase % colMult(mode)) == 0);
		assert((namBase % namMult(mode)) == 0);

		std::array<uint32_t, 16> palette;
		auto msxPalette = manager.palette->getPalette(vdp);
		ranges::transform(msxPalette, palette.data(),
			[](uint16_t msx) { return ImGuiPalette::toRGBA(msx); });
		if (color0 < 16) palette[0] = palette[color0];

		auto fgCol = manual ? manualFgCol : vdpFgCol;
		auto bgCol = manual ? manualBgCol : vdpBgCol;
		auto fgBlink = manual ? manualFgBlink : vdpFgBlink;
		auto bgBlink = manual ? manualBgBlink : vdpBgBlink;
		auto blink = manual ? bool(manualBlink) : vdpBlink;

		bool narrow = mode == TEXT80;
		int zx = (1 + zoom) * (narrow ? 1 : 2);
		int zy = (1 + zoom) * 2;
		gl::vec2 zm{float(zx), float(zy)};

		// Render the patterns to a texture, we need this both to display the name-table and the pattern-table
		auto patternTexSize = [&]() -> gl::ivec2 {
			if (mode == TEXT40) return {192,  64}; // 8 rows of 32 characters, each 6x8 pixels
			if (mode == TEXT80) return {192, 128}; // 8 rows of 32 characters, each 6x8 pixels, x2 for blink
			if (mode == SCR1) return {256,  64}; // 8 rows of 32 characters, each 8x8 pixels
			if (mode == SCR2) return {256, lines == 192 ? 192 : 256}; // 8 rows of 32 characters, each 8x8 pixels, all this 3 or 4 times
			if (mode == SCR3) return { 64,  64}; // 8 rows of 32 characters, each 2x2 pixels, all this 4 times
			return {1, 1}; // OTHER -> dummy
		}();
		auto patternTexChars = [&]() -> gl::vec2 {
			if (mode == SCR3) return {32.0f, 32.0f};
			if (mode == SCR2) return {32.0f, lines == 192 ? 24.0f : 32.0f};
			if (mode == TEXT80) return {32.0f, 16.0f};
			return {32.0f, 8.0f};
		}();
		auto recipPatTexChars = recip(patternTexChars);
		auto patternDisplaySize = [&]() -> gl::ivec2 {
			if (mode == TEXT40) return {192,  64};
			if (mode == TEXT80) return {192, 128};
			if (mode == SCR2) return {256, lines == 192 ? 192 : 256};
			if (mode == SCR3) return {256, 256};
			return {256,  64}; // SCR1, OTHER
		}();
		std::array<uint32_t, 256 * 256> pixels; // max size for SCR2
		renderPatterns(mode, vram, palette, fgCol, bgCol, fgBlink, bgBlink, patBase, colBase, lines, pixels);
		if (!patternTex.get()) {
			patternTex = gl::Texture(false, false); // no interpolation, no wrapping
		}
		patternTex.bind();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, patternTexSize.x, patternTexSize.y, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

		// create grid texture
		auto charWidth = mode == one_of(TEXT40, TEXT80) ? 6 : 8;
		auto charSize = gl::vec2{float(charWidth), 8.0f};
		auto charZoom = zm * charSize;
		auto zoomCharSize = gl::vec2(narrow ? 6.0f : 12.0f, 12.0f) * charSize; // hovered size
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
		if (nameTableOverlay && !smallHexDigits.get()) {
			initHexDigits();
		}

		ImGui::Separator();
		im::TreeNode("Pattern Table", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			auto size = gl::vec2(patternDisplaySize) * zm;
			im::Child("##pattern", {0, size.y}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto pos1 = ImGui::GetCursorPos();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				ImGui::Image(reinterpret_cast<void*>(patternTex.get()), size);
				bool hovered = ImGui::IsItemHovered() && (mode != OTHER);
				ImGui::SameLine();
				im::Group([&]{
					if (hovered) {
						auto gridPos = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / charZoom);
						ImGui::StrCat("Pattern: ", gridPos.x + 32 * gridPos.y);
						auto uv1 = gl::vec2(gridPos) * recipPatTexChars;
						auto uv2 = uv1 + recipPatTexChars;
						auto pos2 = ImGui::GetCursorPos();
						ImGui::Image(reinterpret_cast<void*>(patternTex.get()), zoomCharSize, uv1, uv2);
						if (grid) {
							ImGui::SetCursorPos(pos2);
							ImGui::Image(reinterpret_cast<void*>(gridTex.get()),
								zoomCharSize, {}, charSize);
						}
					} else {
						ImGui::Dummy(zoomCharSize);
					}
				});
				if (grid) {
					ImGui::SetCursorPos(pos1);
					ImGui::Image(reinterpret_cast<void*>(gridTex.get()), size,
						{}, patternTexChars);
				}
			});
		});
		ImGui::Separator();
		im::TreeNode("Name Table", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			float rows = float(lines) * (1.0f / 8.0f); // 24, 26.5 or 32
			auto columns = [&] {
				if (mode == TEXT40) return 40;
				if (mode == TEXT80) return 80;
				return 32;
			}();
			auto charsSize = gl::vec2(float(columns), rows); // (x * y) number of characters
			auto msxSize = charSize * charsSize; // (x * y) number of MSX pixels
			auto hostSize = msxSize * zm; // (x * y) number of host pixels

			im::Child("##name", {0.0f, hostSize.y}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				auto* drawList = ImGui::GetWindowDrawList();

				auto getPattern = [&](unsigned column, unsigned row) {
					auto block = [&]() -> unsigned {
						if (mode == TEXT80 && blink) {
							auto colPat = vram[colBase + 10 * row + (column / 8)];
							return (colPat & (0x80 >> (column % 8))) ? 1 : 0;
						}
						if (mode == SCR2) return row / 8;
						if (mode == SCR3) return row % 4;
						return 0;
					}();
					return vram[namBase + columns * row + column] + 256 * block;
				};
				auto getPatternUV = [&](unsigned pattern) -> std::pair<gl::vec2, gl::vec2> {
					auto patRow = pattern / 32;
					auto patCol = pattern % 32;
					gl::vec2 uv1 = gl::vec2{float(patCol), float(patRow)} * recipPatTexChars;
					gl::vec2 uv2 = uv1 + recipPatTexChars;
					return {uv1, uv2};
				};

				if (mode == OTHER) {
					drawList->AddRectFilled(scrnPos, scrnPos + hostSize, getColor(imColor::GRAY));
				} else {
					auto rowsCeil = int(ceilf(rows));
					auto numChars = rowsCeil * columns;
					drawList->PushClipRect(scrnPos, scrnPos + hostSize, true);
					drawList->PushTextureID(reinterpret_cast<void*>(patternTex.get()));
					drawList->PrimReserve(6 * numChars, 4 * numChars);
					for (auto row : xrange(rowsCeil)) {
						for (auto column : xrange(columns)) {
							gl::vec2 p1 = scrnPos + charZoom * gl::vec2{float(column), float(row)};
							gl::vec2 p2 = p1 + charZoom;
							auto pattern = getPattern(column, row);
							auto [uv1, uv2] = getPatternUV(pattern);
							drawList->PrimRectUV(p1, p2, uv1, uv2, 0xffffffff);
						}
					}
					drawList->PopTextureID();
					if (nameTableOverlay) {
						drawList->PushTextureID(reinterpret_cast<void*>(smallHexDigits.get()));
						drawList->PrimReserve(12 * numChars, 8 * numChars);
						static constexpr gl::vec2 digitSize{5.0f, 8.0f};
						static constexpr float texDigitWidth = 1.0f / 16.0f;
						static constexpr gl::vec2 texDigitSize{texDigitWidth, 1.0f};
						auto digitOffset = narrow ? gl::vec2{0.0f, digitSize.y} : gl::vec2{digitSize.x, 0.0f};
						for (auto row : xrange(rowsCeil)) {
							for (auto column : xrange(columns)) {
								gl::vec2 p1 = scrnPos + charZoom * gl::vec2{float(column), float(row)};
								gl::vec2 p2 = p1 + digitOffset;
								auto pattern = getPattern(column, row);
								gl::vec2 uv1{narrow_cast<float>(pattern >> 4) * texDigitWidth, 0.0f};
								gl::vec2 uv2{(pattern & 15) * texDigitWidth, 0.0f};
								drawList->PrimRectUV(p1, p1 + digitSize, uv1, uv1 + texDigitSize, 0xffffffff);
								drawList->PrimRectUV(p2, p2 + digitSize, uv2, uv2 + texDigitSize, 0xffffffff);
							}
						}
						drawList->PopTextureID();
					}
					drawList->PopClipRect();
				}

				auto pos1 = ImGui::GetCursorPos();
				ImGui::Dummy(hostSize);
				bool hovered = ImGui::IsItemHovered() && (mode != OTHER);
				ImGui::SameLine();
				im::Group([&]{
					if (hovered) {
						auto gridPos = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / charZoom);
						ImGui::StrCat("Column: ", gridPos.x, " Row: ", gridPos.y);
						auto pattern = getPattern(gridPos.x, gridPos.y);
						ImGui::StrCat("Pattern: ", pattern);
						auto [uv1, uv2] = getPatternUV(pattern);
						auto pos2 = ImGui::GetCursorPos();
						ImGui::Image(reinterpret_cast<void*>(patternTex.get()), zoomCharSize, uv1, uv2);
						if (grid) {
							ImGui::SetCursorPos(pos2);
							ImGui::Image(reinterpret_cast<void*>(gridTex.get()),
								zoomCharSize, {}, charSize);
						}
					} else {
						gl::vec2 textSize = ImGui::CalcTextSize("Column: 31 Row: 23"sv);
						ImGui::Dummy(max(zoomCharSize, textSize));
					}
				});
				if (grid) {
					ImGui::SetCursorPos(pos1);
					ImGui::Image(reinterpret_cast<void*>(gridTex.get()), hostSize,
						{}, charsSize);
				}
			});
		});
	});
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
                                    int patBase, int colBase, int lines, std::span<uint32_t> output)
{
	switch (mode) {
	case TEXT40:
	case TEXT80: {
		auto fg = palette[fgCol];
		auto bg = palette[bgCol];
		auto fgB = palette[fgBlink];
		auto bgB = palette[bgBlink];
		for (auto row : xrange(8)) {
			for (auto column : xrange(32)) {
				auto patNum = 32 * row + column;
				auto addr = patBase + 8 * patNum;
				for (auto y : xrange(8)) {
					auto pattern = vram[addr + y];
					auto out = subspan<6>(output, (8 * row + y) * 192 + 6 * column);
					draw6(pattern, fg, bg, out);
					if (mode == TEXT80) {
						auto out2 = subspan<6>(output, (64 + 8 * row + y) * 192 + 6 * column);
						draw6(pattern, fgB, bgB, out2);
					}
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
		for (auto row : xrange((lines == 192 ? 3 : 4) * 8)) {
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
		output[0] = getColor(imColor::GRAY);
		break;
	}
}

} // namespace openmsx
