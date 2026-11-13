#include "ImGuiSMSVDPSprites.hh"

#include "Franky.hh"
#include "SMSVDP.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiSMSVDPUtils.hh"
#include "ImGuiUtils.hh"
#include "ImGuiVDPUtils.hh"

#include "MSXMotherBoard.hh"

#include "narrow.hh"
#include "ranges.hh"
#include "static_vector.hh"
#include "xrange.hh"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <span>

namespace openmsx {

using namespace std::literals;

ImGuiSMSVDPSprites::ImGuiSMSVDPSprites(ImGuiManager& manager_, size_t index)
	: ImGuiPart(manager_)
	, title(strCat("SMS VDP sprite viewer", strCat_if(index, " (", index + 1, ')')))
{
}

void ImGuiSMSVDPSprites::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiSMSVDPSprites::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

// The 8 pens (0-15) of one mode 4 pattern row: four bytes, one per bit
// plane, starting at 'addr'.
static std::array<uint8_t, 8> mode4Pens(std::span<const uint8_t, 0x4000> vram, unsigned addr)
{
	return mode4Pens(vram[(addr + 0) & 0x3fff], vram[(addr + 1) & 0x3fff],
	                 vram[(addr + 2) & 0x3fff], vram[(addr + 3) & 0x3fff]);
}

// Mode 4 sprite patterns are 4bpp and always use CRAM palette 1.
// 'pen0' is the color used for the (transparent) pen 0 pixels. Sprites
// address the 256 tiles of one bank (0x0000 or 0x2000, R#6 bit 2).
static void renderPatternsSms8(std::span<const uint8_t, 0x4000> vram, unsigned patBase,
                               std::span<const uint32_t, 32> palette, uint32_t pen0,
                               std::span<uint32_t> output)
{
	for (auto tile : xrange(256)) {
		auto charRow = tile / 32;
		auto charCol = tile % 32;
		for (auto y : xrange(8)) {
			auto pens = mode4Pens(vram, patBase + tile * 32 + y * 4);
			for (auto x : xrange(8)) {
				output[(charRow * 8 + y) * 256 + charCol * 8 + x] =
					(pens[x] == 0) ? pen0 : palette[16 + pens[x]];
			}
		}
	}
}

static void renderPatternsSms16(std::span<const uint8_t, 0x4000> vram, unsigned patBase,
                                std::span<const uint32_t, 32> palette, uint32_t pen0,
                                std::span<uint32_t> output)
{
	// SMS sprites are 8 pixels wide and 16 lines tall (the size bit only
	// doubles the height); each sprite is a vertical pair of tiles
	// (2s, 2s+1). 128 sprites in a 32x4 grid (256x64).
	for (auto sprite : xrange(128)) {
		auto charRow = sprite / 32;
		auto charCol = sprite % 32;
		for (auto y : xrange(16)) {
			auto tile = 2 * sprite + (y / 8);
			auto pens = mode4Pens(vram, patBase + tile * 32 + (y & 7) * 4);
			for (auto x : xrange(8)) {
				output[(charRow * 16 + y) * 256 + charCol * 8 + x] =
					(pens[x] == 0) ? pen0 : palette[16 + pens[x]];
			}
		}
	}
}

void ImGuiSMSVDPSprites::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;
	ImGui::SetNextWindowSize({748, 1010}, ImGuiCond_FirstUseEver);
	im::Window(title.c_str(), &show, [&]{
		auto* franky = dynamic_cast<Franky*>(motherBoard->findDevice("Franky"));
		if (!franky) return;
		auto& vdp = franky->getVdp();
		const auto vram = vdp.getVram();
		std::array<uint8_t, 8> reg;
		for (auto i : xrange(8)) reg[i] = vdp.peekReg(i);
		int vdpMode = vdp.peekMode();
		bool sms1 = vdp.getVariant() == SMSVDPCore::Variant::SMS1;
		int vdpYpixels = vdp.peekYPixels();

		int vdpType = (vdpMode == 1) ? NONE : (vdpMode == 4) ? SMS : TMS;
		auto typeToStr = [](int type) {
			if (type == NONE) return "None";
			if (type == TMS)  return "1";
			if (type == SMS)  return "SMS";
			assert(false); return "ERROR";
		};
		auto sizeToStr = [](int type, int size) {
			if (size == 8) return "8 x 8";
			// SMS sprites are 8 pixels wide and 16 lines tall; the TMS9918
			// ones are 16x16.
			if (size == 16) return (type == SMS) ? "8 x 16" : "16 x 16";
			assert(false); return "ERROR";
		};
		int vdpSize = (reg[1] & 0x02) ? 16 : 8;
		int vdpMag = (reg[1] & 0x01) ? 1 : 0;
		int vdpTransparent = 1; // color 0 is always transparent on the SMS VDP
		// Table bases as the chip derives them from the registers for sprite
		// type 't'. The settings readout uses the chip's type, the views use
		// the type that is shown (these differ when the type is overridden).
		// In mode 4 the pattern table is the sprite tile bank (R#6 bit 2).
		auto patBaseFor = [&](int t) {
			return (t == SMS) ? ((reg[6] & 0x04) << 11) : ((reg[6] & 0x07) << 11);
		};
		auto attBaseFor = [&](int t) {
			return (t == SMS) ? ((reg[5] << 7) & 0x3f00)
			                  : ((reg[5] & 0x7f) << 7);
		};
		// The alignment of the bases that the registers can select.
		auto patMult = [](int t) { return (t == SMS) ? 0x2000 : 0x800; }; // SMS: the two tile banks
		auto attMult = [](int t) { return (t == SMS) ? 0x100 : 0x80; };
		int vdpPatBase = patBaseFor(vdpType);
		int vdpAttBase = attBaseFor(vdpType);
		int vdpLines = (vdpType == SMS) ? vdpYpixels : 192;
		auto vramSize = 0x4000;
		bool manType   = overrideAll || overrideType;
		bool manSize   = overrideAll || overrideSize;
		bool manMag    = overrideAll || overrideMag;
		bool manTrans  = overrideAll || overrideTrans;
		bool manPat    = overrideAll || overridePat;
		bool manAtt    = overrideAll || overrideAtt;
		bool manLines  = overrideAll || overrideLines;

		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			// The type actually shown. manualType stays at its default
			// unless the type itself is overridden.
			auto shownType = [&] {
				return (overrideAll || overrideType) ? manualType : vdpType;
			};
			overridableSettings("SMS VDP settings", {}, overrideAll, {
				{tmpStrCat("Sprite type: ", typeToStr(vdpType)), &overrideType, [&]{
					im::Combo("##type", typeToStr(manualType), [&]{
						if (ImGui::Selectable("1"))   manualType = TMS;
						if (ImGui::Selectable("SMS")) manualType = SMS;
					});
				}},
				{tmpStrCat("Sprite size: ", sizeToStr(vdpType, vdpSize)), &overrideSize, [&]{
					im::Combo("##size", sizeToStr(shownType(), manualSize), [&]{
						if (ImGui::Selectable(" 8 x  8")) manualSize =  8;
						if (ImGui::Selectable((shownType() == SMS) ? " 8 x 16" : "16 x 16")) manualSize = 16;
					});
				}},
				{tmpStrCat("Sprites magnified: ", yesNo(vdpMag)), &overrideMag, [&]{
					ImGui::Combo("##mag", &manualMag, "no\000yes\000");
				}},
				{tmpStrCat("Color 0 transparent: ", yesNo(vdpTransparent)), &overrideTrans, [&]{
					ImGui::Combo("##trans", &manualTransparent, "no\000yes\000");
				}},
				{tmpStrCat("Pattern table: 0x", hex_string<4>(vdpPatBase)), &overridePat, [&]{
					comboHexSequence<4>("##pat", &manualPatBase, patMult(shownType()), vramSize, 0);
				}},
				{tmpStrCat("Attribute table: 0x", hex_string<4>(vdpAttBase)), &overrideAtt, [&]{
					comboHexSequence<4>("##att", &manualAttBase, attMult(shownType()), vramSize, 0);
				}},
				// Only mode 4 can show more than 192 lines.
				{.vdpValue = tmpStrCat("Visible lines: ", vdpLines),
				 .overridden = &overrideLines,
				 .editor = [&]{
					im::Disabled(shownType() != SMS, [&]{
						ImGui::Combo("##lines", &manualLines, "192\000224\000240\000");
					});
				 },
				 .vdpValueUsed = vdpType == SMS},
			});
			ImGui::SameLine();
			ImGui::Dummy(ImVec2(25, 1));
			ImGui::SameLine();
			im::Group([&]{
				// One zoom for all sections, like the MSX VDP sprite viewer.
				zoomAndGridSettings(zoom, grid, gridColor);

				checkerBoardSettings("Used as background for transparent pixels and in the "
				                     "'Rendered sprites' view. Pick both colors equal for a "
				                     "solid background.",
				                     checkerBoardColor1, checkerBoardColor2, checkerBoardSize);
			});
		});

