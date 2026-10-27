#include "ImGuiSMSVDPTiles.hh"

#include "Franky.hh"
#include "SMSVDP.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "MSXMotherBoard.hh"

#include "one_of.hh"
#include "ranges.hh"
#include "xrange.hh"

#include <imgui.h>

#include <array>
#include <ranges>
#include <utility>

namespace openmsx {

using namespace std::literals;

ImGuiSMSVDPTiles::ImGuiSMSVDPTiles(ImGuiManager& manager_, size_t index)
	: ImGuiPart(manager_)
	, title(strCat("SMS VDP tile viewer", strCat_if(index, " (", index + 1, ')')))
{
}

void ImGuiSMSVDPTiles::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiSMSVDPTiles::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiSMSVDPTiles::initHexDigits()
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

// Render the pattern table to a texture, mirroring SMSVDPCore::drawScanlineMode*.
void ImGuiSMSVDPTiles::renderPatterns(const PatternParams& params,
                                      std::span<const uint32_t, 32> palette,
                                      std::span<const uint8_t, 0x4000> vram,
                                      const VramTables& tables,
                                      std::span<uint32_t> output)
{
	auto read = [&](unsigned address) { return vram[address & 0x3fff]; };
	auto readColor = [&](unsigned address) {
		return params.overrideColorTable ? params.overrideColorValue : read(address);
	};
	switch (params.mode) {
	case ImGuiSMSVDPTiles::TEXT: {
		auto fg = palette[params.fgCol];
		auto bg = palette[params.bgCol];
		for (auto row : xrange(8)) {
			for (auto column : xrange(32)) {
				auto patNum = 32 * row + column;
				auto offset = 8 * patNum;
				for (auto y : xrange(8)) {
					auto pattern = read(tables.patBase + offset + y);
					auto out = subspan<6>(output, (8 * row + y) * 192 + 6 * column);
					draw6(pattern, fg, bg, out);
				}
			}
		}
		break;
	}
	case ImGuiSMSVDPTiles::GRAPHIC1:
		for (auto row : xrange(8)) {
			for (auto group : xrange(4)) { // 32 columns, split in 4 groups of 8
				auto color = readColor(tables.colBase + 4 * row + group);
				auto fg = palette[color >> 4];
				auto bg = palette[color & 15];
				for (auto subColumn : xrange(8)) {
					auto column = 8 * group + subColumn;
					auto patNum = 32 * row + column;
					auto offset = 8 * patNum;
					for (auto y : xrange(8)) {
						auto pattern = read(tables.patBase + offset + y);
						auto out = subspan<8>(output, (8 * row + y) * 256 + 8 * column);
						draw8(pattern, fg, bg, out);
					}
				}
			}
		}
		break;
	case ImGuiSMSVDPTiles::GRAPHIC2:
		// Rows 24-31 of the name table address a fourth bank (row / 8 == 3).
		// The chip never scans those lines; the 32-row override is what shows them.
		for (auto row : xrange(params.graphic2Banks * 8)) {
			for (auto column : xrange(32)) {
				auto patNum = 32 * row + column;
				for (auto y : xrange(8)) {
					auto pattern = read(tables.patBase + (((patNum & tables.patMask) << 3) + y));
					auto color   = readColor(tables.colBase + (((patNum & tables.colMask) << 3) + y));
					auto fg = palette[color >> 4];
					auto bg = palette[color & 15];
					auto out = subspan<8>(output, (8 * row + y) * 256 + 8 * column);
					draw8(pattern, fg, bg, out);
				}
			}
		}
		break;
	case ImGuiSMSVDPTiles::MULTICOLOR:
		for (auto group : xrange(4)) {
			for (auto row : xrange(8)) {
				for (auto column : xrange(32)) {
					auto patNum = 32 * row + column;
					auto offset = 8 * patNum + 2 * group;
					for (auto y : xrange(2)) {
						auto out = subspan<2>(output, (16 * group + 2 * row + y) * 64 + 2 * column);
						auto pattern = read(tables.patBase + offset + y);
						out[0] = palette[pattern >> 4];
						out[1] = palette[pattern & 15];
					}
				}
			}
		}
		break;
	case ImGuiSMSVDPTiles::MODE4:
		// 512 patterns x 2 palettes, stacked: 32x32 characters (256x256).
		// The tile-number masks mirror SMSVDPCore::tile1SelectMode4 /
		// tile2SelectMode4 (they only differ from the identity on SMS1).
		for (auto pal : xrange(2)) {
			for (auto patNum : xrange(512)) {
				auto charRow = (patNum / 32) + 16 * pal;
				auto charCol = patNum % 32;
				auto tile1 = unsigned(patNum) & tables.tile1Mask;
				auto tile2 = unsigned(patNum) & tables.tile2Mask;
				for (auto y : xrange(8)) {
					auto addr0 = tile1 * 32 + y * 4;
					auto addr2 = tile2 * 32 + y * 4;
					auto bitPlane0 = read(addr0 + 0);
					auto bitPlane1 = read(addr0 + 1);
					auto bitPlane2 = read(addr2 + 2);
					auto bitPlane3 = read(addr2 + 3);
					for (auto x : xrange(8)) {
						auto pen = uint8_t((((bitPlane3 >> (7 - x)) & 1) << 3) |
						                   (((bitPlane2 >> (7 - x)) & 1) << 2) |
						                   (((bitPlane1 >> (7 - x)) & 1) << 1) |
						                    ((bitPlane0 >> (7 - x)) & 1));
						output[(charRow * 8 + y) * 256 + charCol * 8 + x] =
							palette[16 * pal + pen];
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

void ImGuiSMSVDPTiles::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;
	ImGui::SetNextWindowSize({692, 886}, ImGuiCond_FirstUseEver);
	im::Window(title.c_str(), &show, [&]{
		auto* franky = dynamic_cast<Franky*>(motherBoard->findDevice("Franky"));
		if (!franky) return;
		auto& vdp = franky->getVdp();
		const auto vram = vdp.getVram();
		std::array<uint8_t, 10> reg; // R#0-R#9 (R#8/R#9 are used for the beam mapping)
		for (auto i : xrange(10)) reg[i] = vdp.peekReg(i);

		int vdpMode = [&] {
			switch (vdp.peekMode()) {
			case 0: return GRAPHIC1;
			case 1: return TEXT;
			case 2: return GRAPHIC2;
			case 3: return MULTICOLOR;
			case 4: return MODE4;
			default: return OTHER;
			}
		}();
		auto modeToStr = [](int mode) {
			if (mode == GRAPHIC1)   return "graphics I";
			if (mode == TEXT)       return "text";
			if (mode == GRAPHIC2)   return "graphics II";
			if (mode == MULTICOLOR) return "multicolor";
			if (mode == MODE4)      return "mode 4";
			if (mode == OTHER)      return "non-character";
			assert(false); return "ERROR";
		};
		auto patMult = [](int mode) { return 1 << (mode == GRAPHIC2 ? 13 : 11); };
		auto colMult = [](int mode) { return 1 << (mode == GRAPHIC2 ? 13 : 6); };
		int vdpFgCol = reg[7] >> 4;
		int vdpBgCol = reg[7] & 0x0f;
		// In the TMS9918-compatible modes pen 0 is drawn in the backdrop
		// color (R#7 low nibble).
		int vdpColor0 = vdpBgCol;
		// Table bases as the chip derives them from the registers in mode 'm'.
		// The settings readout uses the chip's mode, the tables use the
		// mode that is shown (these differ when the mode is overridden).
		auto patBaseFor = [&](int m) {
			return (m == MODE4)    ? 0
			     : (m == GRAPHIC2) ? ((reg[4] & 0x04) << 11)
			                       : ((reg[4] << 11) & 0x3fff);
		};
		auto colBaseFor = [&](int m) {
			return (m == MODE4)    ? 0
			     : (m == GRAPHIC2) ? ((reg[3] & 0x80) << 6)
			                       : ((reg[3] << 6) & 0x3fff);
		};
		auto namBaseFor = [&](int m) {
			return (m == MODE4)
				? ((vdp.peekYPixels() != 192) ? (((reg[2] & 0x0c) << 10) | 0x0700)
				                              : ((reg[2] << 10) & 0x3800))
				: ((reg[2] & 0x0f) << 10);
		};
		int vdpPatBase = patBaseFor(vdpMode);
		int vdpColBase = colBaseFor(vdpMode);
		int vdpNamBase = namBaseFor(vdpMode);
		int vdpYpixels = vdp.peekYPixels();
		int vdpLines = vdpYpixels; // visible lines (192/224/240)
		int vramSize = 0x4000;
		bool sms1 = vdp.getVariant() == SMSVDPCore::Variant::SMS1;
		// Mode-4 tile-number masks (identity on SMS2).
		unsigned tile1Mask = sms1 ? unsigned((reg[3] << 1) | 1) : 0x1ff;
		unsigned tile2Mask = sms1 ? unsigned(((reg[4] & 0x07) << 6) | 0x3f) : 0x1ff;
		unsigned nameRowMask = sms1 ? unsigned(((reg[2] & 0x01) << 10) | 0x3bff) : 0xffff;

		bool manMode   = overrideAll || overrideMode;
		bool manFgCol  = overrideAll || overrideFgCol;
		bool manBgCol  = overrideAll || overrideBgCol;
		bool manPat    = overrideAll || overridePat;
		bool manCol    = overrideAll || overrideCol;
		bool manNam    = overrideAll || overrideNam;
		bool manRows   = overrideAll || overrideRows;
		bool manColor0 = overrideAll || overrideColor0;

		int mode = manMode ? manualMode : vdpMode;

		auto [beamX, beamY] = vdp.getRasterPosition();
		if (mode == MODE4) {
			// Map the beam to the (scrolled) position in the name table, like
			// SMSVDPCore::drawScanlineMode4: screen x shows name-table x minus
			// R#8, screen y shows name-table y plus R#9. R#0 bit 6 locks the
			// top 16 lines horizontally, bit 7 the rightmost 8 columns vertically.
			const int scrollMod = (vdpYpixels == 192) ? 224 : 256;
			const int xScroll = (((reg[0] & 0x40) != 0) && (beamY < 16)) ? 0 : reg[8];
			const int screenColumn = (beamX - (xScroll & 7)) >> 3;
			const int yScroll = (((reg[0] & 0x80) != 0) && (screenColumn > 23)) ? 0 : reg[9];
			beamX = (beamX - xScroll) % 256;
			if (beamX < 0) beamX += 256;
			beamY = (beamY + yScroll) % scrollMod;
			if (beamY < 0) beamY += scrollMod;
		}
		gl::vec2 rasterBeamPos{float(beamX), float(beamY)};
		if (mode == TEXT) rasterBeamPos.x -= 8.0f; // 40 columns of 6 pixels

		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			static const char* const color0Str = "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000none\000";
			im::Group([&]{
				ImGui::TextUnformatted("SMS VDP settings");
				auto pos = ImGui::GetCursorPos();
				ImGui::Dummy(ImGui::CalcTextSize("Screen mode: graphics II"));
				ImGui::SetCursorPos(pos);
				im::Disabled(manMode, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Screen mode: ", modeToStr(vdpMode));
				});
				im::Disabled(manFgCol, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Foreground color: ", vdpFgCol);
				});
				im::Disabled(manBgCol, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Background color: ", vdpBgCol);
				});
				im::Disabled(manPat || (vdpMode == MODE4), [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Pattern table: 0x", hex_string<4>(vdpPatBase));
				});
				im::Disabled(manCol || (vdpMode == MODE4), [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Color table: 0x", hex_string<4>(vdpColBase));
				});
				im::Disabled(manNam, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Name table: 0x", hex_string<4>(vdpNamBase));
				});
				im::Disabled(manRows, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Visible rows: ", vdpLines / 8);
				});
				im::Disabled(manColor0 || (vdpMode == MODE4), [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Replace color 0: ", getComboString(vdpColor0, color0Str));
				});
			});
			ImGui::SameLine();
			im::Group([&]{
				ImGui::Checkbox("Manual override", &overrideAll);
				im::Group([&]{
					im::Disabled(overrideAll, [&]{
						ImGui::Checkbox("##o-mode",    overrideAll ? &overrideAll : &overrideMode);
						// Foreground and background are only used in text mode.
						im::Disabled(mode != TEXT, [&]{
							ImGui::Checkbox("##o-fgCol", overrideAll ? &overrideAll : &overrideFgCol);
							ImGui::Checkbox("##o-bgCol", overrideAll ? &overrideAll : &overrideBgCol);
						});
						im::Disabled(mode == MODE4, [&]{
							ImGui::Checkbox("##o-pat",     overrideAll ? &overrideAll : &overridePat);
						});
						// Only graphics I and II have a color table.
						im::Disabled(mode != one_of(GRAPHIC1, GRAPHIC2), [&]{
							ImGui::Checkbox("##o-col",     overrideAll ? &overrideAll : &overrideCol);
						});
						ImGui::Checkbox("##o-nam",     overrideAll ? &overrideAll : &overrideNam);
						im::Disabled(mode == MODE4, [&]{
							ImGui::Checkbox("##o-rows",    overrideAll ? &overrideAll : &overrideRows);
							ImGui::Checkbox("##o-color0",  overrideAll ? &overrideAll : &overrideColor0);
						});
					});
				});
				ImGui::SameLine();
				im::Group([&]{
					im::ItemWidth(ImGui::GetFontSize() * 9.0f, [&]{
						im::Disabled(!manMode, [&]{
							im::Combo("##mode", modeToStr(manualMode), [&]{
								if (ImGui::Selectable("graphics I"))  manualMode = GRAPHIC1;
								if (ImGui::Selectable("text"))        manualMode = TEXT;
								if (ImGui::Selectable("graphics II")) manualMode = GRAPHIC2;
								if (ImGui::Selectable("multicolor"))  manualMode = MULTICOLOR;
								if (ImGui::Selectable("mode 4"))      manualMode = MODE4;
							});
						});
						static const char* const range0_15 = "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000";
						// Gate on the mode actually shown. manualMode stays at its
						// default when the mode itself is not overridden, so a chip
						// in text mode would otherwise keep these editors disabled.
						im::Disabled(mode != TEXT, [&]{
							im::Disabled(!manFgCol, [&]{
								ImGui::Combo("##fgCol", &manualFgCol, range0_15);
							});
							im::Disabled(!manBgCol, [&]{
								ImGui::Combo("##bgCol", &manualBgCol, range0_15);
							});
						});
						im::Disabled(!manPat || (mode == MODE4), [&]{
							comboHexSequence<4>("##pattern", &manualPatBase, patMult(mode), vramSize, 0);
						});
						im::Disabled(mode != one_of(GRAPHIC1, GRAPHIC2), [&]{
							im::Disabled(!manCol, [&]{
								comboHexSequence<4>("##color", &manualColBase, colMult(mode), vramSize, 0);
							});
						});
						im::Disabled(!manNam, [&]{
							comboHexSequence<4>("##name", &manualNamBase, 1 << 10, vramSize, 0);
						});
						im::Disabled(!manRows || (mode == MODE4), [&]{
							ImGui::Combo("##rows", &manualRows, "24\00032\000");
						});
						im::Disabled(!manColor0 || (mode == MODE4), [&]{
							ImGui::Combo("##Color 0 replacement", &manualColor0, color0Str);
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

				im::Disabled(mode != one_of(GRAPHIC1, GRAPHIC2), [&]{
					ImGui::Checkbox("Override color table", &overrideColorTable);
				});
				HelpMarker("Ignore the content of the color table and instead use this value for everything.\n"
				           "Could be useful to visualize the patterns when the color table has the same value for fg and bg.");
				const char* col0_15 = "0\0001\0002\0003\0004\0005\0006\0007\000"
				                      "8\0009\00010\00011\00012\00013\00014\00015\000";
				im::DisabledIndent(!overrideColorTable || (mode != one_of(GRAPHIC1, GRAPHIC2)), [&]{
					int fg = (overrideColorValue >> 4) & 0x0f;
					int bg = (overrideColorValue >> 0) & 0x0f;
					ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
					if (ImGui::Combo("fg", &fg, col0_15)) {
						overrideColorValue = (fg << 4) | bg;
					}
					ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
					ImGui::SameLine();
					if (ImGui::Combo("bg", &bg, col0_15)) {
						overrideColorValue = (fg << 4) | bg;
					}
				});

				im::Disabled(mode != MODE4, [&]{
					ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
					ImGui::Combo("Pattern palette", &patternPalette, "0\0001\000");
				});

				ImGui::Separator();
				ImGui::Checkbox("beam", &rasterBeam);
				ImGui::SameLine();
				im::Disabled(!rasterBeam, [&]{
					ImGui::ColorEdit4("raster beam color", rasterBeamColor.data(),
						ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
					ImGui::SameLine();
					im::Font(manager.fontMono, [&]{
						ImGui::StrCat('(', dec_string<4>(beamX),
						              ',', dec_string<4>(beamY), ')');
					});
				});
				HelpMarker("Position of the raster beam, expressed in Sega VDP pixels.\n"
				           "(0,0) is the top-left of the active display area.\n"
				           "In mode 4 the position is mapped through the horizontal\n"
				           "and vertical scroll registers, so it points at the tile\n"
				           "currently being displayed.\n"
				           "Only practically useful when emulation is paused.");
			});
		});
		int color0 = manColor0 ? manualColor0 : vdpColor0;

		VramTables tables;
		tables.namBase = manNam ? unsigned(manualNamBase) : unsigned(namBaseFor(mode));
		if (mode == MODE4) {
			// Mode 4 has no pattern/color table base registers: the tile
			// number indexes the pattern table at 0x0000.
			tables.patBase = 0;
			tables.patMask = 0x1ff;
			tables.colBase = 0;
			tables.colMask = 0;
		} else if (manPat) {
			tables.patBase = unsigned(manualPatBase);
			tables.patMask = (mode == GRAPHIC2) ? 0x3ff : 0xff;
		} else {
			tables.patBase = unsigned(patBaseFor(mode));
			tables.patMask = (mode == GRAPHIC2) ? unsigned(((reg[4] & 0x03) << 8) | 0xff) : 0xff;
		}
		if (mode != MODE4) {
			if (manCol) {
				tables.colBase = unsigned(manualColBase);
				tables.colMask = (mode == GRAPHIC2) ? 0x3ff : 0;
			} else {
				tables.colBase = unsigned(colBaseFor(mode));
				tables.colMask = (mode == GRAPHIC2) ? unsigned(((reg[3] & 0x7f) << 3) | 0x07) : 0;
			}
		}
		tables.tile1Mask = tile1Mask;
		tables.tile2Mask = tile2Mask;

		// Palette: the TMS9918-compatible modes use the 16 fixed TMS
		// colors (entries 64-79 of the chip palette); mode 4 uses the 32
		// CRAM colors (2 palettes of 16).
		std::array<uint32_t, 32> palette{};
		const auto colors = vdp.getPaletteColors();
		if (mode == MODE4) {
			for (auto i : xrange(32)) {
				const uint32_t rgb = colors[vdp.peekCram(i) & 0x3f];
				palette[i] = ImColor(int((rgb >> 16) & 0xff),
				                     int((rgb >> 8) & 0xff),
				                     int(rgb & 0xff));
			}
		} else {
			for (auto i : xrange(16)) {
				const uint32_t rgb = colors[64 + i];
				palette[i] = ImColor(int((rgb >> 16) & 0xff),
				                     int((rgb >> 8) & 0xff),
				                     int(rgb & 0xff));
			}
			if (color0 < 16) palette[0] = palette[color0];
		}

		// Editable CRAM palette: click a swatch to pick one of the 64 colors
		// of the chip.
		auto drawCramEditor = [&]{
			for (auto i : xrange(32)) {
				auto cram = vdp.peekCram(i);
				ImGui::PushID(i);
				ImGui::ColorButton("##cram",
					ImGui::ColorConvertU32ToFloat4(palette[i]),
					ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(24.0f, 24.0f));
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("CRAM[%d] = 0x%02X (color 0x%02X)", i, cram, cram & 0x3f);
				}
				if (ImGui::IsItemClicked()) {
					ImGui::OpenPopup("##cramColors");
				}
				if (ImGui::BeginPopup("##cramColors")) {
					for (auto row : xrange(8)) {
						for (auto column : xrange(8)) {
							auto c = row * 8 + column;
							const uint32_t rgb = colors[c];
							ImGui::PushID(c);
							ImGui::ColorButton("##color",
								ImGui::ColorConvertU32ToFloat4(ImColor(int((rgb >> 16) & 0xff),
								                                      int((rgb >> 8) & 0xff),
								                                      int(rgb & 0xff))),
								ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(24.0f, 24.0f));
							ImGui::PopID();
							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip("color 0x%02X", c);
							}
							if (ImGui::IsItemClicked()) {
								vdp.pokeCram(i, uint8_t(c));
								ImGui::CloseCurrentPopup();
							}
							if (column != 7) ImGui::SameLine();
						}
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
				if ((i % 16) != 15) ImGui::SameLine();
			}
		};

		if (mode == MODE4) {
			ImGui::Separator();
			im::TreeNode("CRAM", ImGuiTreeNodeFlags_DefaultOpen, drawCramEditor);
		}

		auto fgCol = manFgCol ? manualFgCol : vdpFgCol;
		auto bgCol = manBgCol ? manualBgCol : vdpBgCol;

		int zx = (1 + zoom) * 2;
		int zy = (1 + zoom) * 2;
		gl::vec2 zm{float(zx), float(zy)};

		// Render the patterns to a texture, we need this both to display
		// the name-table and the pattern-table.
		// Graphics II name-table rows 24-31 use a fourth pattern bank.
		int graphic2Banks = (mode == GRAPHIC2 && manRows && (manualRows == 1)) ? 4 : 3;
		auto patternTexSize = [&] -> gl::ivec2 {
			if (mode == TEXT)       return {192,  64}; // 8 rows of 32 characters, each 6x8 pixels
			if (mode == GRAPHIC1)   return {256,  64}; // 8 rows of 32 characters, each 8x8 pixels
			if (mode == GRAPHIC2)   return {256, 64 * graphic2Banks};
			if (mode == MULTICOLOR) return { 64,  64}; // 8 rows of 32 characters, each 2x2 pixels, all this 4 times
			if (mode == MODE4)      return {256, 256}; // 512 patterns x 2 palettes (32x32 characters)
			return {1, 1}; // OTHER -> dummy
		}();
		auto patternTexChars = [&] -> gl::vec2 {
			if (mode == MULTICOLOR) return {32.0f, 32.0f};
			if (mode == GRAPHIC2)   return {32.0f, float(8 * graphic2Banks)};
			if (mode == MODE4)      return {32.0f, 32.0f};
			return {32.0f, 8.0f};
		}();
		auto recipPatTexChars = recip(patternTexChars);
		auto patternDisplaySize = [&] -> gl::ivec2 {
			if (mode == TEXT)       return {192,  64};
			if (mode == GRAPHIC2)   return {256, 64 * graphic2Banks};
			if (mode == MULTICOLOR) return {256, 256};
			if (mode == MODE4)      return {256, 128}; // 512 tiles of one palette
			return {256,  64}; // GRAPHIC1, OTHER
		}();
		// UVs of one pattern in the pattern texture; mode 4 stacks the two
		// palettes and supports the per-tile h/v flip.
		auto patternUV = [&](unsigned pattern, unsigned paletteBit, bool hflip, bool vflip)
			-> std::pair<gl::vec2, gl::vec2>
		{
			if (mode == MODE4) {
				auto patCol = float(pattern % 32);
				auto patRow = float(pattern / 32) + 16.0f * float(paletteBit);
				gl::vec2 uv1{patCol * (1.0f / 32.0f), patRow * (1.0f / 32.0f)};
				gl::vec2 uv2 = uv1 + gl::vec2{1.0f / 32.0f, 1.0f / 32.0f};
				if (hflip) std::swap(uv1.x, uv2.x);
				if (vflip) std::swap(uv1.y, uv2.y);
				return {uv1, uv2};
			}
			auto patRow = pattern / 32;
			auto patCol = pattern % 32;
			gl::vec2 uv1 = gl::vec2{float(patCol), float(patRow)} * recipPatTexChars;
			gl::vec2 uv2 = uv1 + recipPatTexChars;
			return {uv1, uv2};
		};
		// The pattern-table view shows one palette (mode 4) or the whole texture.
		auto patternViewUV = (mode == MODE4)
			? std::pair{gl::vec2{0.0f, float(patternPalette * 16) * (1.0f / 32.0f)},
			            gl::vec2{1.0f, float(patternPalette * 16 + 16) * (1.0f / 32.0f)}}
			: std::pair{gl::vec2{}, gl::vec2{1.0f, 1.0f}};
		auto gridTiling = (mode == MODE4) ? gl::vec2{32.0f, 16.0f} : patternTexChars;
		std::array<uint32_t, 256 * 256> pixels; // max size for GRAPHIC2/MODE4
		PatternParams params{mode, fgCol, bgCol, overrideColorTable,
		                     uint8_t(overrideColorValue), graphic2Banks};
		renderPatterns(params, palette, vram, tables, pixels);
		if (!patternTex.get()) {
			patternTex = gl::Texture(false, false); // no interpolation, no wrapping
		}
		patternTex.bind();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, patternTexSize.x, patternTexSize.y, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

		// create grid texture
		auto charWidth = (mode == TEXT) ? 6 : 8;
		auto charSize = gl::vec2{float(charWidth), 8.0f};
		auto charZoom = zm * charSize;
		auto zoomCharSize = gl::vec2(12.0f, 12.0f) * charSize; // hovered size
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

		auto printPatternNr = [](unsigned pat) {
			ImGui::StrCat("Pattern: ", pat, " (0x", hex_string<3>(pat), ')');
		};
		auto printAddressName = [](std::string_view name) {
			ImGui::StrCat(name, " address:");
		};
		auto printAddress = [&](std::string_view name, unsigned address) {
			printAddressName(name);
			ImGui::StrCat("  0x", hex_string<4>(address));
		};
		auto printAddressRange = [&](std::string_view name, unsigned address, unsigned count) {
			printAddressName(name);
			ImGui::StrCat("  0x", hex_string<4>(address), "-0x", hex_string<4>(address + count - 1));
		};
		auto patternAddress = [&](unsigned pattern) { // modes 0-3
			return (tables.patBase + ((pattern & tables.patMask) << 3)) & 0x3fff;
		};
		// Mode 4 takes bit planes 0-1 from tile1 and planes 2-3 from tile2;
		// they only differ on SMS1 (SMSVDPCore::tile1SelectMode4/tile2SelectMode4).
		auto mode4Tiles = [&](unsigned pattern) {
			return std::pair{pattern & tile1Mask, pattern & tile2Mask};
		};
		auto colorAddress = [&](unsigned pattern) {
			if (mode == GRAPHIC1) return (tables.colBase + (pattern >> 3)) & 0x3fff;
			return (tables.colBase + ((pattern & tables.colMask) << 3)) & 0x3fff;
		};
		auto printPatternColorAddress = [&](unsigned pattern) {
			if (mode == MODE4) {
				auto [tile1, tile2] = mode4Tiles(pattern);
				if (tile1 == tile2) {
					printAddressRange("Pattern", tile1 * 32, 32);
				} else {
					printAddressRange("Pattern (planes 0-1)", tile1 * 32, 32);
					printAddressRange("Pattern (planes 2-3)", tile2 * 32, 32);
				}
			} else {
				printAddressRange("Pattern", patternAddress(pattern), 8);
				if (mode == one_of(GRAPHIC1, GRAPHIC2)) {
					printAddressRange("Color", colorAddress(pattern), 8);
				}
			}
		};
		auto formatBinaryData = [&](unsigned address, unsigned count) {
			return formatToString([&](unsigned addr){ return vram[addr & 0x3fff]; }, address, address + count - 1, {}, 1, {}, ", ", "%02X", manager.getInterpreter());
		};
		auto formatPatternData = [&](unsigned pattern) {
			if (mode == MODE4) {
				auto [tile1, tile2] = mode4Tiles(pattern);
				return formatToString([&](unsigned i) { return vram[((i & 2) ? tile2 : tile1) * 32 + i]; },
				                      0, 31, {}, 1, {}, ", ", "%02X", manager.getInterpreter());
			}
			return formatBinaryData(patternAddress(pattern), 8);
		};
		auto getPatternFromGrid = [&] {
			return gridPosition.x + 32 * gridPosition.y;
		};
		auto copyPatternColorPopup = [&] {
			return im::PopupContextWindow("PatternCopyPopup", [&]{
				bool hasColorData = mode == one_of(GRAPHIC1, GRAPHIC2);
				const char* caption = hasColorData
					? "Copy pattern and color data to clipboard"
					: "Copy pattern data to clipboard";
				if (ImGui::MenuItem(caption)) {
					auto pattern = getPatternFromGrid();
					auto text = strCat("Pattern data\n", formatPatternData(pattern));
					if (hasColorData) {
						strAppend(text, "Color data\n", formatBinaryData(colorAddress(pattern), 8));
					}
					ImGui::SetClipboardText(text.c_str());
				}
			});
		};
		auto drawGrid = [&] {
			auto pat = getPatternFromGrid();
			printPatternNr(pat);
			auto [uv1, uv2] = patternUV(pat, unsigned(patternPalette), false, false);
			auto pos2 = ImGui::GetCursorPos();
			ImGui::Image(patternTex.getImGui(), zoomCharSize, uv1, uv2);
			if (grid) {
				ImGui::SetCursorPos(pos2);
				ImGui::Image(gridTex.getImGui(),
					zoomCharSize, {}, charSize);
			}
			printPatternColorAddress(pat);
		};

		// Height for the scrollable section children: the content height plus
		// room for the horizontal scrollbar, so that showing the scrollbar
		// does not trigger a vertical one.
		auto childHeight = [](float contentHeight) {
			return contentHeight + ImGui::GetStyle().ScrollbarSize;
		};

		ImGui::Separator();
		im::TreeNode("Pattern Table", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			auto size = gl::vec2(patternDisplaySize) * zm;
			im::Child("##pattern", {0, childHeight(size.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto pos1 = ImGui::GetCursorPos();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				ImGui::Image(patternTex.getImGui(), size, patternViewUV.first, patternViewUV.second);
				bool hovered = ImGui::IsItemHovered() && (mode != OTHER);
				if (hovered) {
					gridPosition = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / charZoom);
				}
				copyPatternColorPopup();
				if (hovered) {
					ImGui::BeginTooltip();
					drawGrid();
					ImGui::EndTooltip();
				}
				if (grid) {
					ImGui::SetCursorPos(pos1);
					ImGui::Image(gridTex.getImGui(), size,
						{}, gridTiling);
				}
			});
		});
		ImGui::Separator();
		im::TreeNode("Name Table", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			// Mode 4 shows the scroll map (28 rows, or 32 in 224/240-line
			// mode). Modes 0-3 show the 24 rows the chip displays; 32 shows
			// the rest of the 1 kB name-table slot.
			float rows = (mode == MODE4) ? ((vdpYpixels == 192) ? 28.0f : 32.0f)
			                             : ((manRows && (manualRows == 1)) ? 32.0f : 24.0f);
			auto columns = (mode == TEXT) ? 40 : 32;
			auto charsSize = gl::vec2(float(columns), rows); // (x * y) number of characters
			auto msxSize = charSize * charsSize; // (x * y) number of VDP pixels
			auto hostSize = msxSize * zm; // (x * y) number of host pixels

			im::Child("##name", {0.0f, childHeight(hostSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				auto* drawList = ImGui::GetWindowDrawList();

				struct TileEntry {
					unsigned tile = 0;
					bool hflip = false;
					bool vflip = false;
					unsigned palette = 0; // mode 4: 0/1
				};
				auto getEntry = [&](unsigned column, unsigned row) -> TileEntry {
					TileEntry entry;
					if (mode == MODE4) {
						// Like SMSVDPCore::drawScanlineMode4: 16-bit entries
						// (the name-row mirroring only differs from the
						// identity on SMS1).
						auto nameRow = (row << 6) & nameRowMask;
						auto address = (tables.namBase + nameRow + 2 * column) & 0x3fff;
						auto data = unsigned(vram[address] | (vram[(address + 1) & 0x3fff] << 8));
						entry.tile = data & 0x1ff;
						entry.hflip = (data & 0x200) != 0;
						entry.vflip = (data & 0x400) != 0;
						entry.palette = (data & 0x800) ? 1 : 0;
					} else {
						// Note: like SMSVDPCore::drawScanlineMode1 (and MAME),
						// the text mode name table uses a row-stride of 32.
						auto name = vram[(tables.namBase + 32 * row + column) & 0x3fff];
						if (mode == GRAPHIC2)        entry.tile = unsigned(name) + 256 * (row / 8);
						else if (mode == MULTICOLOR) entry.tile = unsigned(name) + 256 * (row % 4);
						else                         entry.tile = unsigned(name);
					}
					return entry;
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
							auto entry = getEntry(column, row);
							auto [uv1, uv2] = patternUV(entry.tile, entry.palette, entry.hflip, entry.vflip);
							drawList->PrimRectUV(p1, p2, uv1, uv2, 0xffffffff);
						}
					}
					drawList->PopTexture();
					if (nameTableOverlay) {
						// Mode 4 tile numbers are 9-bit, so they need three hex
						// digits (the mode 4 cells are at least 16 pixels wide,
						// so three 5-pixel digits fit); the TMS modes use two.
						const int numDigits = (mode == MODE4) ? 3 : 2;
						drawList->PushTexture(smallHexDigits.getImGui());
						drawList->PrimReserve(6 * numDigits * numChars, 4 * numDigits * numChars);
						static constexpr gl::vec2 digitSize{5.0f, 8.0f};
						static constexpr float texDigitWidth = 1.0f / 16.0f;
						static constexpr gl::vec2 texDigitSize{texDigitWidth, 1.0f};
						for (auto row : xrange(rowsCeil)) {
							for (auto column : xrange(columns)) {
								gl::vec2 pos = scrnPos + charZoom * gl::vec2{float(column), float(row)};
								auto pattern = getEntry(column, row).tile;
								for (auto i : xrange(numDigits)) {
									auto digit = (pattern >> (4 * (numDigits - 1 - i))) & 15;
									gl::vec2 uv{narrow_cast<float>(digit) * texDigitWidth, 0.0f};
									drawList->PrimRectUV(pos, pos + digitSize, uv, uv + texDigitSize, 0xffffffff);
									pos.x += digitSize.x;
								}
							}
						}
						drawList->PopTexture();
					}
					drawList->PopClipRect();
				}
				if (rasterBeam) {
					auto center = scrnPos + (rasterBeamPos + gl::vec2{0.5f}) * zm;
					auto color = ImGui::ColorConvertFloat4ToU32(rasterBeamColor);
					auto thickness = zm.y * 0.5f;
					auto zm1 = 1.5f * zm;
					auto zm3 = 3.5f * zm;
					drawList->AddRect(center - zm, center + zm, color, 0.0f, thickness);
					drawList->AddLine(center - gl::vec2{zm1.x, 0.0f}, center - gl::vec2{zm3.x, 0.0f}, color, thickness);
					drawList->AddLine(center + gl::vec2{zm1.x, 0.0f}, center + gl::vec2{zm3.x, 0.0f}, color, thickness);
					drawList->AddLine(center - gl::vec2{0.0f, zm1.y}, center - gl::vec2{0.0f, zm3.y}, color, thickness);
					drawList->AddLine(center + gl::vec2{0.0f, zm1.y}, center + gl::vec2{0.0f, zm3.y}, color, thickness);
				}

				auto pos1 = ImGui::GetCursorPos();
				ImGui::Dummy(hostSize);
				bool hovered = ImGui::IsItemHovered() && (mode != OTHER);
				if (hovered) {
					auto gridPos = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / charZoom);
					ImGui::BeginTooltip();
					ImGui::StrCat("Column: ", gridPos.x, " Row: ", gridPos.y);
					auto entry = getEntry(gridPos.x, gridPos.y);
					printPatternNr(entry.tile);
					auto [uv1, uv2] = patternUV(entry.tile, entry.palette, entry.hflip, entry.vflip);
					auto pos2 = ImGui::GetCursorPos();
					ImGui::Image(patternTex.getImGui(), zoomCharSize, uv1, uv2);
					if (grid) {
						ImGui::SetCursorPos(pos2);
						ImGui::Image(gridTex.getImGui(),
							zoomCharSize, {}, charSize);
					}
					if (mode == MODE4) {
						auto nameRow = (unsigned(gridPos.y) << 6) & nameRowMask;
						printAddressRange("Name", (tables.namBase + nameRow + 2 * unsigned(gridPos.x)) & 0x3fff, 2);
					} else {
						// the name table row stride is 32, also for the
						// 40-column text mode (like SMSVDPCore/MAME)
						auto nameIndex = 32 * gridPos.y + gridPos.x;
						printAddress("Name", (tables.namBase + nameIndex) & 0x3fff);
					}
					printPatternColorAddress(entry.tile);
					ImGui::EndTooltip();
				}
				if (grid) {
					ImGui::SetCursorPos(pos1);
					ImGui::Image(gridTex.getImGui(), hostSize, {}, charsSize);
				}
			});
		});
	});
}

} // namespace openmsx
