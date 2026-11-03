#include "ImGuiV9990Sprites.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"
#include "ImGuiV9990Utils.hh"

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

// Grid texture for a single cell of (w x h) host pixels.
void ImGuiV9990Sprites::updateGridTex(gl::Texture& tex, int w, int h)
{
	auto gColor = ImGui::ColorConvertFloat4ToU32(gridColor);
	MemBuffer<uint32_t> pixels(w * h);
	for (auto y : xrange(h)) {
		auto* line = &pixels[y * w];
		for (auto x : xrange(w)) {
			line[x] = (x == 0 || y == 0) ? gColor : 0;
		}
	}
	if (!tex.get()) {
		tex = gl::Texture(false, true); // no interpolation, with wrapping
	}
	tex.bind();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
}

void ImGuiV9990Sprites::updateCheckerTex()
{
	std::array<uint32_t, 4> pixels;
	pixels[0] = pixels[3] = ImGui::ColorConvertFloat4ToU32(checkerBoardColor1);
	pixels[1] = pixels[2] = ImGui::ColorConvertFloat4ToU32(checkerBoardColor2);
	if (!checkerTex.get()) {
		checkerTex = gl::Texture(false, true); // no interpolation, with wrapping
	}
	checkerTex.bind();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
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

		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			im::Group([&]{
				ImGui::TextUnformatted("V9990 settings"sv);
				im::Disabled(manMode, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Display mode: ", v9990_gui::toString(vdpMode));
				});
				im::Disabled(manPat, [&]{
					ImGui::AlignTextToFramePadding();
					if (vdpIsPattern) {
						ImGui::StrCat("Pattern table: 0x", hex_string<5>(vdpPatBase));
					} else {
						ImGui::TextUnformatted("Pattern table: -"sv);
					}
				});
			});
			ImGui::SameLine();
			im::Group([&]{
				ImGui::Checkbox("Manual override", &overrideAll);
				HelpMarker("Interpret VRAM using a manually selected mode and/or sprite pattern table address.");
				im::Group([&]{
					im::Disabled(overrideAll, [&]{
						ImGui::Checkbox("##o-mode", overrideAll ? &overrideAll : &overrideMode);
						ImGui::Checkbox("##o-pat",  overrideAll ? &overrideAll : &overridePat);
					});
				});
				ImGui::SameLine();
				im::Group([&]{
					im::ItemWidth(ImGui::GetFontSize() * 9.0f, [&]{
						im::Disabled(!manMode, [&]{
							ImGui::Combo("##mode", &manualMode, "P1\000P2\000");
						});
						im::Disabled(!manPat, [&]{
							comboHexSequence<5>("##pat", &shownPatBase, 0x8000, isP1 ? 0x40000 : 0x80000, 0);
							if (shownPatBase != (manualPatBase & patMask)) manualPatBase = shownPatBase;
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

				ImGui::TextUnformatted("Checkerboard:"sv);
				simpleToolTip("Used as background in all views");
				ImGui::SameLine();
				auto checkerBoardColorFlags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha;
				ImGui::ColorEdit4("checkerboard color1", checkerBoardColor1.data(), checkerBoardColorFlags);
				ImGui::SameLine();
				ImGui::ColorEdit4("checkerboard color2", checkerBoardColor2.data(), checkerBoardColorFlags);
				im::Indent([&]{
					ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
					ImGui::InputInt("size", &checkerBoardSize);
					checkerBoardSize = std::clamp(checkerBoardSize, 0, maxCheckerBoardSize);
				});
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
		auto copyDataPopup = [&](zstring_view text, function_ref<std::string()> supplier) {
			return im::PopupContextWindow("CopySpriteDataPopup", [&]{
				if (ImGui::MenuItem(text.c_str())) {
					ImGui::SetClipboardText(supplier().c_str());
				}
			});
		};
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
		if (grid) {
			updateGridTex(gridTex, int(zoomSprSize.x), int(zoomSprSize.y));
			updateGridTex(zoomGridTex, int(previewSize.x / 16.0f), int(previewSize.y / 16.0f));
		}
		if (checkerBoardSize) updateCheckerTex();
		auto drawChecker = [&](gl::vec2 size) {
			if (!checkerBoardSize) return;
			auto pos = ImGui::GetCursorPos();
			ImGui::Image(checkerTex.getImGui(), size, {}, size / (4.0f * float(checkerBoardSize)));
			ImGui::SetCursorPos(pos);
		};
		auto drawGrid = [&](gl::vec2 pos, const gl::Texture& tex, gl::vec2 size, gl::vec2 repeat) {
			if (!grid) return;
			ImGui::SetCursorPos(pos);
			ImGui::Image(tex.getImGui(), size, {}, repeat);
		};
		auto drawPreview = [&](const gl::Texture& tex, gl::vec2 uv1, gl::vec2 uv2) {
			auto pos = ImGui::GetCursorPos();
			drawChecker(previewSize);
			ImGui::Image(tex.getImGui(), previewSize, uv1, uv2);
			drawGrid(pos, zoomGridTex, previewSize, sprSize);
		};
		auto upload = [&](gl::Texture& tex, int w, int h, const uint32_t* data) {
			if (!tex.get()) {
				tex = gl::Texture(false, false); // no interpolation, no wrapping
			}
			tex.bind();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		};
		// Height for the scrollable section children: the content height plus
		// room for the horizontal scrollbar, so that showing the scrollbar
		// does not trigger a vertical one.
		auto childHeight = [](float contentHeight) {
			return contentHeight + ImGui::GetStyle().ScrollbarSize;
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
			upload(patternTex, texW, texH, pixels.data());

			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0f);
			ImGui::Combo("Palette", &patternPalette, v9990_gui::paletteStr);
			HelpMarker("Sprites select their palette in the attribute table, this palette is only used to show the pattern table.");

			auto patChars = gl::vec2{float(patsPerRow), float(patRows)};
			auto fullSize = patChars * zoomSprSize;
			im::Child("##pattern", {0, childHeight(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto pos1 = ImGui::GetCursorPos();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				drawChecker(fullSize);
				ImGui::Image(patternTex.getImGui(), fullSize);
				bool hovered = false;
				if (ImGui::IsItemHovered()) {
					auto gridPos = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / zoomSprSize);
					if ((0 <= gridPos.x) && (gridPos.x < patsPerRow) && (0 <= gridPos.y) && (gridPos.y < patRows)) {
						hovered = true;
						hoveredPattern = patsPerRow * gridPos.y + gridPos.x;
					}
				}
				copyDataPopup("Copy pattern data to clipboard", [&]{
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
				drawGrid(pos1, gridTex, fullSize, patChars);
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
			upload(attribTex, texW, texH, pixels.data());

			auto sprChars = gl::vec2{float(COLUMNS), float(ROWS)};
			auto fullSize = sprChars * zoomSprSize;
			im::Child("##attrib", {0, childHeight(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto pos1 = ImGui::GetCursorPos();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				drawChecker(fullSize);
				ImGui::Image(attribTex.getImGui(), fullSize);
				bool hovered = false;
				if (ImGui::IsItemHovered()) {
					auto gridPos = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / zoomSprSize);
					int sprite = COLUMNS * gridPos.y + gridPos.x;
					if ((0 <= gridPos.x) && (gridPos.x < COLUMNS) && (0 <= gridPos.y) && (sprite < NUM_SPRITES)) {
						hovered = true;
						hoveredSprite = sprite;
					}
				}
				copyDataPopup("Copy sprite attribute data to clipboard", [&]{
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
				drawGrid(pos1, gridTex, fullSize, sprChars);
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
			if (!renderTex.get()) {
				renderTex = gl::Texture(false, true); // no interpolation, with wrapping
			}
			renderTex.bind();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, LINES, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, screen.data());

			// bounding boxes, clipped to the screen, split when wrapping at y=256
			struct Box { int x, y, w, h, sprite; };
			std::vector<Box> boxes;
			for (const auto& [i, spr] : enumerate(sprites)) {
				int x = std::max(spr.x, 0);
				int xEnd = std::min(spr.x + 16, width);
				if (x >= xEnd) continue;
				int yEnd = spr.y + 16;
				if (yEnd <= LINES) {
					boxes.push_back({x, spr.y, xEnd - x, 16, int(i)});
				} else {
					boxes.push_back({x, spr.y, xEnd - x, LINES - spr.y, int(i)});
					boxes.push_back({x, 0, xEnd - x, yEnd - LINES, int(i)});
				}
			}

			auto fullSize = zm * gl::vec2{float(width), float(LINES)};
			im::Child("##screen", {0.0f, childHeight(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto* drawList = ImGui::GetWindowDrawList();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				auto boxColor = ImGui::ColorConvertFloat4ToU32(boundingBoxColor);
				auto drawBox = [&](const Box& b) {
					gl::vec2 tl = scrnPos + gl::vec2{float(b.x), float(b.y)} * zm;
					gl::vec2 br = tl + gl::vec2{float(b.w), float(b.h)} * zm;
					drawList->AddRect(tl, br, boxColor);
				};

				drawChecker(fullSize);
				ImGui::Image(renderTex.getImGui(), fullSize);
				bool hovered = ImGui::IsItemHovered();
				auto hoverPos = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / zm);
				ImGui::SameLine();

				im::Group([&]{
					ImGui::Checkbox("Bounding box", &drawBoundingBox);
					im::Indent([&]{
						im::Disabled(!drawBoundingBox, [&]{
							ImGui::ColorEdit4("color", boundingBoxColor.data(),
								ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
							ImGui::RadioButton("On hovered sprites", &boundingBoxOnAll, 0);
							ImGui::RadioButton("On all sprites", &boundingBoxOnAll, 1);
						});
					});
					ImGui::Checkbox("Max 16 sprites per line", &enableLimitPerLine);
				});

				if (hovered) {
					auto [hx, hy] = hoverPos;
					auto isHit = [&](const Box& b) {
						return (b.x <= hx) && (hx < (b.x + b.w)) &&
						       (b.y <= hy) && (hy < (b.y + b.h));
					};
					im::Tooltip([&]{
						ImGui::StrCat("x,y=", hx, ',', hy);
						for (const auto& b : boxes) {
							if (!isHit(b)) continue;
							const auto& spr = sprites[b.sprite];
							ImGui::Bullet();
							ImGui::StrCat("sprite ", b.sprite, ": pat=", spr.pattern, " (0x", hex_string<2>(spr.pattern), ")\n"
							              "x,y=", spr.x, ',', spr.rawY, " pal=", spr.palette,
							              spr.back ? " back"sv : " front"sv,
							              spr.disabled ? " disabled"sv : ""sv);
						}
					});
					if (drawBoundingBox && (boundingBoxOnAll == 0)) {
						for (const auto& b : boxes) {
							if (isHit(b)) drawBox(b);
						}
					}
				}

				if (drawBoundingBox && (boundingBoxOnAll == 1)) {
					for (const auto& b : boxes) {
						if (!sprites[b.sprite].disabled) drawBox(b);
					}
				}
			});
		});
	});
}

} // namespace openmsx
