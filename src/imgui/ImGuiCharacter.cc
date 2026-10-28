#include "ImGuiCharacter.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiPalette.hh"
#include "ImGuiUtils.hh"
#include "ImGuiVDPUtils.hh"

#include "DisplayMode.hh"
#include "MSXMotherBoard.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"

#include "one_of.hh"
#include "ranges.hh"

#include <imgui.h>

namespace openmsx {

using namespace std::literals;

ImGuiCharacter::ImGuiCharacter(ImGuiManager& manager_, size_t index)
	: ImGuiPart(manager_)
	, title(strCat("Tile viewer", strCat_if(index, " (", index + 1, ')')))
{
}

void ImGuiCharacter::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiCharacter::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiCharacter::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;
	ImGui::SetNextWindowSize({692, 886}, ImGuiCond_FirstUseEver);
	im::Window(title.c_str(), &show, [&]{
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
		bool vdpBlink = vdp->getBlinkState(0); // fast-blink is not implemented here
		int vdpPatBase = vdp->getPatternTableBase() & ~(patMult(vdpMode) - 1);
		int vdpColBase = vdp->getColorTableBase() & ~(colMult(vdpMode) - 1);
		int vdpNamBase = vdp->getNameTableBase() & ~(namMult(vdpMode) - 1);
		int vdpLines = vdp->getNumberOfLines();
		int vdpColor0 = vdp->getTransparency() ? vdpBgCol
		                                       : 16; // no replacement
		auto vramSize = std::min(vdp->getVRAM().getSize(), 0x20000u); // max 128kB
		bool isMSX1 = vdp->isMSX1VDP();

		bool manMode    = overrideAll || overrideMode;
		bool manFgCol   = overrideAll || overrideFgCol;
		bool manBgCol   = overrideAll || overrideBgCol;
		bool manFgBlink = overrideAll || overrideFgBlink;
		bool manBgBlink = overrideAll || overrideBgBlink;
		bool manBlink   = overrideAll || overrideBlink;
		bool manPat     = overrideAll || overridePat;
		bool manCol     = overrideAll || overrideCol;
		bool manNam     = overrideAll || overrideNam;
		bool manRows    = overrideAll || overrideRows;
		bool manColor0  = overrideAll || overrideColor0;

		int mode = manMode ? manualMode : vdpMode;
		if (mode == SCR4) mode = SCR2;

		auto rasterBeamPos = vdp->getMSXPos(vdp->getCurrentTime());
		if (mode == one_of(TEXT40, TEXT80)) rasterBeamPos.x -= (512 - (6 * 80)) / 2;
		if (mode != TEXT80) rasterBeamPos.x /= 2;

		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			overridableSettings("VDP settings", "Screen mode: screen 0, width 80", overrideAll, {
				{tmpStrCat("Screen mode: ", modeToStr(vdpMode)), &overrideMode, [&]{
					if (isMSX1 && (manualMode == one_of(TEXT80, SCR4))) manualMode = TEXT40;
					im::Combo("##mode", modeToStr(manualMode), [&]{
						if            (ImGui::Selectable("screen 0,40")) manualMode = TEXT40;
						if (!isMSX1 && ImGui::Selectable("screen 0,80")) manualMode = TEXT80;
						if            (ImGui::Selectable("screen 1"))    manualMode = SCR1;
						if            (ImGui::Selectable("screen 2"))    manualMode = SCR2;
						if            (ImGui::Selectable("screen 3"))    manualMode = SCR3;
						if (!isMSX1 && ImGui::Selectable("screen 4"))    manualMode = SCR4;
					});
				}},
				{tmpStrCat("Foreground color: ", vdpFgCol), &overrideFgCol, [&]{
					im::Disabled(manualMode != one_of(TEXT40, TEXT80), [&]{ ImGui::Combo("##fgCol", &manualFgCol, colorItems); });
				}},
				{tmpStrCat("Background color: ", vdpBgCol), &overrideBgCol, [&]{
					im::Disabled(manualMode != one_of(TEXT40, TEXT80), [&]{ ImGui::Combo("##bgCol", &manualBgCol, colorItems); });
				}},
				{tmpStrCat("Foreground blink color: ", vdpFgBlink), &overrideFgBlink, [&]{
					im::Disabled(manualMode != TEXT80, [&]{ ImGui::Combo("##fgBlink", &manualFgBlink, colorItems); });
				}},
				{tmpStrCat("Background blink color: ", vdpBgBlink), &overrideBgBlink, [&]{
					im::Disabled(manualMode != TEXT80, [&]{ ImGui::Combo("##bgBlink", &manualBgBlink, colorItems); });
				}},
				{tmpStrCat("Blink: ", vdpBlink ? "enabled"sv : "disabled"sv), &overrideBlink, [&]{
					im::Disabled(manualMode != TEXT80, [&]{ ImGui::Combo("##blink", &manualBlink, "disabled\000enabled\000"); });
				}},
				{tmpStrCat("Pattern table: 0x", hex_string<5>(vdpPatBase)), &overridePat, [&]{
					comboHexSequence<5>("##pattern", &manualPatBase, patMult(manualMode), vramSize, 0);
				}},
				{tmpStrCat("Color table: 0x", hex_string<5>(vdpColBase)), &overrideCol, [&]{
					im::Disabled(manualMode == one_of(TEXT40, SCR3), [&]{
						comboHexSequence<5>("##color", &manualColBase, colMult(manualMode), vramSize, 0);
					});
				}},
				{tmpStrCat("Name table: 0x", hex_string<5>(vdpNamBase)), &overrideNam, [&]{
					comboHexSequence<5>("##name", &manualNamBase, namMult(manualMode), vramSize, 0);
				}},
				{tmpStrCat("Visible rows: ", (vdpLines == 192) ? "24"sv : "26.5"sv), &overrideRows, [&]{
					ImGui::Combo("##rows", &manualRows, "24\00026.5\00032\000");
				}},
				{tmpStrCat("Replace color 0: ", getComboString(vdpColor0, color0Items)), &overrideColor0, [&]{
					ImGui::Combo("##Color 0 replacement", &manualColor0, color0Items);
				}},
			});

			ImGui::SameLine();
			ImGui::Dummy(ImVec2(25, 1));
			ImGui::SameLine();
			im::Group([&]{
				manager.palette->drawSelector();
				ImGui::Separator();
				zoomAndGridSettings(zoom, grid, gridColor);
				ImGui::Checkbox("Name table overlay", &nameTableOverlay);

				ImGui::Checkbox("Override color table", &overrideColorTable);
				HelpMarker("Ignore the content of the color table and instead use this value for everything.\n"
				           "Could be useful to visualize the patterns when the color table has the same value for fg and bg.");
				im::DisabledIndent(!overrideColorTable, [&]{
					comboFgBgColor(overrideColorValue);
				});

				ImGui::Separator();
				rasterBeamSettings(rasterBeam, rasterBeamColor, rasterBeamPos, manager.fontMono,
					"Position of the raster beam, expressed in MSX coordinates.\n"
					"Left/top border have negative x/y-coordinates.\n"
					"Only practically useful when emulation is paused.");
			});
		});
		int manualLines = (manualRows == 0) ? 192
		                : (manualRows == 1) ? 212
		                : 256;