		int type = manType ? manualType : vdpType;
		int size = manSize ? manualSize : vdpSize;
		int mag  = manMag ? manualMag  : vdpMag;
		int transparent = manTrans ? manualTransparent : vdpTransparent;
		int lines = (type == SMS)
			? (manLines ? (manualLines == 0 ? 192 :
			               manualLines == 1 ? 224 :
			                                  240)
			            : vdpLines)
			: 192; // the TMS9918-compatible modes always show 192 lines
		// The combos above only align the manual bases while they're shown.
		int patBase = manPat ? (manualPatBase & ~(patMult(type) - 1)) : patBaseFor(type);
		int attBase = manAtt ? (manualAttBase & ~(attMult(type) - 1)) : attBaseFor(type);

		// Palette: TMS9918 colors (modes 0-3) or the 32 CRAM colors (mode 4;
		// sprites always use palette 1 = entries 16-31).
		auto palette = getSMSPalette(vdp, type == SMS);

		if (type == SMS) {
			ImGui::Separator();
			im::TreeNode("CRAM", ImGuiTreeNodeFlags_DefaultOpen, [&]{ drawSMSCramEditor(vdp); });
		}
		ImGui::Separator();

		// Mode-4 tile-number masking (mirrors SMSVDPCore::spriteTileSelectMode4).
		auto spriteTileSelect = [&](unsigned tile) {
			return sms1 ? (tile & unsigned(((reg[6] & 0x03) << 6) | 0x3f)) : tile;
		};
		bool xShift = (reg[0] & 0x08) != 0;
		bool tileBank = (type == SMS) && (patBase != 0);
		int attExtra = attBase + (sms1 ? (0x80 & ((reg[5] & 0x01) << 7 | 0x7f)) : 0x80);
		// The first (top) pattern a sprite uses, as SMSVDPCore::selectSprites
		// derives it from the attribute byte. 16-line sprites add 1 for the
		// lower half.
		auto spriteTile = [&](int sprite) -> unsigned {
			if (type == TMS) {
				auto tile = unsigned(vram[(attBase + 4 * sprite + 2) & 0x3fff]);
				return (size == 16) ? (tile & 0xfc) : tile;
			}
			auto tile = spriteTileSelect(vram[(attExtra + 2 * sprite + 1) & 0x3fff]);
			if (tileBank) tile += 256;
			return (size == 16) ? (tile & 0x1fe) : tile;
		};

