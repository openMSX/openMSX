#include "ImGuiV9990Tiles.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"
#include "ImGuiV9990Utils.hh"
#include "ImGuiVDPUtils.hh"

#include "MSXMotherBoard.hh"
#include "V9990.hh"
#include "V9990VRAM.hh"

#include "MemBuffer.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "xrange.hh"

#include <imgui.h>

#include <algorithm>
#include <optional>
#include <utility>

namespace openmsx {

using namespace std::literals;

ImGuiV9990Tiles::ImGuiV9990Tiles(ImGuiManager& manager_, size_t index)
	: ImGuiPart(manager_)
	, title(strCat("V9990 tile viewer", strCat_if(index, " (", index + 1, ')')))
{
}

void ImGuiV9990Tiles::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiV9990Tiles::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

// V9990 P1 and P2 (pattern) modes. Mirrors the logic in V9990PxConverter.
// * P1: two layers (A and B), each with a name table of 64x64 entries
//   (512x512 pixels) and a pattern table of 8192 patterns, organized as a
//   256-pixel wide 4bpp bitmap (32 patterns per row). Layer A uses palette A,
//   layer B palette B.
// * P2: a single layer with a name table of 128x64 entries (1024x512 pixels)
//   and a pattern table of 8192 patterns, organized as a 512-pixel wide 4bpp
//   bitmap (64 patterns per row). Even/odd bytes use palette A/B.
void ImGuiV9990Tiles::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;
	ImGui::SetNextWindowSize({692, 886}, ImGuiCond_FirstUseEver);
	im::Window(title.c_str(), &show, [&]{
		auto* v9990 = dynamic_cast<V9990*>(motherBoard->findDevice("Sunrise GFX9000")); // TODO name based OK?
		if (!v9990) return;
		using enum V9990DisplayMode;
		using v9990_gui::paletteStr;

		auto vdpMode = v9990->getDisplayMode();
		bool vdpIsPattern = vdpMode == one_of(P1, P2);
		uint8_t vdpPaletteOffset = v9990->getPaletteOffset();
		int vdpPaletteA = (vdpPaletteOffset >> 0) & 3;
		int vdpPaletteB = (vdpPaletteOffset >> 2) & 3;

		bool manMode = overrideAll || overrideMode;
		bool manPalA = overrideAll || overridePaletteA;
		bool manPalB = overrideAll || overridePaletteB;

		bool isPattern = manMode || vdpIsPattern;
		bool isP1 = manMode ? (manualMode == 0) : (vdpMode == P1);
		int palA = manPalA ? manualPaletteA : vdpPaletteA;
		int palB = manPalB ? manualPaletteB : vdpPaletteB;

		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			overridableSettings("V9990 settings", "Scroll A: (0000, 0000)", overrideAll, {
				{tmpStrCat("Display mode: ", v9990_gui::toString(vdpMode)), &overrideMode, [&]{
					ImGui::Combo("##mode", &manualMode, "P1\000P2\000");
				}},
				{tmpStrCat("Palette A: ", getComboString(vdpPaletteA, paletteStr)), &overridePaletteA, [&]{
					ImGui::Combo("##paletteA", &manualPaletteA, paletteStr);
				}},
				{tmpStrCat("Palette B: ", getComboString(vdpPaletteB, paletteStr)), &overridePaletteB, [&]{
					ImGui::Combo("##paletteB", &manualPaletteB, paletteStr);
				}},
				{tmpStrCat("Scroll A: (", v9990->getScrollAX(), ", ", v9990->getScrollAY(), ')'), nullptr, []{}},
				{tmpStrCat("Scroll B: (", v9990->getScrollBX(), ", ", v9990->getScrollBY(), ')'), nullptr, []{}, isP1},
			}, "Interpret VRAM using a manually selected mode and/or palette offset.\n"
			   "\n"
			   "Palette A and B are the two halves of the palette offset register (R#13):\n"
			   "* P1: palette of layer A and layer B\n"
			   "* P2: palette of the even and odd bytes in the pattern data");

			ImGui::SameLine();
			ImGui::Dummy(ImVec2(25, 1));
			ImGui::SameLine();
			im::Group([&]{
				zoomAndGridSettings(zoom, grid, gridColor);
				ImGui::Checkbox("Name table overlay", &nameTableOverlay);
				HelpMarker("Show the (hexadecimal) pattern number on top of each name table entry.\n"
				           "Only shown when the zoom factor is large enough to fit 4 digits.");
			});
		});
		ImGui::Separator();