		int lines = manRows ? manualLines : vdpLines;
		int color0 = manColor0 ? manualColor0 : vdpColor0;

		VramTable patTable(vram);
		unsigned patReg = (manPat ? (manualPatBase | (patMult(manualMode) - 1)) : vdp->getPatternTableBase()) >> 11;
		patTable.setRegister(patReg, 11);

		VramTable colTable(vram);
		unsigned colReg = (manCol ? (manualColBase | (colMult(manualMode) - 1)) : vdp->getColorTableBase()) >> 6;
		colTable.setRegister(colReg, 6);

		auto colorTabVal = uint8_t(overrideColorValue);
		if (overrideColorTable) {
			colTable = VramTable(std::span(&colorTabVal, 1));
			colTable.setRegister(0, 0);
		}

		VramTable namTable(vram);
		unsigned namReg = (manNam ? (manualNamBase | (namMult(manualMode) - 1)) : vdp->getNameTableBase()) >> 10;
		namTable.setRegister(namReg, 10);
		namTable.setIndexSize((mode == TEXT80) ? 12 : 10);

		auto palette = manager.palette->getPalette(vdp);
		if (color0 < 16) palette[0] = palette[color0];

		auto fgCol = manFgCol ? manualFgCol : vdpFgCol;
		auto bgCol = manBgCol ? manualBgCol : vdpBgCol;
		auto fgBlink = manFgBlink ? manualFgBlink : vdpFgBlink;
		auto bgBlink = manBgBlink ? manualBgBlink : vdpBgBlink;
		auto blink = manBlink ? bool(manualBlink) : vdpBlink;

