#include "ImGuiSMSVDPSprites.hh"

#include "Franky.hh"
#include "SMSVDP.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "MSXMotherBoard.hh"

#include "ranges.hh"
#include "xrange.hh"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>

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

// TMS9918 sprite patterns are monochrome; the color comes from the sprite
// attributes (drawn by tinting this texture).
static void renderPatternsTms8(std::span<const uint8_t, 0x4000> vram, unsigned patBase,
                               std::span<uint32_t> output)
{
	auto zero = getColor(imColor::TRANSPARENT);
	auto one  = getColor(imColor::TEXT);
	for (auto row : xrange(8)) {
		for (auto column : xrange(32)) {
			auto patNum = 32 * row + column;
			auto offset = 8 * patNum;
			for (auto y : xrange(8)) {
				auto pattern = vram[(patBase + offset + y) & 0x3fff];
				auto out = subspan<8>(output, (8 * row + y) * 256 + 8 * column);
				draw8(pattern, one, zero, out);
			}
		}
	}
}

static void renderPatternsTms16(std::span<const uint8_t, 0x4000> vram, unsigned patBase,
                                std::span<uint32_t> output)
{
	// 64 sprites of 16x16, each composed of tiles (4s, 4s+1, 4s+2, 4s+3).
	auto zero = getColor(imColor::TRANSPARENT);
	auto one  = getColor(imColor::TEXT);
	for (auto row : xrange(4)) {
		for (auto column : xrange(16)) {
			auto sprite = 16 * row + column;
			auto tile = 4 * sprite;
			for (auto y : xrange(16)) {
				auto patternA = vram[(patBase + 8 * (tile + 0) + y) & 0x3fff];
				auto patternB = vram[(patBase + 8 * (tile + 2) + y) & 0x3fff];
				auto outA = subspan<8>(output, (16 * row + y) * 256 + 16 * column + 0);
				auto outB = subspan<8>(output, (16 * row + y) * 256 + 16 * column + 8);
				draw8(patternA, one, zero, outA);
				draw8(patternB, one, zero, outB);
			}
		}
	}
}