		auto formatClipboardData = [&](unsigned address, unsigned count) {
			return formatVramData([&](unsigned addr) { return vram[addr & 0x3fff]; }, address, count);
		};
		auto patternAddress = [&](unsigned pattern) {
			return (patBase + ((type == SMS) ? 32 : 8) * pattern) & 0x3fff;
		};
		auto patternDataSize = [&](int s) {
			if (type == SMS) return (s == 16) ? 64u : 32u;
			return (s == 16) ? 32u : 8u;
		};

		// Pattern texture: monochrome for TMS (colored via the sprite
		// attribute), full color (palette 1) for mode 4.
		// Always 256x64: 256 TMS patterns, 64 TMS 16x16 sprites, the 256
		// tiles of the mode 4 sprite bank or 128 mode 4 8x16 sprites.
		auto patternTexSize = gl::ivec2{256, 64};
		auto patternTexChars = (type == SMS)
			? ((size == 8) ? gl::vec2{32.0f, 8.0f} : gl::vec2{32.0f, 4.0f})
			: spritePatternCells(size);
		std::array<uint32_t, 256 * 256> pixels;
		if (type != NONE) {
			if (type == TMS) {
				auto pat = [&](unsigned i) { return vram[(unsigned(patBase) + i) & 0x3fff]; };
				if (size == 8) renderSpritePatterns8 (pat, pixels);
				else           renderSpritePatterns16(pat, pixels);
			} else {
				auto pen0 = transparent ? getColor(imColor::TRANSPARENT) : palette[16];
				if (size == 8) renderPatternsSms8 (vram, unsigned(patBase), palette, pen0, pixels);
				else           renderPatternsSms16(vram, unsigned(patBase), palette, pen0, pixels);
			}
			updateTexture(patternTex, patternTexSize, pixels);
		}
		// One zoom (Settings) for all sections, like the MSX VDP sprite viewer.
		int zm = 2 * (1 + zoom);
		auto gColor = ImGui::ColorConvertFloat4ToU32(gridColor);
		if (grid) {
			int gridW = ((type == SMS) ? 8 : size) * zm; // SMS sprites are 8 pixels wide
			updateGridTexture(gridTex, {gridW, size * zm}, gColor);
		}