		bool narrow = mode == TEXT80;
		int zx = (1 + zoom) * (narrow ? 1 : 2);
		int zy = (1 + zoom) * 2;
		gl::vec2 zm{float(zx), float(zy)};

		// Render the patterns to a texture, we need this both to display the name-table and the pattern-table
		auto patternTexSize = [&] -> gl::ivec2 {
			if (mode == TEXT40) return {192,  64}; // 8 rows of 32 characters, each 6x8 pixels
			if (mode == TEXT80) return {192, 128}; // 8 rows of 32 characters, each 6x8 pixels, x2 for blink
			if (mode == SCR1) return {256,  64}; // 8 rows of 32 characters, each 8x8 pixels
			if (mode == SCR2) return {256, lines == 192 ? 192 : 256}; // 8 rows of 32 characters, each 8x8 pixels, all this 3 or 4 times
			if (mode == SCR3) return { 64,  64}; // 8 rows of 32 characters, each 2x2 pixels, all this 4 times
			return {1, 1}; // OTHER -> dummy
		}();
		auto patternTexChars = [&] -> gl::vec2 {
			if (mode == SCR3) return {32.0f, 32.0f};
			if (mode == SCR2) return {32.0f, lines == 192 ? 24.0f : 32.0f};
			if (mode == TEXT80) return {32.0f, 16.0f};
			return {32.0f, 8.0f};
		}();
		auto recipPatTexChars = recip(patternTexChars);
		auto patternDisplaySize = [&] -> gl::ivec2 {
			if (mode == TEXT40) return {192,  64};
			if (mode == TEXT80) return {192, 128};
			if (mode == SCR2) return {256, lines == 192 ? 192 : 256};
			if (mode == SCR3) return {256, 256};
			return {256,  64}; // SCR1, OTHER
		}();
		std::array<uint32_t, 256 * 256> pixels; // max size for SCR2
		renderPatterns(mode, palette, fgCol, bgCol, fgBlink, bgBlink, patTable, colTable, lines, pixels);
		updateTexture(patternTex, patternTexSize, pixels);

		// create grid texture
		auto charWidth = mode == one_of(TEXT40, TEXT80) ? 6 : 8;
		auto charSize = gl::vec2{float(charWidth), 8.0f};
		auto charZoom = zm * charSize;
		auto zoomCharSize = gl::vec2(narrow ? 6.0f : 12.0f, 12.0f) * charSize; // hovered size
		if (grid) {
			updateGridTexture(gridTex, {charWidth * zx, 8 * zy},
			                  ImGui::ColorConvertFloat4ToU32(gridColor));
		}
		if (nameTableOverlay && !smallHexDigits.get()) {
			smallHexDigits = createHexDigitsTexture();
		}