// The 8 pens (0-15) of one mode 4 pattern row: four bytes, one per bit
// plane, starting at 'addr'.
static std::array<uint8_t, 8> mode4Pens(std::span<const uint8_t, 0x4000> vram, unsigned addr)
{
	auto b0 = vram[(addr + 0) & 0x3fff];
	auto b1 = vram[(addr + 1) & 0x3fff];
	auto b2 = vram[(addr + 2) & 0x3fff];
	auto b3 = vram[(addr + 3) & 0x3fff];
	std::array<uint8_t, 8> pens;
	for (auto x : xrange(8)) {
		pens[x] = uint8_t((((b3 >> (7 - x)) & 1) << 3) |
		                  (((b2 >> (7 - x)) & 1) << 2) |
		                  (((b1 >> (7 - x)) & 1) << 1) |
		                   ((b0 >> (7 - x)) & 1));
	}
	return pens;
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
		auto yesNo = [](int x) {
			if (x == 0) return "no";
			if (x == 1) return "yes";
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
		int vdpPatBase = patBaseFor(vdpType);
		int vdpAttBase = attBaseFor(vdpType);
		int vdpLines = (vdpType == SMS) ? vdpYpixels : 192;
		auto vramSize = 0x4000;

		// Palette: TMS9918 colors (modes 0-3) or the 32 CRAM colors (mode 4;
		// sprites always use palette 1 = entries 16-31). Filled once the
		// shown type is known.
		std::array<uint32_t, 32> palette{};
		const auto colors = vdp.getPaletteColors();
		auto toImColor = [](uint32_t rgb) {
			return uint32_t(ImColor(int((rgb >> 16) & 0xff),
			                        int((rgb >> 8) & 0xff),
			                        int(rgb & 0xff)));
		};

		// Height for the scrollable section children: the content height plus
		// room for the horizontal scrollbar, so that showing the scrollbar
		// does not trigger a vertical one.
		auto childHeight = [](float contentHeight) {
			return contentHeight + ImGui::GetStyle().ScrollbarSize;
		};

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
							ImGui::PushID(c);
							ImGui::ColorButton("##color",
								ImGui::ColorConvertU32ToFloat4(toImColor(colors[c])),
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

		bool manType   = overrideAll || overrideType;
		bool manSize   = overrideAll || overrideSize;
		bool manMag    = overrideAll || overrideMag;
		bool manTrans  = overrideAll || overrideTrans;
		bool manPat    = overrideAll || overridePat;
		bool manAtt    = overrideAll || overrideAtt;
		bool manLines  = overrideAll || overrideLines;

		// Checkerboard color: a color button that opens a picker, which also
		// has a button to restore the default color.
		auto checkerBoardColorEdit = [](const char* id, gl::vec4& color,
		                                const gl::vec4& defaultColor) {
			ImGui::PushID(id);
			if (ImGui::ColorButton("##color", ImVec4(color.x, color.y, color.z, color.w),
					ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip)) {
				ImGui::OpenPopup("##picker");
			}
			im::Popup("##picker", [&]{
				ImGui::ColorPicker4("##picker", color.data(), ImGuiColorEditFlags_NoAlpha);
				ImGui::Separator();
				if (ImGui::Button("Reset to default")) {
					color = defaultColor;
				}
			});
			ImGui::PopID();
		};

		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			im::Group([&]{
				ImGui::TextUnformatted("SMS VDP settings");
				im::Disabled(manType, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Sprite type: ", typeToStr(vdpType));
				});
				im::Disabled(manSize, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Sprite size: ", sizeToStr(vdpType, vdpSize));
				});
				im::Disabled(manMag, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Sprites magnified: ", yesNo(vdpMag));
				});
				im::Disabled(manTrans, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Color 0 transparent: ", yesNo(vdpTransparent));
				});
				im::Disabled(manPat, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Pattern table: 0x", hex_string<4>(vdpPatBase));
				});
				im::Disabled(manAtt, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Attribute table: 0x", hex_string<4>(vdpAttBase));
				});
				im::Disabled(manLines || (vdpType != SMS), [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Visible lines: ", vdpLines);
				});
			});
			ImGui::SameLine();
			im::Group([&]{
				ImGui::Checkbox("Manual override", &overrideAll);
				im::Group([&]{
					im::Disabled(overrideAll, [&]{
						ImGui::Checkbox("##o-type",   overrideAll ? &overrideAll : &overrideType);
						ImGui::Checkbox("##o-size",   overrideAll ? &overrideAll : &overrideSize);
						ImGui::Checkbox("##o-mag",    overrideAll ? &overrideAll : &overrideMag);
						ImGui::Checkbox("##o-trans",  overrideAll ? &overrideAll : &overrideTrans);
						ImGui::Checkbox("##o-pat",    overrideAll ? &overrideAll : &overridePat);
						ImGui::Checkbox("##o-att",    overrideAll ? &overrideAll : &overrideAtt);
						ImGui::Checkbox("##o-lines",  overrideAll ? &overrideAll : &overrideLines);
					});
				});
				ImGui::SameLine();
				im::Group([&]{
					// The type actually shown. manualType stays at its default
					// unless the type itself is overridden.
					auto shownType = [&] {
						return (overrideAll || overrideType) ? manualType : vdpType;
					};
					im::ItemWidth(ImGui::GetFontSize() * 9.0f, [&]{
						im::Disabled(!manType, [&]{
							im::Combo("##type", typeToStr(manualType), [&]{
								if (ImGui::Selectable("1"))   manualType = TMS;
								if (ImGui::Selectable("SMS")) manualType = SMS;
							});
						});
						im::Disabled(!manSize, [&]{
							im::Combo("##size", sizeToStr(shownType(), manualSize), [&]{
								if (ImGui::Selectable(" 8 x  8")) manualSize =  8;
								if (ImGui::Selectable((shownType() == SMS) ? " 8 x 16" : "16 x 16")) manualSize = 16;
							});
						});
						im::Disabled(!manMag, [&]{
							im::Combo("##mag", yesNo(manualMag), [&]{
								if (ImGui::Selectable("no"))  manualMag = 0;
								if (ImGui::Selectable("yes")) manualMag = 1;
							});
						});
						im::Disabled(!manTrans, [&]{
							im::Combo("##trans", yesNo(manualTransparent), [&]{
								if (ImGui::Selectable("no"))  manualTransparent = 0;
								if (ImGui::Selectable("yes")) manualTransparent = 1;
							});
						});
						im::Disabled(!manPat, [&]{
							if (shownType() == SMS) manualPatBase &= 0x2000;
							comboHexSequence<4>("##pat", &manualPatBase,
							                    (shownType() == SMS) ? 0x2000 : 0x800, vramSize, 0);
						});
						im::Disabled(!manAtt, [&]{
							comboHexSequence<4>("##att", &manualAttBase,
							                    (shownType() == SMS) ? 0x100 : 0x80, vramSize, 0);
						});
						im::Disabled(!manLines || (shownType() != SMS), [&]{
							ImGui::Combo("##lines", &manualLines, "192\000224\000240\000");
						});
					});
				});
			});
			ImGui::SameLine();
			ImGui::Dummy(ImVec2(25, 1));
			ImGui::SameLine();
			im::Group([&]{
				// One zoom for all sections, like the MSX VDP sprite viewer.
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.0f);
				ImGui::Combo("Zoom", &zoom, "1x\0002x\0003x\0004x\0005x\0006x\0007x\0008x\000");
				ImGui::Checkbox("grid", &grid);
				ImGui::SameLine();
				im::Disabled(!grid, [&]{
					ImGui::ColorEdit4("Grid color", gridColor.data(),
						ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
				});

				ImGui::TextUnformatted("Checkerboard:"sv);
				simpleToolTip("Used as background for transparent pixels and in the "
				              "'Rendered sprites' view. Pick both colors equal for a "
				              "solid background.");
				ImGui::SameLine();
				checkerBoardColorEdit("checkerboard color1", checkerBoardColor1,
				                      defaultCheckerBoardColor1);
				ImGui::SameLine();
				checkerBoardColorEdit("checkerboard color2", checkerBoardColor2,
				                      defaultCheckerBoardColor2);
				im::Indent([&]{
					ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
					if (ImGui::InputInt("size", &checkerBoardSize)) {
						checkerBoardSize = std::clamp(checkerBoardSize, 0, maxCheckerBoardSize);
					}
					simpleToolTip("0 = no checkerboard");
				});
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
		int patBase = !manPat       ? patBaseFor(type)
		            : (type == SMS) ? (manualPatBase & 0x2000) // the two tile banks
		                            : manualPatBase;
		int attBase = manAtt ? manualAttBase : attBaseFor(type);

		for (auto i : xrange(32)) {
			palette[i] = (type == SMS) ? toImColor(colors[vdp.peekCram(i) & 0x3f])
			           : (i < 16)      ? toImColor(colors[64 + i])
			                           : 0;
		}

		// Background where nothing is drawn (transparent pixels) is the
		// checkerboard texture.
		auto drawTransparentBackground = [&](gl::vec2 imageSize) {
			if (checkerBoardSize) {
				ImGui::Image(checkerTex.getImGui(), imageSize,
					{}, imageSize / (4.0f * float(checkerBoardSize)));
			}
		};

		if (type == SMS) {
			ImGui::Separator();
			im::TreeNode("CRAM", ImGuiTreeNodeFlags_DefaultOpen, drawCramEditor);
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

		auto copySpriteDataPopup = [&](zstring_view text, function_ref<std::string()> supplier) {
			return im::PopupContextWindow("CopySpriteDataPopup", [&]{
				if (ImGui::MenuItem(text.c_str())) {
					ImGui::SetClipboardText(supplier().c_str());
				}
			});
		};
		auto formatClipboardData = [&](unsigned address, unsigned count) {
			return formatToString([&](unsigned addr) { return vram[addr & 0x3fff]; },
			                      address, address + count - 1, {}, 1, {}, ", ", "%02X", manager.getInterpreter());
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
			: ((size == 8) ? gl::vec2{32.0f, 8.0f} : gl::vec2{16.0f, 4.0f});
		std::array<uint32_t, 256 * 256> pixels;
		if (!patternTex.get()) {
			patternTex = gl::Texture(false, false); // no interpolation, no wrapping
		}
		if (type != NONE) {
			patternTex.bind();
			if (type == TMS) {
				if (size == 8) renderPatternsTms8 (vram, unsigned(patBase), pixels);
				else           renderPatternsTms16(vram, unsigned(patBase), pixels);
			} else {
				auto pen0 = transparent ? getColor(imColor::TRANSPARENT) : palette[16];
				if (size == 8) renderPatternsSms8 (vram, unsigned(patBase), palette, pen0, pixels);
				else           renderPatternsSms16(vram, unsigned(patBase), palette, pen0, pixels);
			}
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, patternTexSize.x, patternTexSize.y, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		}
		// One zoom (Settings) for all sections, like the MSX VDP sprite viewer.
		int zm = 2 * (1 + zoom);
		auto gColor = ImGui::ColorConvertFloat4ToU32(gridColor);
		if (grid) {
			size_t gridW = ((type == SMS) ? 8 : size) * zm; // SMS sprites are 8 pixels wide
			size_t gridH = size * zm;
			for (auto y : xrange(gridH)) {
				auto* line = &pixels[y * gridW];
				for (auto x : xrange(gridW)) {
					line[x] = (x == 0 || y == 0) ? gColor : 0;
				}
			}
			if (!gridTex.get()) {
				gridTex = gl::Texture(false, true); // no interpolation, with wrapping
			}
			gridTex.bind();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
			             narrow<int>(gridW), narrow<int>(gridH),
			             0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		}

		if (checkerBoardSize) {
			pixels[0] = pixels[3] = ImGui::ColorConvertFloat4ToU32(checkerBoardColor1);
			pixels[1] = pixels[2] = ImGui::ColorConvertFloat4ToU32(checkerBoardColor2);
			if (!checkerTex.get()) {
				checkerTex = gl::Texture(false, true); // no interpolation, with wrapping
			}
			checkerTex.bind();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		}

		// UVs of one sprite (or tile) in the pattern texture.
		auto spriteUV = [&](unsigned tile) -> std::array<float, 4> {
			if (type == SMS) {
				tile &= 0xff; // the texture holds only the selected tile bank
				if (size == 16) {
					auto s = tile / 2; // even tile selects the 8x16 sprite
					auto cc = s % 32;
					auto rr = s / 32;
					return {float(cc) / 32.0f, float(cc + 1) / 32.0f,
					        float(rr) /  4.0f, float(rr + 1) /  4.0f};
				}
			}
			if (size == 8) {
				auto cc = tile % 32;
				auto rr = tile / 32;
				return {float(cc) / 32.0f, float(cc + 1) / 32.0f,
				        float(rr) /  8.0f, float(rr + 1) /  8.0f};
			}
			auto s = tile / 4; // tiles (4s..4s+3) form the 16x16 sprite
			auto cc = s % 16;
			auto rr = s / 16;
			return {float(cc) / 16.0f, float(cc + 1) / 16.0f,
			        float(rr) /  4.0f, float(rr + 1) /  4.0f};
		};

		// Render one sprite from the sprite attribute table at the given
		// zoom factor (used by the Sprite attributes view).
		auto spriteW = (type == SMS) ? 8 : size; // SMS sprites are 8 pixels wide
		auto spriteH = size;
		auto renderSpriteAttrib = [&](int sprite, float zoomFactor) {
			auto zoomVec = zoomFactor * gl::vec2{float(spriteW), float(spriteH)};
			auto [u1, u2, v1, v2] = spriteUV(spriteTile(sprite));
			if (type == TMS) {
				auto flags = vram[(attBase + 4 * sprite + 3) & 0x3fff];
				auto color = flags & 0x0f;
				auto tint = (transparent && (color == 0))
				          ? ImVec4{} : ImGui::ColorConvertU32ToFloat4(palette[color]);
				ImGui::ImageWithBg(patternTex.getImGui(), zoomVec,
					{u1, v1}, {u2, v2}, {}, tint);
			} else {
				ImGui::Image(patternTex.getImGui(), zoomVec, {u1, v1}, {u2, v2});
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
		auto selectLine = [&](int line, bool limitPerLine, bool stopAtTerminator,
		                      std::array<Selected, 64>& selected) -> int {
			int count = 0;
			for (int sprite = 0; sprite < total; ++sprite) {
				int parseLine = line - 1;
				int spriteY = vram[yAddress(sprite)];
				if (stopAtTerminator && (spriteY == stopY)) break;
				if (spriteY >= 240) spriteY -= 256;
				if (mag && (count < maxZoomV)) {
					parseLine >>= 1;
					spriteY >>= 1;
				}
				if ((parseLine >= spriteY) && (parseLine < (spriteY + size))) {
					if (!limitPerLine || (count < maxSprites)) {
						Selected& s = selected[count];
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
						++count;
					}
				}
			}
			return count;
		};

		im::TreeNode("Sprite patterns", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			if (type == NONE) {
				ImGui::TextUnformatted("No sprites in this screen mode"sv);
				return;
			}
			auto fullSize = gl::vec2(patternTexSize) * float(zm);
			im::Child("##pattern", {0, childHeight(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto pos1 = ImGui::GetCursorPos();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				if (type == SMS) {
					drawTransparentBackground(fullSize);
					ImGui::SetCursorPos(pos1);
				}
				ImGui::Image(patternTex.getImGui(), fullSize);
				gl::vec2 zoomPatSize{float(spriteW * zm), float(spriteH * zm)};
				bool hovered = ImGui::IsItemHovered();
				if (hovered) {
					gridPosition = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / zoomPatSize);
				}
				auto pattern = [&] {
					if (type == SMS) {
						return (size == 16) ? ((32 * gridPosition.y) + gridPosition.x) * 2
						                    : ((32 * gridPosition.y) + gridPosition.x);
					}
					return (size == 16) ? ((16 * gridPosition.y) + gridPosition.x) * 4
					                    : ((32 * gridPosition.y) + gridPosition.x);
				}();
				copySpriteDataPopup("Copy pattern data to clipboard", [&] {
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
						drawTransparentBackground(float(z) * zoomPatSize);
						ImGui::SetCursorPos(pos2);
					}
					ImGui::Image(patternTex.getImGui(), float(z) * zoomPatSize, uv1, uv2);
					if (grid) {
						if (!zoomGridTex.get()) {
							zoomGridTex = gl::Texture(false, true); // no interpolation, with wrapping
						}
						int s = z * zm;
						for (auto y : xrange(s)) {
							auto* line = &pixels[y * s];
							for (auto x : xrange(s)) {
								line[x] = (x == 0 || y == 0) ? gColor : 0;
							}
						}
						zoomGridTex.bind();
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s, s, 0,
							GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
						ImGui::SetCursorPos(pos2);
						ImGui::Image(zoomGridTex.getImGui(),
						             float(z) * zoomPatSize, {},
						             gl::vec2{float(spriteW), float(spriteH)});
					}
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
				int count = selectLine(line, true, canStopY, selected);
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
			auto fullSize = zoomSize * gl::vec2(8, rows);
			im::Child("##attrib", {0, childHeight(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				if (type == NONE) {
					ImGui::TextUnformatted("No sprites in this screen mode"sv);
				} else {
					gl::vec2 topLeft = ImGui::GetCursorPos();
					gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
					drawTransparentBackground(fullSize);
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
						gridPosition = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / zoomSize);
					}
					auto sprite = 8 * gridPosition.y + gridPosition.x;
					copySpriteDataPopup("Copy sprite data to clipboard", [&]{
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
						drawTransparentBackground(3.0f * zoomSize);
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
			struct SpriteBox {
				int x, y, w, h; // box
				int sprite;
				uint8_t vramX, vramY;
				unsigned pattern;
			};

			std::array<uint32_t, 256 * 256> screen;
			memset(screen.data(), 0, sizeof(uint32_t) * 256 * lines);

			// Per-line simulation, mirroring SMSVDPCore::selectSprites and
			// drawSpritesTms9918Mode()/drawSpritesMode4().
			for (int line = 0; line < lines; ++line) {
				std::array<Selected, 64> selected;
				int count = selectLine(line, enableLimitPerLine, !renderAfterStopY && canStopY, selected);
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
			std::array<SpriteBox, 64> clippedBoxes;
			int nrClippedBoxes = 0;
			auto addClippedBox = [&](SpriteBox b) {
				assert(nrClippedBoxes < 64);
				clippedBoxes[nrClippedBoxes] = b;
				++nrClippedBoxes;
			};
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
				if (x < 0) {
					w += x;
					if (w <= 0) continue;
					x = 0;
				} else if (x + w > 256) {
					w = 256 - x;
				}
				// No wrap to the top: Y>=240 is already the negative range.
				if (y < 0) {
					h += y;
					if (h <= 0) continue;
					y = 0;
				} else if (y + h > lines) {
					h = lines - y;
					if (h <= 0) continue;
				}
				addClippedBox(SpriteBox{x, y, w, h, sprite, vramX, vramY, pattern});
			}

			if (!renderTex.get()) {
				renderTex = gl::Texture(false, true); // no interpolation, with wrapping
			}
			renderTex.bind();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, lines, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, screen.data());

			auto fullSize = float(zm) * gl::vec2(256, float(lines));
			im::Child("##screen", {0.0f, childHeight(fullSize.y)}, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
				auto* drawList = ImGui::GetWindowDrawList();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				auto boxColor = ImGui::ColorConvertFloat4ToU32(boundingBoxColor);
				auto drawBox = [&](int x, int y, int w, int h) {
					gl::vec2 tl = scrnPos + gl::vec2{float(x), float(y)} * float(zm);
					gl::vec2 br = tl + gl::vec2{float(w), float(h)} * float(zm);
					drawList->AddRect(tl, br, boxColor);
				};

				gl::vec2 topLeft = ImGui::GetCursorPos();
				drawTransparentBackground(fullSize);
				ImGui::SetCursorPos(topLeft);
				ImGui::Image(renderTex.getImGui(), fullSize);
				bool hovered = ImGui::IsItemHovered();
				auto hoverPos = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / gl::vec2(float(zm)));
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
					{
						auto maxStr = tmpStrCat("Max ", maxSprites, " sprites per line");
						ImGui::Checkbox(maxStr.c_str(), &enableLimitPerLine);
					}
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
					for (int i : xrange(nrClippedBoxes)) {
						const auto& b = clippedBoxes[i];
						if ((b.x <= hx) && (hx < (b.x + b.w)) &&
						    (b.y <= hy) && (hy < (b.y + b.h))) {
							ImGui::Bullet();
							ImGui::StrCat("sprite ", b.sprite, ": pat=", b.pattern, " (0x", hex_string<3>(b.pattern), ")\n"
							              "x,y=", b.vramX, ',', b.vramY, " (0x", hex_string<2>(b.vramX), ",0x", hex_string<2>(b.vramY), ')');
						}
					}
					ImGui::EndTooltip();
					if (drawBoundingBox && (boundingBoxOnAll == 0)) {
						for (int i : xrange(nrClippedBoxes)) {
							const auto& b = clippedBoxes[i];
							if ((b.x <= hx) && (hx < (b.x + b.w)) &&
							    (b.y <= hy) && (hy < (b.y + b.h))) {
								drawBox(b.x, b.y, b.w, b.h);
							}
						}
					}
				}

				if (drawBoundingBox && (boundingBoxOnAll == 1)) {
					for (int i : xrange(nrClippedBoxes)) {
						const auto& b = clippedBoxes[i];
						drawBox(b.x, b.y, b.w, b.h);
					}
				}
			});
		});
	});
}

} // namespace openmsx