		if (checkerBoardSize) {
			updateCheckerBoardTexture(checkerTex,
			                          ImGui::ColorConvertFloat4ToU32(checkerBoardColor1),
			                          ImGui::ColorConvertFloat4ToU32(checkerBoardColor2));
		}

		// UVs of one sprite (or tile) in the pattern texture.
		auto spriteUV = [&](unsigned tile) -> std::pair<gl::vec2, gl::vec2> {
			if (type == SMS) {
				tile &= 0xff; // the texture holds only the selected tile bank
				if (size == 16) {
					auto s = tile / 2; // even tile selects the 8x16 sprite
					gl::vec2 uv1{float(s % 32) / 32.0f, float(s / 32) / 4.0f};
					return {uv1, uv1 + gl::vec2{1.0f / 32.0f, 1.0f / 4.0f}};
				}
				// the 8x8 tiles have the same layout as the TMS 8x8 patterns
			}
			return spritePatternUV(tile, size);
		};

		// Render one sprite from the sprite attribute table at the given
		// zoom factor (used by the Sprite attributes view).
		auto spriteW = (type == SMS) ? 8 : size; // SMS sprites are 8 pixels wide
		auto spriteH = size;
		auto renderSpriteAttrib = [&](int sprite, float zoomFactor) {
			auto zoomVec = zoomFactor * gl::vec2{float(spriteW), float(spriteH)};
			auto [uv1, uv2] = spriteUV(spriteTile(sprite));
			if (type == TMS) {
				auto flags = vram[(attBase + 4 * sprite + 3) & 0x3fff];
				auto color = flags & 0x0f;
				auto tint = (transparent && (color == 0))
				          ? ImVec4{} : ImGui::ColorConvertU32ToFloat4(palette[color]);
				ImGui::ImageWithBg(patternTex.getImGui(), zoomVec, uv1, uv2, {}, tint);
			} else {
				ImGui::Image(patternTex.getImGui(), zoomVec, uv1, uv2);
			}
		};

		int maxSprites = (type == SMS) ? 8 : 4;
		int magFactor = mag ? 2 : 1;
		int magW = magFactor * ((type == SMS) ? 8 : size); // SMS sprites are 8 pixels wide
		int magH = magFactor * size;
		// First screen line of a sprite (see selectLine): Y>=240 is above the
		// screen, and magnification halves Y before the compare, so an odd Y
		// starts one line earlier.
		auto spriteTop = [&](int vramY) {
			int y = (vramY >= 240) ? (vramY - 256) : vramY;
			return (mag ? (y & ~1) : y) + 1;
		};
		int maxZoomH = sms1 ? 4 : 8;
		int maxZoomV = 8;
		bool canStopY = (type != SMS) || (lines == 192);
		int stopY = 0xd0;
		int total = (type == SMS) ? 64 : 32;
		auto yAddress = [&](int sprite) {
			return (attBase + ((type == SMS) ? sprite : 4 * sprite)) & 0x3fff;
		};
		// Screen x of a sprite's left edge: the TMS early clock bit and the
		// mode 4 R#0 shift move it left.
		auto spriteLeft = [&](int sprite) {
			if (type == TMS) {
				int x = vram[(attBase + 4 * sprite + 1) & 0x3fff];
				bool earlyClock = (vram[(attBase + 4 * sprite + 3) & 0x3fff] & 0x80) != 0;
				return earlyClock ? (x - 32) : x;
			}
			int x = vram[(attExtra + 2 * sprite) & 0x3fff];
			return xShift ? (x - 8) : x;
		};
		// Index of the Y=0xD0 terminator; entries from there on are not
		// evaluated by the hardware. Follow the visible line count, including
		// a manual override, not the chip's.
		int stopYIndex = total;
		if (canStopY) {
			for (auto i : xrange(total)) {
				if (vram[yAddress(i)] == stopY) {
					stopYIndex = i;
					break;
				}
			}
		}

