#include "ImGuiV9990Tiles.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"
#include "ImGuiV9990Utils.hh"

#include "MSXMotherBoard.hh"
#include "V9990.hh"
#include "V9990VRAM.hh"

#include "MemBuffer.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "xrange.hh"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>
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

void ImGuiV9990Tiles::initHexDigits()
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
	for (auto [c, p] : std::views::zip(glyphs, pixels)) {
		p = (c == ' ') ? ImColor(0.0f, 0.0f, 0.0f, 0.0f)  // transparent
		  : (c == '.') ? ImColor(0.0f, 0.0f, 0.0f, 0.7f)  // black semi-transparent outline
		               : ImColor(1.0f, 1.0f, 1.0f, 0.7f); // white semi-transparent
	}

	// and upload as a texture
	smallHexDigits.bind();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, totalWidth, charHeight, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
}

// Grid texture for a single pattern of 8x8 V9990 pixels, zoomed by (zx x zy).
void ImGuiV9990Tiles::updateGridTex(int zx, int zy)
{
	auto gColor = ImGui::ColorConvertFloat4ToU32(gridColor);
	auto gridWidth = 8 * zx;
	auto gridHeight = 8 * zy;
	MemBuffer<uint32_t> pixels(gridWidth * gridHeight);
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
			im::Group([&]{
				ImGui::TextUnformatted("V9990 settings"sv);
				im::Disabled(manMode, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Display mode: ", v9990_gui::toString(vdpMode));
				});
				im::Disabled(manPalA, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Palette A: ", getComboString(vdpPaletteA, paletteStr));
				});
				im::Disabled(manPalB, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Palette B: ", getComboString(vdpPaletteB, paletteStr));
				});
				ImGui::AlignTextToFramePadding();
				ImGui::StrCat("Scroll A: (", v9990->getScrollAX(), ", ", v9990->getScrollAY(), ')');
				im::Disabled(!isP1, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Scroll B: (", v9990->getScrollBX(), ", ", v9990->getScrollBY(), ')');
				});
			});
			ImGui::SameLine();
			im::Group([&]{
				ImGui::Checkbox("Manual override", &overrideAll);
				HelpMarker("Interpret VRAM using a manually selected mode and/or palette offset.\n"
				           "\n"
				           "Palette A and B are the two halves of the palette offset register (R#13):\n"
				           "* P1: palette of layer A and layer B\n"
				           "* P2: palette of the even and odd bytes in the pattern data");
				im::Group([&]{
					im::Disabled(overrideAll, [&]{
						ImGui::Checkbox("##o-mode",     overrideAll ? &overrideAll : &overrideMode);
						ImGui::Checkbox("##o-paletteA", overrideAll ? &overrideAll : &overridePaletteA);
						ImGui::Checkbox("##o-paletteB", overrideAll ? &overrideAll : &overridePaletteB);
					});
				});
				ImGui::SameLine();
				im::Group([&]{
					im::ItemWidth(ImGui::GetFontSize() * 9.0f, [&]{
						im::Disabled(!manMode, [&]{
							ImGui::Combo("##mode", &manualMode, "P1\000P2\000");
						});
						im::Disabled(!manPalA, [&]{
							ImGui::Combo("##paletteA", &manualPaletteA, paletteStr);
						});
						im::Disabled(!manPalB, [&]{
							ImGui::Combo("##paletteB", &manualPaletteB, paletteStr);
						});
					});
				});
			});

			ImGui::SameLine();
			ImGui::Dummy(ImVec2(25, 1));
			ImGui::SameLine();
			im::Group([&]{
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
				ImGui::Combo("Zoom", &zoom, "1x\0002x\0003x\0004x\0005x\0006x\0007x\0008x\000");
				ImGui::Checkbox("grid", &grid);
				ImGui::SameLine();
				im::Disabled(!grid, [&]{
					ImGui::ColorEdit4("Grid color", gridColor.data(),
						ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
				});
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
			if (!tex.get()) {
				tex = gl::Texture(false, false); // no interpolation, no wrapping
			}
			tex.bind();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, patTexWidth, patTexHeight, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
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
		if (grid) updateGridTex(zx, zy);
		static constexpr gl::vec2 digitSize{5.0f, 8.0f};
		bool showOverlay = nameTableOverlay && (charZoom.x >= 2.0f * digitSize.x) && (charZoom.y >= 2.0f * digitSize.y);
		if (showOverlay && !smallHexDigits.get()) {
			initHexDigits();
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
			return im::PopupContextWindow("PatternCopyPopup", [&]{
				if (ImGui::MenuItem("Copy pattern data to clipboard")) {
					auto text = formatPatternData(hoveredPattern[0], unsigned(hoveredPattern[1]));
					ImGui::SetClipboardText(text.c_str());
				}
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
			auto pos = ImGui::GetCursorPos();
			ImGui::Image(getPatternTex(layer).getImGui(), zoomCharSize, uv1, uv2);
			if (grid) {
				ImGui::SetCursorPos(pos);
				ImGui::Image(gridTex.getImGui(), zoomCharSize, {}, charSize);
			}
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
			gl::vec2 childSize{layerWidth(size.x), std::min(size.y, maxHeight) + scrollbar};
			for (auto layer : xrange(numLayers)) {
				if (layer) ImGui::SameLine();
				im::Group([&]{
					if (isP1) {
						ImGui::StrCat(layerName(layer), "  (palette ", layer ? palB : palA, ')');
					} else {
						ImGui::StrCat("Palette ", palA, " (even bytes), ", palB, " (odd bytes)");
					}
					im::Child(layer ? "##patternB" : "##patternA", childSize, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
						auto pos1 = ImGui::GetCursorPos();
						gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
						ImGui::Image(getPatternTex(layer).getImGui(), size);
						bool hovered = false;
						if (ImGui::IsItemHovered()) {
							auto gridPos = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / charZoom);
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
						if (grid) {
							ImGui::SetCursorPos(pos1);
							ImGui::Image(gridTex.getImGui(), size, {}, patTexChars);
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
			auto height = std::min(hostSize.y + scrollbar, std::max(availHeight, 16.0f * charZoom.y));
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
						auto numChars = (end.x - begin.x) * (end.y - begin.y);

						drawList->PushClipRect(scrnPos, scrnPos + hostSize, true);
						drawList->PushTexture(getPatternTex(layer).getImGui());
						drawList->PrimReserve(6 * numChars, 4 * numChars);
						for (auto row : xrange(begin.y, end.y)) {
							for (auto column : xrange(begin.x, end.x)) {
								gl::vec2 p1 = scrnPos + charZoom * gl::vec2{float(column), float(row)};
								auto [uv1, uv2] = getPatternUV(getPattern(layer, column, row));
								drawList->PrimRectUV(p1, p1 + charZoom, uv1, uv2, 0xffffffff);
							}
						}
						drawList->PopTexture();
						if (showOverlay) {
							drawList->PushTexture(smallHexDigits.getImGui());
							drawList->PrimReserve(4 * 6 * numChars, 4 * 4 * numChars);
							static constexpr float texDigitWidth = 1.0f / 16.0f;
							static constexpr gl::vec2 texDigitSize{texDigitWidth, 1.0f};
							for (auto row : xrange(begin.y, end.y)) {
								for (auto column : xrange(begin.x, end.x)) {
									gl::vec2 p = scrnPos + charZoom * gl::vec2{float(column), float(row)};
									auto pattern = getPattern(layer, column, row);
									for (auto i : xrange(4)) { // 4 digits in a 2x2 arrangement
										auto digit = (pattern >> (4 * (3 - i))) & 15;
										gl::vec2 p1 = p + digitSize * gl::vec2{float(i & 1), float(i >> 1)};
										gl::vec2 uv1{float(digit) * texDigitWidth, 0.0f};
										drawList->PrimRectUV(p1, p1 + digitSize, uv1, uv1 + texDigitSize, 0xffffffff);
									}
								}
							}
							drawList->PopTexture();
						}
						drawList->PopClipRect();

						auto pos1 = ImGui::GetCursorPos();
						ImGui::Dummy(hostSize);
						std::optional<gl::ivec2> gridPos;
						if (ImGui::IsItemHovered()) {
							auto p = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / charZoom);
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
						if (grid) {
							ImGui::SetCursorPos(pos1);
							ImGui::Image(gridTex.getImGui(), hostSize, {}, charsSize);
						}
					});
				});
			}
		});
	});
}

} // namespace openmsx