		if (!isPattern) {
			ImGui::TextUnformatted("Not a pattern mode (P1 or P2). Use manual override to show the VRAM content anyway."sv);
			return;
		}

		auto palette64 = v9990_gui::getPalette64(*v9990);
		auto palette16A = subspan<16>(palette64, 16 * palA);
		auto palette16B = subspan<16>(palette64, 16 * palB);
		const auto& vram = v9990->getVRAM();

		// Render the pattern tables to a texture (one per layer).
		static constexpr int NUM_PATTERNS = 8192;
		static constexpr unsigned LAYER_B_PATTERN = 0x40000;
		int numLayers = isP1 ? 2 : 1;
		int patsPerRow = isP1 ? 32 : 64;
		int patRows = NUM_PATTERNS / patsPerRow;
		int patTexWidth = 8 * patsPerRow;
		int patTexHeight = 8 * patRows;
		unsigned lineBytes = patTexWidth / 2;
		auto patTexChars = gl::vec2(float(patsPerRow), float(patRows));
		auto recipPatTexChars = recip(patTexChars);

		MemBuffer<uint32_t> pixels(patTexWidth * patTexHeight);
		auto uploadPatterns = [&](gl::Texture& tex) {
			updateTexture(tex, {patTexWidth, patTexHeight}, pixels);
		};
		if (isP1) {
			for (int layer : {0, 1}) {
				auto pal16 = layer ? palette16B : palette16A;
				unsigned addr = layer ? LAYER_B_PATTERN : 0;
				for (auto i : xrange(patTexWidth * patTexHeight / 2)) {
					auto value = vram.readVRAMP1(addr++);
					pixels[2 * i + 0] = pal16[value >> 4];
					pixels[2 * i + 1] = pal16[value & 0x0f];
				}
				uploadPatterns(layer ? patternTexB : patternTexA);
			}
		} else {
			for (auto addr : xrange(unsigned(patTexWidth * patTexHeight / 2))) {
				auto value = vram.readVRAMBx(addr);
				auto pal16 = (addr & 1) ? palette16B : palette16A;
				pixels[2 * addr + 0] = pal16[value >> 4];
				pixels[2 * addr + 1] = pal16[value & 0x0f];
			}
			uploadPatterns(patternTexA);
		}
		auto getPatternTex = [&](int layer) -> const gl::Texture& {
			return layer ? patternTexB : patternTexA;
		};

		// P1: 256 pixels per line, P2: 512 pixels per line (non-interlaced)
		int zx = (1 + zoom) * (isP1 ? 2 : 1);
		int zy = (1 + zoom) * 2;
		auto zm = gl::vec2{float(zx), float(zy)};
		static constexpr auto charSize = gl::vec2{8.0f, 8.0f};
		auto charZoom = zm * charSize;
		auto zoomCharSize = gl::vec2(isP1 ? 12.0f : 6.0f, 12.0f) * charSize; // hovered size
		bool showOverlay = nameTableOverlay && (charZoom.x >= 2.0f * hexDigitSize.x) && (charZoom.y >= 2.0f * hexDigitSize.y);
		if (showOverlay && !smallHexDigits.get()) {
			smallHexDigits = createHexDigitsTexture();
		}
		const gl::Texture* gridOverlay = nullptr;
		if (grid) {
			updateGridTexture(gridTex, {8 * zx, 8 * zy}, ImGui::ColorConvertFloat4ToU32(gridColor));
			gridOverlay = &gridTex;
		}