		// One entry of SMSVDPCore::selectSprites() for a scanline.
		struct Selected {
			int sprite;
			int x;
			unsigned tile;
			int spriteLine;
			uint8_t flags;
		};
		// The number of sprites selected out of 'onLine' sprites on a line.
		auto selectedCount = [&](int onLine, bool limitPerLine) {
			return limitPerLine ? std::min(onLine, maxSprites) : onLine;
		};
		// Returns the number of sprites on the line, also those beyond the
		// per-line limit: 'selected' has selectedCount() entries. Like on the
		// hardware, the magnification test below uses the number of selected
		// sprites, so with magnified sprites the result depends on 'limitPerLine'.
		auto selectLine = [&](int line, bool limitPerLine, bool stopAtTerminator,
		                      std::array<Selected, 64>& selected) -> int {
			int onLine = 0;
			for (int sprite = 0; sprite < total; ++sprite) {
				int count = selectedCount(onLine, limitPerLine);
				int parseLine = line - 1;
				int spriteY = vram[yAddress(sprite)];
				if (stopAtTerminator && (spriteY == stopY)) break;
				if (spriteY >= 240) spriteY -= 256;
				if (mag && (count < maxZoomV)) {
					parseLine >>= 1;
					spriteY >>= 1;
				}
				if ((parseLine >= spriteY) && (parseLine < (spriteY + size))) {
					if (!limitPerLine || (onLine < maxSprites)) {
						Selected& s = selected[onLine];
						int spriteLine = parseLine - spriteY;
						s.sprite = sprite;
						s.tile = spriteTile(sprite);
						if (spriteLine > 0x07) s.tile += 1;
						if (type == TMS) {
							s.x = vram[(attBase + 4 * sprite + 1) & 0x3fff];
							s.flags = vram[(attBase + 4 * sprite + 3) & 0x3fff];
						} else {
							s.x = vram[(attExtra + 2 * sprite) & 0x3fff];
							s.flags = 0;
							if (xShift) s.x -= 8;
						}
						s.spriteLine = spriteLine & 0x07;
					}
					++onLine;
				}
			}
			return onLine;
		};