		auto printPatternNr = [](unsigned pat) {
			ImGui::StrCat("Pattern: ", pat, " (0x", hex_string<3>(pat), ')');
		};
		auto printAddressName = [](std::string_view name) {
			ImGui::StrCat(name, " address:");
		};
		auto printAddress = [&](std::string_view name, unsigned address) {
			printAddressName(name);
			ImGui::StrCat("  0x", hex_string<5>(address));
		};
		auto printAddressRange8 = [&](std::string_view name, unsigned address) {
			printAddressName(name);
			ImGui::StrCat("  0x", hex_string<5>(address), "-0x", hex_string<5>(address + 7));
		};
		auto printPatternColorAddress = [&](unsigned pattern) {
			printAddressRange8("Pattern", patTable.getAddress(8 * pattern));
			if (mode == SCR1) {
				// one color byte for 8 patterns
				printAddress("Color", colTable.getAddress(pattern / 8));
			} else if (mode == one_of(SCR2, SCR4)) {
				printAddressRange8("Color", colTable.getAddress(8 * pattern));
			}
		};
		auto formatBinaryData = [&](unsigned address, unsigned count) {
			return formatToString([&](unsigned addr){ return vram[addr]; }, address, address + count - 1, {}, 1, {}, ", ", "%02X", manager.getInterpreter());
		};
		auto getPatternFromGrid = [&] {
			return gridPosition.x + 32 * gridPosition.y;
		};
		auto copyPatternColorPopup = [&] {
			bool hasColorData = mode == one_of(SCR1, SCR2);
			const char* caption = hasColorData
				? "Copy pattern and color data to clipboard"
				: "Copy pattern data to clipboard";
			return copyToClipboardPopup("PatternCopyPopup", caption, [&]{
				auto pattern = getPatternFromGrid();
				auto text = strCat("Pattern data\n", formatBinaryData(patTable.getAddress(8 * pattern), 8));
				if (hasColorData) {
					strAppend(text, "Color data\n",
					          (mode == SCR1) ? formatBinaryData(colTable.getAddress(pattern / 8), 1)
					                         : formatBinaryData(colTable.getAddress(8 * pattern), 8));
				}
				return text;
			});
		};
		auto drawGrid = [&] {
			auto pat = getPatternFromGrid();
			printPatternNr(pat);
			auto uv1 = gl::vec2(gridPosition) * recipPatTexChars;
			auto uv2 = uv1 + recipPatTexChars;
			imageWithGrid(patternTex, zoomCharSize, uv1, uv2, grid ? &gridTex : nullptr, charSize);
			printPatternColorAddress(pat);
		};