		auto layerName = [&](int layer) {
			return layer ? "Layer B"sv : "Layer A"sv;
		};
		auto getPatternUV = [&](unsigned pattern) -> std::pair<gl::vec2, gl::vec2> {
			auto patRow = pattern / patsPerRow;
			auto patCol = pattern % patsPerRow;
			gl::vec2 uv1 = gl::vec2{float(patCol), float(patRow)} * recipPatTexChars;
			gl::vec2 uv2 = uv1 + recipPatTexChars;
			return {uv1, uv2};
		};
		// logical (CPU / command engine) address of the first line of a pattern
		auto getPatternAddr = [&](int layer, unsigned pattern) {
			unsigned base = layer ? LAYER_B_PATTERN : 0;
			return base + (pattern / patsPerRow) * 8 * lineBytes + (pattern % patsPerRow) * 4;
		};
		auto readPatternByte = [&](unsigned addr) {
			return isP1 ? vram.readVRAMP1(addr) : vram.readVRAMBx(addr);
		};
		// 8 lines of 4 bytes (8 pixels of 4 bits), one line per row
		auto formatPatternData = [&](int layer, unsigned pattern) {
			auto addr = getPatternAddr(layer, pattern);
			return formatToString([&](unsigned i) { return readPatternByte(addr + (i / 4) * lineBytes + (i % 4)); },
			                      0, 31, {}, 4, {}, ", ", "%02X", manager.getInterpreter());
		};
		auto copyPatternPopup = [&] {
			copyToClipboardPopup("PatternCopyPopup", "Copy pattern data to clipboard", [&]{
				return formatPatternData(hoveredPattern[0], unsigned(hoveredPattern[1]));
			});
		};
		auto printPatternInfo = [&](int layer, unsigned pattern) {
			ImGui::StrCat("Pattern: ", pattern, " (0x", hex_string<4>(pattern), ')');
			auto addr = getPatternAddr(layer, pattern);
			ImGui::StrCat("Pattern address: 0x", hex_string<5>(addr),
			              " (+0x", hex_string<3>(lineBytes), " per line)");
		};
		auto drawZoomedPattern = [&](int layer, unsigned pattern) {
			auto [uv1, uv2] = getPatternUV(pattern);
			imageWithGrid(getPatternTex(layer), zoomCharSize, uv1, uv2, gridOverlay, charSize);
		};

		// Width of one layer when the layers are shown side-by-side.
		auto spacing = ImGui::GetStyle().ItemSpacing.x;
		auto scrollbar = ImGui::GetStyle().ScrollbarSize;
		auto layerWidth = [&](float contentWidth) {
			auto avail = (ImGui::GetContentRegionAvail().x - spacing * float(numLayers - 1)) / float(numLayers);
			return std::max(std::min(avail, contentWidth + scrollbar), charZoom.x);
		};