		im::TreeNode("Sprite patterns", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			if (type == NONE) {
				ImGui::TextUnformatted("No sprites in this screen mode"sv);
				return;
			}
			auto fullSize = gl::vec2(patternTexSize) * float(zm);
			im::Child("##pattern", {0, childHeightWithScrollbar(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto pos1 = ImGui::GetCursorPos();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				if (type == SMS) {
					drawCheckerBoard(checkerTex, fullSize, checkerBoardSize);
					ImGui::SetCursorPos(pos1);
				}
				ImGui::Image(patternTex.getImGui(), fullSize);
				gl::vec2 zoomPatSize{float(spriteW * zm), float(spriteH * zm)};
				bool hovered = ImGui::IsItemHovered();
				if (hovered) {
					gridPosition = hoveredCell(scrnPos, zoomPatSize);
				}
				auto pattern = [&] -> unsigned {
					if (type == SMS) {
						return (size == 16) ? ((32 * gridPosition.y) + gridPosition.x) * 2
						                    : ((32 * gridPosition.y) + gridPosition.x);
					}
					return spritePatternAt(gridPosition, size);
				}();
				copyToClipboardPopup("CopySpriteDataPopup", "Copy pattern data to clipboard", [&] {
					return formatClipboardData(patternAddress(pattern), patternDataSize(size));
				});
				if (hovered) {
					ImGui::BeginTooltip();
					ImGui::StrCat("pattern: ", pattern);
					if (tileBank) {
						auto used = pattern + 256;
						ImGui::StrCat("uses pattern: ", used, " (0x", hex_string<3>(used), ')');
					}
					ImGui::StrCat("address: 0x", hex_string<4>(patternAddress(pattern)));
					auto recipPatTex = recip(patternTexChars);
					auto uv1 = gl::vec2(gridPosition) * recipPatTex;
					auto uv2 = uv1 + recipPatTex;
					auto pos2 = ImGui::GetCursorPos();
					int z = (size == 16) ? 3 : 6;
					if (type == SMS) {
						drawCheckerBoard(checkerTex, float(z) * zoomPatSize, z * checkerBoardSize);
						ImGui::SetCursorPos(pos2);
					}
					if (grid) {
						updateGridTexture(zoomGridTex, gl::ivec2{z * zm}, gColor);
					}
					imageWithGrid(patternTex, float(z) * zoomPatSize, uv1, uv2,
					              grid ? &zoomGridTex : nullptr, gl::vec2{float(spriteW), float(spriteH)});
					ImGui::EndTooltip();
				}
				if (grid) {
					ImGui::SetCursorPos(pos1);
					ImGui::Image(gridTex.getImGui(), fullSize, {}, patternTexChars);
				}
			});
		});
		// Whether (and if not, why) the hardware would display this sprite
		// (mirrors SMSVDPCore::selectSprites()).
		auto visibleText = [&](int sprite) -> std::string_view {
			if (!(reg[1] & 0x40)) return "no (display disabled)"sv;
			if (sprite >= stopYIndex) return "no (after the Y=0xD0 terminator)"sv;
			// A sprite can be selected for a line and still be completely
			// left of the screen (e.g. X=0 with the R#0 shift).
			int yTop = spriteTop(vram[yAddress(sprite)]);
			if (((yTop + magH) <= 0) || (yTop >= lines) || ((spriteLeft(sprite) + magW) <= 0)) {
				return "no (off-screen)"sv;
			}
			std::array<Selected, 64> selected;
			for (int line = 0; line < lines; ++line) {
				int count = selectedCount(selectLine(line, true, canStopY, selected), true);
				for (int i = 0; i < count; ++i) {
					if (selected[i].sprite != sprite) continue;
					// Still counts for the per-line limit, but draws nothing.
					if ((type == TMS) && transparent && ((selected[i].flags & 0x0f) == 0)) {
						return "no (color 0 is transparent)"sv;
					}
					return "yes"sv;
				}
			}
			return "no (beyond the per-line sprite limit)"sv;
		};
		// All entries of the sprite attribute table (32 TMS, 64 mode 4),
		// regardless of whether the hardware would display them.
		auto drawSpriteAttributes = [&]{
			auto rows = total / 8;
			auto zoomSize = gl::vec2{float(spriteW), float(spriteH)} * float(zm);
			auto fullSize = zoomSize * gl::vec2(8, float(rows));
			im::Child("##attrib", {0, childHeightWithScrollbar(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				if (type == NONE) {
					ImGui::TextUnformatted("No sprites in this screen mode"sv);
				} else {
					gl::vec2 topLeft = ImGui::GetCursorPos();
					gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
					drawCheckerBoard(checkerTex, fullSize, checkerBoardSize);
					for (auto row : xrange(rows)) {
						for (auto column : xrange(8)) {
							int sprite = 8 * row + column;
							ImGui::SetCursorPos(topLeft + zoomSize * gl::vec2(float(column), float(row)));
							renderSpriteAttrib(sprite, float(zm));
						}
					}
					ImGui::SetCursorPos(topLeft);
					if (grid) {
						ImGui::Image(gridTex.getImGui(), fullSize, {}, gl::vec2{8, float(rows)});
					} else {
						ImGui::Dummy(fullSize);
					}
					bool hovered = ImGui::IsItemHovered();
					if (hovered) {
						gridPosition = hoveredCell(scrnPos, zoomSize);
					}
					auto sprite = 8 * gridPosition.y + gridPosition.x;
					copyToClipboardPopup("CopySpriteDataPopup", "Copy sprite data to clipboard", [&]{
						if (type == TMS) {
							return formatClipboardData(yAddress(sprite), 4);
						}
						return formatClipboardData(yAddress(sprite), 1) + '\n'
						     + formatClipboardData(attExtra + 2 * sprite, 2);
					});
					if (hovered) {
						ImGui::BeginTooltip();
						ImGui::StrCat("sprite: ", sprite);
						ImGui::StrCat("address: 0x", hex_string<4>(yAddress(sprite)));
						auto pos = ImGui::GetCursorPos();
						drawCheckerBoard(checkerTex, 3.0f * zoomSize, 3 * checkerBoardSize);
						ImGui::SetCursorPos(pos);
						renderSpriteAttrib(sprite, float(3 * zm));
						auto y = vram[yAddress(sprite)];
						auto used = spriteTile(sprite);
						if (type == TMS) {
							auto x = vram[(attBase + 4 * sprite + 1) & 0x3fff];
							auto tile = vram[(attBase + 4 * sprite + 2) & 0x3fff];
							auto flags = vram[(attBase + 4 * sprite + 3) & 0x3fff];
							ImGui::StrCat("x: ", x, "  y: ", y);
							ImGui::StrCat("pattern: ", tile);
							if (used != tile) ImGui::StrCat("uses pattern: ", used);
							ImGui::StrCat("color: ", flags & 15, (flags & 0x80 ? " (EC)"sv : ""sv));
						} else {
							auto x = vram[(attExtra + 2 * sprite + 0) & 0x3fff];
							auto tile = vram[(attExtra + 2 * sprite + 1) & 0x3fff];
							ImGui::StrCat("x: ", x, "  y: ", y);
							ImGui::StrCat("tile: ", tile, " (0x", hex_string<2>(tile), ')');
							if (used != tile) {
								ImGui::StrCat("uses pattern: ", used, " (0x", hex_string<3>(used), ')');
							}
							ImGui::StrCat("entry: 0x", hex_string<4>((attExtra + 2 * sprite) & 0x3fff));
						}
						ImGui::StrCat("visible: ", visibleText(sprite));
						ImGui::EndTooltip();
					}
				}
			});
		};
		ImGui::Separator();
		im::TreeNode("Sprite attributes", ImGuiTreeNodeFlags_DefaultOpen, drawSpriteAttributes);
		ImGui::Separator();

		im::TreeNode("Rendered sprites", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			if (type == NONE) {
				ImGui::TextUnformatted("No sprites in this screen mode"sv);
				return;
			}
			std::array<uint32_t, 256 * 256> screen;
			memset(screen.data(), 0, sizeof(uint32_t) * 256 * lines);

			// Per-line simulation, mirroring SMSVDPCore::selectSprites and
			// drawSpritesTms9918Mode()/drawSpritesMode4().
			std::array<uint8_t, 256> perLine = {};
			for (int line = 0; line < lines; ++line) {
				std::array<Selected, 64> selected;
				int onLine = selectLine(line, enableLimitPerLine, !renderAfterStopY && canStopY, selected);
				int count = selectedCount(onLine, enableLimitPerLine);
				perLine[line] = narrow<uint8_t>(onLine);
				// draw in reverse order: the first selected sprite is on top
				auto lineBuf = subspan<256>(screen, 256 * line);
				for (int i = count - 1; i >= 0; --i) {
					const auto& s = selected[i];
					int zoomScale = (i < maxZoomH) ? magFactor : 1;
					if (type == TMS) {
						auto color = s.flags & 0x0f;
						if (transparent && (color == 0)) continue;
						int x = s.x;
						if (s.flags & 0x80) x -= 32;
						for (int part = 8; part <= size; part += 8) {
							unsigned tile = s.tile;
							if (part == 16) tile += 2;
							auto pattern = vram[(patBase + 8 * tile + s.spriteLine) & 0x3fff];
							for (auto pixelX : xrange(8)) {
								if ((pattern >> (7 - pixelX)) & 1) {
									int px = x + ((zoomScale > 1) ? (pixelX << 1) : pixelX);
									for (int z = 0; z < zoomScale; ++z) {
										if ((0 <= (px + z)) && ((px + z) < 256)) {
											lineBuf[px + z] = palette[color];
										}
									}
								}
							}
							x += (zoomScale > 1) ? 16 : 8;
						}
					} else {
						auto pens = mode4Pens(vram, s.tile * 32 + s.spriteLine * 4);
						for (auto pixelX : xrange(8)) {
							auto pen = pens[pixelX];
							if (transparent && (pen == 0)) continue;
							auto color = palette[16 + pen];
							int px = s.x + ((zoomScale > 1) ? (pixelX << 1) : pixelX);
							for (int z = 0; z < zoomScale; ++z) {
								if ((0 <= (px + z)) && ((px + z) < 256)) {
									lineBuf[px + z] = color;
								}
							}
						}
					}
				}
			}

			// bounding boxes (raw sprite positions, up to the terminator)
			static_vector<SpriteBox, 64> clippedBoxes;
			for (int sprite = 0; sprite < total; ++sprite) {
				if (!renderAfterStopY && (sprite >= stopYIndex)) break;
				auto vramY = vram[yAddress(sprite)];
				auto vramX = vram[(type == TMS) ? ((attBase + 4 * sprite + 1) & 0x3fff)
				                                : ((attExtra + 2 * sprite) & 0x3fff)];
				unsigned pattern = spriteTile(sprite);
				int x = spriteLeft(sprite);
				int y = spriteTop(vramY);
				int w = magW;
				int h = magH;
				if (!clipSpriteBoxSide(x, w, 256)) continue;
				// No wrap to the top: Y>=240 is already the negative range.
				if (!clipSpriteBoxSide(y, h, lines)) continue;
				clippedBoxes.push_back(SpriteBox{x, y, w, h, sprite, vramX, vramY, pattern});
			}

			updateTexture(renderTex, {256, lines}, screen, true); // with wrapping

			auto fullSize = float(zm) * gl::vec2(256, float(lines));
			im::Child("##screen", {0.0f, childHeightWithScrollbar(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto* drawList = ImGui::GetWindowDrawList();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();

				gl::vec2 topLeft = ImGui::GetCursorPos();
				drawCheckerBoard(checkerTex, fullSize, checkerBoardSize);
				ImGui::SetCursorPos(topLeft);
				ImGui::Image(renderTex.getImGui(), fullSize);
				bool hovered = ImGui::IsItemHovered();
				auto hoverPos = hoveredCell(scrnPos, gl::vec2(float(zm)));
				ImGui::SameLine();
				auto visibleLines = std::span(perLine).first(lines);
				spritesPerLineHistogram(showSpritesPerLine, visibleLines, maxSprites, float(zm));

				im::Group([&]{
					spriteBoxSettings(drawBoundingBox, boundingBoxColor, boundingBoxOnAll);
					{
						auto maxStr = tmpStrCat("Max ", maxSprites, " sprites per line");
						ImGui::Checkbox(maxStr.c_str(), &enableLimitPerLine);
					}
					spritesPerLineSettings(showSpritesPerLine, visibleLines, maxSprites);
					im::Disabled(!canStopY, [&]{
						ImGui::Checkbox("Show After Y=0xD0", &renderAfterStopY);
						simpleToolTip("Also render the sprites from the Y=0xD0 terminator onwards. "
						              "The hardware ignores those entries in 192-line mode.");
					});
				});

				if (hovered) {
					auto [hx, hy] = hoverPos;
					ImGui::BeginTooltip();
					ImGui::StrCat("x,y=", hx, ',', hy, " (0x", hex_string<2>(hx), ",0x", hex_string<2>(hy), ')');
					printSpriteBoxes(clippedBoxes, hoverPos, 3);
					ImGui::EndTooltip();
				}

				if (drawBoundingBox) {
					drawSpriteBoxes(drawList, scrnPos, gl::vec2{float(zm)}, clippedBoxes, boundingBoxColor,
					                boundingBoxOnAll == 1, hovered, hoverPos);
				}
			});
		});
	});
}

} // namespace openmsx
