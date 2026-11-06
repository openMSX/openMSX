#include "ImGuiV9990Sprites.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"
#include "ImGuiV9990Utils.hh"
#include "ImGuiVDPUtils.hh"

#include "MSXMotherBoard.hh"
#include "V9990.hh"
#include "V9990VRAM.hh"

#include "MemBuffer.hh"
#include "enumerate.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "xrange.hh"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace openmsx {

using namespace std::literals;

ImGuiV9990Sprites::ImGuiV9990Sprites(ImGuiManager& manager_, size_t index)
	: ImGuiPart(manager_)
	, title(strCat("V9990 sprite viewer", strCat_if(index, " (", index + 1, ')')))
{
}

void ImGuiV9990Sprites::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiV9990Sprites::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

// V9990 sprites, only available in P1 and P2 mode. Mirrors the logic in
// V9990PxConverter (renderSprites()).
// * 125 sprites, 16x16 pixels, 4bpp, attribute table at 0x3FE00 (4 bytes per
//   sprite: y, pattern, x (low), attributes).
// * Pattern table: 256 patterns, organized as a 4bpp bitmap. P1: 256 pixels
//   wide (16 patterns per row), P2: 512 pixels wide (32 patterns per row).
// * Lower sprite numbers have priority. Max 16 sprites per line (disabled
//   sprites also count towards this limit).
void ImGuiV9990Sprites::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;
	ImGui::SetNextWindowSize({748, 1010}, ImGuiCond_FirstUseEver);
	im::Window(title.c_str(), &show, [&]{
		auto* v9990 = dynamic_cast<V9990*>(motherBoard->findDevice("Sunrise GFX9000")); // TODO name based OK?
		if (!v9990) return;
		using enum V9990DisplayMode;

		auto vdpMode = v9990->getDisplayMode();
		bool vdpIsPattern = vdpMode == one_of(P1, P2);
		int vdpPatBase = v9990->getSpritePatternAddress(vdpMode);

		bool manMode = overrideAll || overrideMode;
		bool manPat  = overrideAll || overridePat;

		bool isPattern = manMode || vdpIsPattern;
		bool isP1 = manMode ? (manualMode == 0) : (vdpMode == P1);
		// P1: 0x00000-0x38000, P2: 0x00000-0x78000, in steps of 0x8000.
		// Only mask the shown value: the stored one may be meant for P2.
		int patMask = isP1 ? 0x38000 : 0x78000;
		int shownPatBase = manualPatBase & patMask;
		unsigned patBase = manPat ? unsigned(shownPatBase)
		                          : unsigned(v9990->getSpritePatternAddress(isP1 ? P1 : P2));
		auto patLabel = vdpIsPattern
			? strCat("Pattern table: 0x", hex_string<5>(vdpPatBase))
			: strCat("Pattern table: -");

		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			overridableSettings("V9990 settings", "Pattern table: 0x00000", overrideAll, {
				{tmpStrCat("Display mode: ", v9990_gui::toString(vdpMode)), &overrideMode, [&]{
					ImGui::Combo("##mode", &manualMode, "P1\000P2\000");
				}},
				{patLabel, &overridePat, [&]{
					comboHexSequence<5>("##pat", &shownPatBase, 0x8000, isP1 ? 0x40000 : 0x80000, 0);
					if (shownPatBase != (manualPatBase & patMask)) manualPatBase = shownPatBase;
				}},
			}, "Interpret VRAM using a manually selected mode and/or sprite pattern table address.");

			ImGui::SameLine();
			ImGui::Dummy(ImVec2(25, 1));
			ImGui::SameLine();
			im::Group([&]{
				zoomAndGridSettings(zoom, grid, gridColor);
				checkerBoardSettings("Used as background in all views",
				                     checkerBoardColor1, checkerBoardColor2, checkerBoardSize);
			});
		});
		ImGui::Separator();

		if (!isPattern) {
			ImGui::TextUnformatted("No sprites in this display mode (only in P1 and P2). Use manual override to show the VRAM content anyway."sv);
			return;
		}

		auto palette64 = v9990_gui::getPalette64(*v9990);
		const auto& vram = v9990->getVRAM();
		static constexpr int NUM_SPRITES = 125;
		static constexpr unsigned ATTRIB_TABLE = 0x3FE00;
		int patsPerRow = isP1 ? 16 : 32;
		unsigned lineBytes = isP1 ? 128 : 256;
		auto getPatternAddr = [&](int pattern, int y) {
			return patBase + lineBytes * (16 * (pattern / patsPerRow) + y) + 8 * (pattern % patsPerRow);
		};
		auto readPatternByte = [&](unsigned addr) {
			return isP1 ? vram.readVRAMP1(addr) : vram.readVRAMBx(addr);
		};
		// returns the 4-bit color index of a pixel in a sprite pattern
		auto getPatternPixel = [&](int pattern, int x, int y) {
			auto value = readPatternByte(getPatternAddr(pattern, y) + x / 2);
			return (x & 1) ? (value & 0x0f) : (value >> 4);
		};

		struct SpriteAttr {
			unsigned addr;
			uint8_t rawY, rawX, rawAttr;
			int x; // -16..1008
			int y; // first display line (wraps at 256)
			int pattern;
			int palette; // 0-3
			bool back; // behind the foreground layer(s)
			bool disabled;
		};
		auto getSprite = [&](int sprite) {
			unsigned addr = ATTRIB_TABLE + 4 * sprite;
			uint8_t rawY    = vram.readVRAMDirect(addr + 0);
			uint8_t pattern = vram.readVRAMDirect(addr + 1);
			uint8_t rawX    = vram.readVRAMDirect(addr + 2);
			uint8_t attr    = vram.readVRAMDirect(addr + 3);
			int x = rawX + 256 * (attr & 0x03);
			if (x > 1008) x -= 1024;
			return SpriteAttr{
				.addr = addr, .rawY = rawY, .rawX = rawX, .rawAttr = attr,
				.x = x, .y = (rawY + 1) & 255, .pattern = pattern,
				.palette = attr >> 6, .back = bool(attr & 0x20), .disabled = bool(attr & 0x10)};
		};
		std::array<SpriteAttr, NUM_SPRITES> sprites;
		for (auto i : xrange(NUM_SPRITES)) sprites[i] = getSprite(i);

		// clipboard helpers
		// 16 lines of 8 bytes (16 pixels of 4 bits), one line per row
		auto formatPatternData = [&](int pattern) {
			auto addr = getPatternAddr(pattern, 0);
			return formatToString([&](unsigned i) { return readPatternByte(addr + (i / 8) * lineBytes + (i % 8)); },
			                      0, 127, {}, 8, {}, ", ", "%02X", manager.getInterpreter());
		};
		auto formatAttribData = [&](int sprite) {
			auto addr = sprites[sprite].addr;
			return formatToString([&](unsigned a) { return vram.readVRAMDirect(a); },
			                      addr, addr + 3, {}, 4, {}, ", ", "%02X", manager.getInterpreter());
		};

		// P1: 256 pixels per line, P2: 512 pixels per line
		auto zm = gl::vec2{float((1 + zoom) * (isP1 ? 2 : 1)), float((1 + zoom) * 2)};
		static constexpr auto sprSize = gl::vec2{16.0f, 16.0f};
		auto zoomSprSize = zm * sprSize;
		auto previewSize = 3.0f * zoomSprSize;
		auto gColor = ImGui::ColorConvertFloat4ToU32(gridColor);
		if (grid) {
			updateGridTexture(gridTex, {int(zoomSprSize.x), int(zoomSprSize.y)}, gColor);
			updateGridTexture(zoomGridTex, {int(previewSize.x / 16.0f), int(previewSize.y / 16.0f)}, gColor);
		}
		if (checkerBoardSize) {
			updateCheckerBoardTexture(checkerTex,
			                          ImGui::ColorConvertFloat4ToU32(checkerBoardColor1),
			                          ImGui::ColorConvertFloat4ToU32(checkerBoardColor2));
		}
		const gl::Texture* gridOverlay = grid ? &gridTex : nullptr;
		auto drawPreview = [&](const gl::Texture& tex, gl::vec2 uv1, gl::vec2 uv2) {
			auto pos = ImGui::GetCursorPos();
			drawCheckerBoard(checkerTex, previewSize, checkerBoardSize);
			ImGui::SetCursorPos(pos);
			imageWithGrid(tex, previewSize, uv1, uv2, grid ? &zoomGridTex : nullptr, sprSize);
		};

		// Sprite patterns, all shown with the same (selectable) palette
		im::TreeNode("Sprite patterns", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			int patRows = 256 / patsPerRow;
			int texW = 16 * patsPerRow;
			int texH = 16 * patRows;
			MemBuffer<uint32_t> pixels(texW * texH);
			auto palette16 = subspan<16>(palette64, 16 * patternPalette);
			for (auto pattern : xrange(256)) {
				int px = 16 * (pattern % patsPerRow);
				int py = 16 * (pattern / patsPerRow);
				for (auto y : xrange(16)) {
					for (auto x : xrange(16)) {
						auto p = getPatternPixel(pattern, x, y);
						pixels[(py + y) * texW + px + x] = p ? palette16[p] : 0;
					}
				}
			}
			updateTexture(patternTex, {texW, texH}, pixels);

			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0f);
			ImGui::Combo("Palette", &patternPalette, v9990_gui::paletteStr);
			HelpMarker("Sprites select their palette in the attribute table, this palette is only used to show the pattern table.");

			auto patChars = gl::vec2{float(patsPerRow), float(patRows)};
			auto fullSize = patChars * zoomSprSize;
			im::Child("##pattern", {0, childHeightWithScrollbar(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto pos1 = ImGui::GetCursorPos();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				drawCheckerBoard(checkerTex, fullSize, checkerBoardSize);
				ImGui::SetCursorPos(pos1);
				imageWithGrid(patternTex, fullSize, {}, {1.0f, 1.0f}, gridOverlay, patChars);
				bool hovered = false;
				if (ImGui::IsItemHovered()) {
					auto gridPos = hoveredCell(scrnPos, zoomSprSize);
					if ((0 <= gridPos.x) && (gridPos.x < patsPerRow) && (0 <= gridPos.y) && (gridPos.y < patRows)) {
						hovered = true;
						hoveredPattern = patsPerRow * gridPos.y + gridPos.x;
					}
				}
				copyToClipboardPopup("CopySpriteDataPopup", "Copy pattern data to clipboard", [&]{
					return formatPatternData(hoveredPattern);
				});
				if (hovered) {
					im::Tooltip([&]{
						auto pattern = hoveredPattern;
						ImGui::StrCat("pattern: ", pattern, " (0x", hex_string<2>(pattern), ')');
						ImGui::StrCat("address: 0x", hex_string<5>(getPatternAddr(pattern, 0)),
						              " (+0x", hex_string<3>(lineBytes), " per line)");
						auto recipChars = recip(patChars);
						auto uv1 = gl::vec2{float(pattern % patsPerRow), float(pattern / patsPerRow)} * recipChars;
						drawPreview(patternTex, uv1, uv1 + recipChars);
					});
				}
			});
		});
		ImGui::Separator();

		// Sprite attributes: each sprite shown with its own palette, disabled sprites are dimmed
		im::TreeNode("Sprite attributes", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			static constexpr int COLUMNS = 16;
			static constexpr int ROWS = (NUM_SPRITES + COLUMNS - 1) / COLUMNS;
			static constexpr int texW = 16 * COLUMNS;
			static constexpr int texH = 16 * ROWS;
			MemBuffer<uint32_t> pixels(texW * texH);
			std::ranges::fill(std::span{pixels.data(), size_t(texW * texH)}, 0);
			for (const auto& [i, spr] : enumerate(sprites)) {
				int px = 16 * (int(i) % COLUMNS);
				int py = 16 * (int(i) / COLUMNS);
				auto palette16 = subspan<16>(palette64, 16 * spr.palette);
				for (auto y : xrange(16)) {
					for (auto x : xrange(16)) {
						auto p = getPatternPixel(spr.pattern, x, y);
						if (!p) continue;
						auto col = palette16[p];
						if (spr.disabled) col = (col & 0x00ffffff) | 0x60000000;
						pixels[(py + y) * texW + px + x] = col;
					}
				}
			}
			updateTexture(attribTex, {texW, texH}, pixels);

			auto sprChars = gl::vec2{float(COLUMNS), float(ROWS)};
			auto fullSize = sprChars * zoomSprSize;
			im::Child("##attrib", {0, childHeightWithScrollbar(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto pos1 = ImGui::GetCursorPos();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				drawCheckerBoard(checkerTex, fullSize, checkerBoardSize);
				ImGui::SetCursorPos(pos1);
				imageWithGrid(attribTex, fullSize, {}, {1.0f, 1.0f}, gridOverlay, sprChars);
				bool hovered = false;
				if (ImGui::IsItemHovered()) {
					auto gridPos = hoveredCell(scrnPos, zoomSprSize);
					int sprite = COLUMNS * gridPos.y + gridPos.x;
					if ((0 <= gridPos.x) && (gridPos.x < COLUMNS) && (0 <= gridPos.y) && (sprite < NUM_SPRITES)) {
						hovered = true;
						hoveredSprite = sprite;
					}
				}
				copyToClipboardPopup("CopySpriteDataPopup", "Copy sprite attribute data to clipboard", [&]{
					return formatAttribData(hoveredSprite);
				});
				if (hovered) {
					im::Tooltip([&]{
						auto sprite = hoveredSprite;
						const auto& spr = sprites[sprite];
						ImGui::StrCat("sprite: ", sprite);
						ImGui::StrCat("address: 0x", hex_string<5>(spr.addr));
						auto recipChars = recip(sprChars);
						auto uv1 = gl::vec2{float(sprite % COLUMNS), float(sprite / COLUMNS)} * recipChars;
						drawPreview(attribTex, uv1, uv1 + recipChars);
						ImGui::StrCat("x: ", spr.x, "  y: ", spr.rawY);
						ImGui::StrCat("pattern: ", spr.pattern, " (0x", hex_string<2>(spr.pattern), ')');
						ImGui::StrCat("palette: ", spr.palette, " (", 16 * spr.palette, '-', 16 * spr.palette + 15, ')');
						ImGui::StrCat("priority: ", spr.back ? "back"sv : "front"sv);
						ImGui::StrCat("disabled: ", spr.disabled ? "yes"sv : "no"sv);
						ImGui::StrCat("raw: ", hex_string<2>(spr.rawY), ' ', hex_string<2>(spr.pattern), ' ',
						              hex_string<2>(spr.rawX), ' ', hex_string<2>(spr.rawAttr));
					});
				}
			});
		});
		ImGui::Separator();

		// Rendered sprites (without the background layers)
		im::TreeNode("Rendered sprites", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			static constexpr int LINES = 256; // y-coordinates wrap at 256
			int width = isP1 ? 256 : 512;
			MemBuffer<uint32_t> screen(width * LINES);
			std::ranges::fill(std::span{screen.data(), size_t(width * LINES)}, 0); // transparent
			for (auto line : xrange(LINES)) {
				auto* lineBuf = &screen[width * line];
				int limit = 16;
				int count = 0;
				for (const auto& spr : sprites) {
					auto sprY = narrow_cast<uint8_t>(line - spr.y);
					if (sprY >= 16) continue;
					if (spr.disabled) {
						--limit; // disabled sprites do count towards the limit
					} else {
						++count;
						auto palette16 = subspan<16>(palette64, 16 * spr.palette);
						for (auto x : xrange(16)) {
							int xx = spr.x + x;
							if ((xx < 0) || (xx >= width)) continue;
							if (lineBuf[xx]) continue; // lower sprite numbers have priority
							if (auto p = getPatternPixel(spr.pattern, x, sprY)) {
								lineBuf[xx] = palette16[p];
							}
						}
					}
					if (enableLimitPerLine && (count == limit)) break;
				}
			}
			updateTexture(renderTex, {width, LINES}, screen, true); // with wrapping

			// bounding boxes, clipped to the screen, split when wrapping at y=256.
			// 'boxes' includes disabled sprites (they still show a box when hovered);
			// 'enabledBoxes' is what "on all sprites" draws.
			std::vector<SpriteBox> boxes;
			std::vector<SpriteBox> enabledBoxes;
			for (const auto& [i, spr] : enumerate(sprites)) {
				int x = spr.x;
				int w = 16;
				if (!clipSpriteBoxSide(x, w, width)) continue;
				auto add = [&](int y, int h) {
					SpriteBox box{x, y, w, h, int(i), 0, 0, 0}; // the remaining fields are only for printSpriteBoxes()
					boxes.push_back(box);
					if (!spr.disabled) enabledBoxes.push_back(box);
				};
				int yEnd = spr.y + 16;
				if (yEnd <= LINES) {
					add(spr.y, 16);
				} else {
					add(spr.y, LINES - spr.y);
					add(0, yEnd - LINES);
				}
			}

			auto fullSize = zm * gl::vec2{float(width), float(LINES)};
			im::Child("##screen", {0.0f, childHeightWithScrollbar(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto* drawList = ImGui::GetWindowDrawList();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				auto topLeft = ImGui::GetCursorPos();

				drawCheckerBoard(checkerTex, fullSize, checkerBoardSize);
				ImGui::SetCursorPos(topLeft);
				ImGui::Image(renderTex.getImGui(), fullSize);
				bool hovered = ImGui::IsItemHovered();
				auto hoverPos = hoveredCell(scrnPos, zm);
				ImGui::SameLine();

				im::Group([&]{
					spriteBoxSettings(drawBoundingBox, boundingBoxColor, boundingBoxOnAll);
					ImGui::Checkbox("Max 16 sprites per line", &enableLimitPerLine);
				});

				if (hovered) {
					auto [hx, hy] = hoverPos;
					im::Tooltip([&]{
						ImGui::StrCat("x,y=", hx, ',', hy);
						for (const auto& b : boxes) {
							if (!b.contains(hoverPos)) continue;
							const auto& spr = sprites[b.sprite];
							ImGui::Bullet();
							ImGui::StrCat("sprite ", b.sprite, ": pat=", spr.pattern, " (0x", hex_string<2>(spr.pattern), ")\n"
							              "x,y=", spr.x, ',', spr.rawY, " pal=", spr.palette,
							              spr.back ? " back"sv : " front"sv,
							              spr.disabled ? " disabled"sv : ""sv);
						}
					});
				}
				if (drawBoundingBox) {
					const auto& shown = (boundingBoxOnAll == 1) ? enabledBoxes : boxes;
					drawSpriteBoxes(drawList, scrnPos, zm, shown, boundingBoxColor,
					                boundingBoxOnAll == 1, hovered, hoverPos);
				}
			});

			// Disabled sprites also take one of the 16 slots of a line, so count
			// all sprites; when there are more than 16, the later ones are dropped.
			std::array<float, LINES> perLine = {};
			for (auto line : xrange(LINES)) {
				for (const auto& spr : sprites) {
					if (narrow_cast<uint8_t>(line - spr.y) < 16) ++perLine[line];
				}
			}
			auto busiest = std::ranges::max_element(perLine);
			auto overLimit = std::ranges::count_if(perLine, [](float n) { return n > 16.0f; });
			ImGui::StrCat("Sprites per line: at most ", int(*busiest), " (line ", busiest - perLine.begin(),
			              "), ", overLimit, " line", (overLimit == 1 ? "" : "s"), " over the limit of 16");
			HelpMarker("The number of sprites on each display line (0-255), including disabled\n"
			           "sprites: they are not drawn, but do take one of the 16 sprites per line.\n"
			           "On lines with more than 16 sprites the higher sprite numbers are not drawn.");
			ImGui::PlotHistogram("##perLine", perLine.data(), LINES, 0, nullptr,
			                     0.0f, std::max(16.0f, *busiest),
			                     gl::vec2{fullSize.x, 4.0f * ImGui::GetTextLineHeightWithSpacing()});
		});
	});
}

} // namespace openmsx