		ImGui::Separator();
		im::TreeNode("Pattern Table", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			auto size = gl::vec2(patternDisplaySize) * zm;
			auto textLines = (mode == TEXT40) ? 3.0f : 5.0f;
			auto previewHeight = zoomCharSize.y + textLines * ImGui::GetTextLineHeightWithSpacing();
			im::Child("##pattern", {0, childHeightWithScrollbar(std::max(size.y, previewHeight))}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto pos1 = ImGui::GetCursorPos();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				ImGui::Image(patternTex.getImGui(), size);
				bool hovered = ImGui::IsItemHovered() && (mode != OTHER);
				ImGui::SameLine();
				im::Group([&]{
					if (copyPatternColorPopup()) {
						drawGrid();
					} else if (hovered) {
						// store last selected position before popup
						gridPosition = hoveredCell(scrnPos, charZoom);
						drawGrid();
					} else {
						ImGui::Dummy(zoomCharSize);
					}
				});
				if (grid) {
					ImGui::SetCursorPos(pos1);
					ImGui::Image(gridTex.getImGui(), size,
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

			im::Child("##name", {0.0f, childHeightWithScrollbar(hostSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				auto* drawList = ImGui::GetWindowDrawList();

				auto getPattern = [&](unsigned column, unsigned row) {
					auto block = [&] -> unsigned {
						if (mode == TEXT80 && blink) {
							auto colPat = colTable[10 * row + (column / 8)];
							return (colPat & (0x80 >> (column % 8))) ? 1 : 0;
						}
						if (mode == SCR2) return row / 8;
						if (mode == SCR3) return row % 4;
						return 0;
					}();
					return namTable[columns * row + column] + 256 * block;
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
					drawList->PushTexture(patternTex.getImGui());
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
					drawList->PopTexture();
					if (nameTableOverlay) {
						drawList->PushTexture(smallHexDigits.getImGui());
						drawList->PrimReserve(12 * numChars, 8 * numChars);
						auto digitOffset = narrow ? gl::vec2{0.0f, hexDigitSize.y} : gl::vec2{hexDigitSize.x, 0.0f};
						for (auto row : xrange(rowsCeil)) {
							for (auto column : xrange(columns)) {
								gl::vec2 pos = scrnPos + charZoom * gl::vec2{float(column), float(row)};
								addHexDigits(drawList, pos, digitOffset, getPattern(column, row), 2);
							}
						}
						drawList->PopTexture();
					}
					drawList->PopClipRect();
				}
				if (rasterBeam) {
					addRasterBeam(drawList, scrnPos, zm, gl::vec2(rasterBeamPos), rasterBeamColor);
				}

				auto pos1 = ImGui::GetCursorPos();
				ImGui::Dummy(hostSize);
				bool hovered = ImGui::IsItemHovered() && (mode != OTHER);
				ImGui::SameLine();
				im::Group([&]{
					if (hovered) {
						auto gridPos = hoveredCell(scrnPos, charZoom);
						ImGui::StrCat("Column: ", gridPos.x, " Row: ", gridPos.y);
						auto pattern = getPattern(gridPos.x, gridPos.y);
						printPatternNr(pattern);
						auto [uv1, uv2] = getPatternUV(pattern);
						imageWithGrid(patternTex, zoomCharSize, uv1, uv2, grid ? &gridTex : nullptr, charSize);
						auto nameIndex = columns * gridPos.y + gridPos.x;
						printAddress("Name", namTable.getAddress(nameIndex));
						printPatternColorAddress(pattern);
						if (mode == TEXT80) {
							printAddress("Color", colTable.getAddress(nameIndex / 8));
						}
					} else {
						gl::vec2 textSize = ImGui::CalcTextSize("Column: 31 Row: 23"sv);
						ImGui::Dummy(max(zoomCharSize, textSize));
					}
				});
				if (grid) {
					ImGui::SetCursorPos(pos1);
					ImGui::Image(gridTex.getImGui(), hostSize, {}, charsSize);
				}
			});
		});
	});
}

void ImGuiCharacter::renderPatterns(int mode, std::span<const uint32_t, 16> palette,
                                    int fgCol, int bgCol, int fgBlink, int bgBlink,
                                    VramTable& pat, VramTable& col, int lines, std::span<uint32_t> output)
{
	auto patByte = [&](unsigned i) { return pat[i]; };
	auto colByte = [&](unsigned i) { return col[i]; };
	switch (mode) {
	case TEXT40:
	case TEXT80:
		pat.setIndexSize(11);
		col.setIndexSize(9); // only matters for TEXT80
		renderTextPatterns(patByte, palette[fgCol], palette[bgCol], output);
		if (mode == TEXT80) {
			// the same patterns in the blink colors, below the others
			renderTextPatterns(patByte, palette[fgBlink], palette[bgBlink], output.subspan(64 * 192));
		}
		break;
	case SCR1:
		pat.setIndexSize(11);
		col.setIndexSize(6);
		renderGraphic1Patterns(patByte, colByte, palette, output);
		break;
	case SCR2:
		pat.setIndexSize(13);
		col.setIndexSize(13);
		renderGraphic2Patterns(patByte, colByte, palette, (lines == 192) ? 3 : 4, output);
		break;
	case SCR3:
		pat.setIndexSize(11);
		col.setIndexSize(13); // not used?
		renderMulticolorPatterns(patByte, palette, output);
		break;
	default:
		output[0] = getColor(imColor::GRAY);
		break;
	}
}

} // namespace openmsx