		ImGui::Separator();
		im::TreeNode("Pattern Table", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			auto size = patTexChars * charZoom;
			// The pattern tables are very tall (2048 or 1024 lines): use at most
			// 40% of the available height (but at least 8 rows of patterns),
			// leaving room for the name table, scroll for the rest.
			auto maxHeight = std::max(0.4f * ImGui::GetContentRegionAvail().y, 8.0f * charZoom.y);
			gl::vec2 childSize{layerWidth(size.x), childHeightWithScrollbar(std::min(size.y, maxHeight))};
			for (auto layer : xrange(numLayers)) {
				if (layer) ImGui::SameLine();
				im::Group([&]{
					if (isP1) {
						ImGui::StrCat(layerName(layer), "  (palette ", layer ? palB : palA, ')');
					} else {
						ImGui::StrCat("Palette ", palA, " (even bytes), ", palB, " (odd bytes)");
					}
					im::Child(layer ? "##patternB" : "##patternA", childSize, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
						gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
						imageWithGrid(getPatternTex(layer), size, {}, {1.0f, 1.0f}, gridOverlay, patTexChars);
						bool hovered = false;
						if (ImGui::IsItemHovered()) {
							auto gridPos = hoveredCell(scrnPos, charZoom);
							if ((0 <= gridPos.x) && (gridPos.x < patsPerRow) && (0 <= gridPos.y) && (gridPos.y < patRows)) {
								hovered = true;
								hoveredPattern = {layer, gridPos.x + patsPerRow * gridPos.y};
							}
						}
						copyPatternPopup();
						if (hovered) {
							im::Tooltip([&]{
								auto pattern = unsigned(hoveredPattern[1]);
								if (isP1) ImGui::TextUnformatted(layerName(layer));
								drawZoomedPattern(layer, pattern);
								printPatternInfo(layer, pattern);
							});
						}
					});
				});
			}
		});

		ImGui::Separator();
		im::TreeNode("Name Table", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			int columns = isP1 ? 64 : 128;
			static constexpr int rows = 64;
			auto charsSize = gl::vec2{float(columns), float(rows)};
			auto hostSize = charsSize * charZoom;
			auto getNameAddr = [&](int layer, int column, int row) {
				unsigned nameBase = layer ? 0x7E000 : 0x7C000;
				return nameBase + 2 * (columns * row + column);
			};
			auto getPattern = [&](int layer, int column, int row) {
				auto addr = getNameAddr(layer, column, row);
				return unsigned(vram.readVRAMDirect(addr) + 256 * vram.readVRAMDirect(addr + 1)) & 0x1FFF;
			};

			// Use the remaining height of the window, but show at least 16 rows.
			auto availHeight = ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing();
			auto height = std::min(childHeightWithScrollbar(hostSize.y), std::max(availHeight, 16.0f * charZoom.y));
			gl::vec2 childSize{layerWidth(hostSize.x), height};

			for (auto layer : xrange(numLayers)) {
				if (layer) ImGui::SameLine();
				im::Group([&]{
					ImGui::StrCat(layerName(layer), "  (scroll ",
					              layer ? v9990->getScrollBX() : v9990->getScrollAX(), ", ",
					              layer ? v9990->getScrollBY() : v9990->getScrollAY(), ')');
					im::Child(layer ? "##nameB" : "##nameA", childSize, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
						gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
						auto* drawList = ImGui::GetWindowDrawList();

						// only draw the visible part
						gl::vec2 scroll{ImGui::GetScrollX(), ImGui::GetScrollY()};
						gl::vec2 winSize = ImGui::GetWindowSize();
						auto begin = clamp(trunc(scroll / charZoom), gl::ivec2{}, gl::ivec2{columns, rows});
						auto end = clamp(trunc((scroll + winSize) / charZoom) + gl::ivec2{1}, gl::ivec2{}, gl::ivec2{columns, rows});

						auto origin = scrnPos + charZoom * gl::vec2(begin);
						auto num = end - begin;
						drawList->PushClipRect(scrnPos, scrnPos + hostSize, true);
						addTiles(drawList, getPatternTex(layer), origin, charZoom, num,
						         [&](int column, int row) {
							return getPatternUV(getPattern(layer, begin.x + column, begin.y + row));
						});
						if (showOverlay) {
							// 4 hex digits in a 2x2 arrangement: the cells are too
							// narrow for a row of 4 digits.
							auto numDigits = 4 * num.x * num.y;
							drawList->PushTexture(smallHexDigits.getImGui());
							drawList->PrimReserve(6 * numDigits, 4 * numDigits);
							gl::vec2 step{hexDigitSize.x, 0.0f};
							for (auto row : xrange(num.y)) {
								for (auto column : xrange(num.x)) {
									auto p = origin + charZoom * gl::vec2{float(column), float(row)};
									auto pattern = getPattern(layer, begin.x + column, begin.y + row);
									addHexDigits(drawList, p, step, pattern >> 8, 2);
									addHexDigits(drawList, p + gl::vec2{0.0f, hexDigitSize.y}, step, pattern & 0xff, 2);
								}
							}
							drawList->PopTexture();
						}
						drawList->PopClipRect();

						auto pos1 = ImGui::GetCursorPos();
						ImGui::Dummy(hostSize);
						std::optional<gl::ivec2> gridPos;
						if (ImGui::IsItemHovered()) {
							auto p = hoveredCell(scrnPos, charZoom);
							if ((0 <= p.x) && (p.x < columns) && (0 <= p.y) && (p.y < rows)) {
								gridPos = p;
								hoveredPattern = {layer, int(getPattern(layer, p.x, p.y))};
							}
						}
						copyPatternPopup();
						if (gridPos) {
							im::Tooltip([&]{
								if (isP1) ImGui::TextUnformatted(layerName(layer));
								ImGui::StrCat("Column: ", gridPos->x, " Row: ", gridPos->y);
								auto pattern = unsigned(hoveredPattern[1]);
								drawZoomedPattern(layer, pattern);
								auto nameAddr = getNameAddr(layer, gridPos->x, gridPos->y);
								ImGui::StrCat("Name address: 0x", hex_string<5>(nameAddr), "-0x", hex_string<5>(nameAddr + 1));
								printPatternInfo(layer, pattern);
							});
						}
						if (gridOverlay) {
							ImGui::SetCursorPos(pos1);
							ImGui::Image(gridOverlay->getImGui(), hostSize, {}, charsSize);
						}
					});
				});
			}
		});
	});
}

} // namespace openmsx
