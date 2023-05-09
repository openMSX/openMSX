#include "ImGuiSpriteViewer.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiPalette.hh"
#include "ImGuiUtils.hh"

#include "MSXMotherBoard.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"

#include "StringOp.hh"
#include "one_of.hh"
#include "xrange.hh"

#include <imgui.h>

#include <cassert>
#include <cstdint>
#include <span>

namespace openmsx {

ImGuiSpriteViewer::ImGuiSpriteViewer(ImGuiManager& manager_)
	: manager(manager_)
{
}

void ImGuiSpriteViewer::save(ImGuiTextBuffer& buf)
{
	buf.appendf("show=%d\n", show);
	buf.appendf("override=%d\n", manual);
	buf.appendf("mode=%d\n", manualMode);
	buf.appendf("size=%d\n", manualSize);
	buf.appendf("mag=%d\n", manualMag);
	buf.appendf("transparent=%d\n", manualTransparent);
	buf.appendf("patBase=%d\n", manualPatBase);
	buf.appendf("attBase=%d\n", manualAttBase);
	buf.appendf("zoom=%d\n", zoom);
	buf.appendf("showGrid=%d\n", grid);
	buf.appendf("checkerBoardSize=%d\n", checkerBoardSize);
	buf.appendf("gridColor=[ %f %f %f %f ]\n", gridColor[0], gridColor[1], gridColor[2], gridColor[3]);
	buf.appendf("checkerBoardColor1=[ %f %f %f %f ]\n", checkerBoardColor1[0], checkerBoardColor1[1], checkerBoardColor1[2], checkerBoardColor1[3]);
	buf.appendf("checkerBoardColor2=[ %f %f %f %f ]\n", checkerBoardColor2[0], checkerBoardColor2[1], checkerBoardColor2[2], checkerBoardColor2[3]);
}

void ImGuiSpriteViewer::loadLine(std::string_view name, zstring_view value)
{
	auto checkIntRange = [&](int& result, unsigned max) {
		if (auto r = StringOp::stringTo<unsigned>(value)) {
			if (*r < max) result = *r;
		}
	};

	if (name == "show") {
		show = StringOp::stringToBool(value);
	} else if (name == "override") {
		checkIntRange(manual, 2);
	} else if (name == "mode") {
		checkIntRange(manualMode, 3);
		if (manualMode == 0) manualMode = 1;
	} else if (name == "size") {
		checkIntRange(manualSize, 17);
		if (manualSize != one_of(8, 16)) manualSize = 8;
	} else if (name == "mag") {
		checkIntRange(manualMag, 2);
	} else if (name == "transparent") {
		checkIntRange(manualTransparent, 2);
	} else if (name == "patBase") {
		checkIntRange(manualPatBase, 0x20000);
	} else if (name == "attBase") {
		checkIntRange(manualAttBase, 0x20000);
	} else if (name == "zoom") {
		checkIntRange(zoom, 8);
	} else if (name == "showGrid") {
		grid = StringOp::stringToBool(value);
	} else if (name == "checkerBoardSize") {
		checkIntRange(checkerBoardSize, 256);
	} else if (name == "gridColor") {
		gl::vec4 t;
		if (sscanf(value.c_str(), "[ %f %f %f %f ]", &t[0], &t[1], &t[2], &t[3]) == 4) {
			gridColor = t;
		}
	} else if (name == "checkerBoardColor1") {
		gl::vec4 t;
		if (sscanf(value.c_str(), "[ %f %f %f %f ]", &t[0], &t[1], &t[2], &t[3]) == 4) {
			checkerBoardColor1 = t;
		}
	} else if (name == "checkerBoardColor2") {
		gl::vec4 t;
		if (sscanf(value.c_str(), "[ %f %f %f %f ]", &t[0], &t[1], &t[2], &t[3]) == 4) {
			checkerBoardColor2 = t;
		}
	}
}

static uint8_t vpeek(std::span<const uint8_t> vram, bool planar, size_t addr)
{
	if (planar) {
		addr = ((addr << 16) | (addr >> 1)) & 0x1FFFF;
	}
	return (addr < vram.size()) ? vram[addr] : 0xFF;
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

static void renderPatterns8(std::span<const uint8_t> vram, bool planar, int patBase, std::span<uint32_t> output)
{
	for (auto row : xrange(8)) {
		for (auto column : xrange(32)) {
			auto patNum = 32 * row + column;
			auto addr = patBase + 8 * patNum;
			for (auto y : xrange(8)) {
				auto pattern = vpeek(vram, planar, addr + y);
				auto out = subspan<8>(output, (8 * row + y) * 256 + 8 * column);
				draw8(pattern, 0xffffffff, 0x00000000, out);
			}
		}
	}
}

static void renderPatterns16(std::span<const uint8_t> vram, bool planar, int patBase, std::span<uint32_t> output)
{
	for (auto row : xrange(4)) {
		for (auto column : xrange(16)) {
			auto patNum = 4 * (16 * row + column);
			auto addr = patBase + 8 * patNum;
			for (auto y : xrange(16)) {
				auto patternA = vpeek(vram, planar, addr + y +  0);
				auto patternB = vpeek(vram, planar, addr + y + 16);
				auto outA = subspan<8>(output, (16 * row + y) * 256 + 16 * column +  0);
				auto outB = subspan<8>(output, (16 * row + y) * 256 + 16 * column +  8);
				draw8(patternA, 0xffffffff, 0x00000000, outA);
				draw8(patternB, 0xffffffff, 0x00000000, outB);
			}
		}
	}
}

static int getSpriteAttrAddr(int base, int sprite, int mode)
{
	return base + (mode == 2 ? 512 : 0) + 4 * sprite;
}
static int getSpriteColorAddr(int base, int sprite, int mode)
{
	assert(mode == 2);
	return base + 16 * sprite;
}

static void renderSpriteAttrib(std::span<const uint8_t> vram, bool planar, int attBase, int sprite, int mode, int size, int transparent,
                               float zoom, std::span<uint32_t, 16> palette, void* patternTex)
{
	int addr = getSpriteAttrAddr(attBase, sprite, mode);
	int pattern = vpeek(vram, planar, addr + 2);
	if (size == 16) pattern /= 4;

	int patternsPerRow = 256 / size;
	int cc = pattern % patternsPerRow;
	int rr = pattern / patternsPerRow;
	float u1 = float(cc + 0) / float(patternsPerRow);
	float u2 = float(cc + 1) / float(patternsPerRow);
	float v1 = float(size * rr) / 64.0f;

	auto getColor = [&](int color) -> gl::vec4 {
		if (color == 0 && transparent) return {};
		return ImGui::ColorConvertU32ToFloat4(palette[color]);
	};

	if (mode == 1) {
		auto attrib = vpeek(vram, planar, addr + 3);
		auto color = attrib & 0x0f;
		float v2 = float(size * (rr + 1)) / 64.0f;
		ImGui::Image(patternTex, zoom * gl::vec2{float(size)}, {u1, v1}, {u2, v2}, getColor(color));
	} else {
		int colorBase = getSpriteColorAddr(attBase, sprite, mode);
		gl::vec2 pos = ImGui::GetCursorPos();
		for (auto y : xrange(size)) {
			auto attrib = vpeek(vram, planar, colorBase + y);
			auto color = attrib & 0x0f;
			ImGui::SetCursorPos({pos[0], pos[1] + zoom * float(y)});
			float v2 = v1 + (1.0f / 64.0f);
			ImGui::Image(patternTex, zoom * gl::vec2{float(size), 1.0f}, {u1, v1}, {u2, v2}, getColor(color));
			v1 = v2;
		}
	}
}

void ImGuiSpriteViewer::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	im::Window("Sprite viewer", &show, [&]{
		VDP* vdp = dynamic_cast<VDP*>(motherBoard->findDevice("VDP")); // TODO name based OK?
		if (!vdp) return;
		const auto& vram = vdp->getVRAM().getData();

		auto modeToStr = [](int mode) {
			if (mode == 0) return "no sprites";
			if (mode == 1) return "1";
			if (mode == 2) return "2";
			assert(false); return "ERROR";
		};
		auto sizeToStr = [](int size) {
			if (size ==  8) return "8 x 8";
			if (size == 16) return "16 x 16";
			assert(false); return "ERROR";
		};
		auto yesNo = [](int x) {
			if (x == 0) return "no";
			if (x == 1) return "yes";
			assert(false); return "ERROR";
		};
		auto attMult = [](int mode) { return 1 << ((mode == 2) ? 10 : 7); };

		bool isMSX1 = vdp->isMSX1VDP();
		auto displayMode = vdp->getDisplayMode();
		bool planar = displayMode.isPlanar();
		int vdpMode = displayMode.getSpriteMode(isMSX1);

		int vdpSize = vdp->getSpriteSize();
		int vdpMag = vdp->isSpriteMag();
		int vdpTransparent = vdp->getTransparency();

		int vdpPatBase = vdp->getSpritePatternTableBase();
		int vdpAttBase = vdp->getSpriteAttributeTableBase() & ~(attMult(vdpMode) - 1);

		std::array<uint32_t, 16> palette;
		auto msxPalette = manager.palette.getPalette(vdp);
		ranges::transform(msxPalette, palette.data(),
			[](uint16_t msx) { return ImGuiPalette::toRGBA(msx); });
		// TODO? if (color0 < 16) palette[0] = palette[color0];

		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			im::Group([&]{
				ImGui::RadioButton("Use VDP settings", &manual, 0);
				im::Disabled(manual != 0, [&]{
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Sprite mode: ", modeToStr(vdpMode));
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Sprite size: ", sizeToStr(vdpSize));
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Sprites magnified: ", yesNo(vdpMag));
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Color 0 transparent: ", yesNo(vdpTransparent));
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Pattern table: 0x", hex_string<5>(vdpPatBase));
					ImGui::AlignTextToFramePadding();
					ImGui::StrCat("Attribute table: 0x", hex_string<5>(vdpAttBase));
				});
			});
			ImGui::SameLine();
			im::Group([&]{
				ImGui::RadioButton("Manual override", &manual, 1);
				im::Disabled(manual != 1, [&]{
					im::ItemWidth(ImGui::GetFontSize() * 9.0f, [&]{
						im::Combo("##mode", modeToStr(manualMode), [&]{
							if (ImGui::Selectable("1")) manualMode = 1;
							if (ImGui::Selectable("2")) manualMode = 2;
						});
						im::Combo("##size", sizeToStr(manualSize), [&]{
							if (ImGui::Selectable(" 8 x  8")) manualSize =  8;
							if (ImGui::Selectable("16 x 16")) manualSize = 16;
						});
						im::Combo("##mag", yesNo(manualMag), [&]{
							if (ImGui::Selectable("no"))  manualMag = 0;
							if (ImGui::Selectable("yes")) manualMag = 1;
						});
						im::Combo("##trans", yesNo(manualTransparent), [&]{
							if (ImGui::Selectable("no"))  manualTransparent = 0;
							if (ImGui::Selectable("yes")) manualTransparent = 1;
						});
						comboHexSequence<5>("##pat", &manualPatBase, 8 * 256);
						comboHexSequence<5>("##att", &manualAttBase, attMult(manualMode));
					});
				});
			});
			ImGui::SameLine();
			ImGui::Dummy(ImVec2(25, 1));
			ImGui::SameLine();
			im::Group([&]{
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
				im::Disabled(!grid, [&]{
					ImGui::ColorEdit4("Grid color", &gridColor[0],
						ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
				});

				ImGui::TextUnformatted("Checkerboard:");
				simpleToolTip("Used as background in 'Sprite attribute' and 'Rendered sprites' view");
				ImGui::SameLine();
				ImGui::ColorEdit4("checkerboard color1", &checkerBoardColor1[0], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
				ImGui::SameLine();
				ImGui::ColorEdit4("checkerboard color2", &checkerBoardColor2[0], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
				im::Indent([&]{
					ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
					ImGui::InputInt("size", &checkerBoardSize);
				});
			});
		});
		ImGui::Separator();

		int mode = manual ? manualMode : vdpMode;
		int size = manual ? manualSize : vdpSize;
		//int mag  = manual ? manualMag  : vdpMag;
		int transparent  = manual ? manualTransparent  : vdpTransparent;
		int patBase = manual ? manualPatBase : vdpPatBase;
		int attBase = manual ? manualAttBase : vdpAttBase;
		assert((attBase % attMult(mode)) == 0);

		// create pattern texture
		if (!patternTex.get()) {
			patternTex = gl::Texture(false, false); // no interpolation, no wrapping
		}
		patternTex.bind();
		std::array<uint32_t, 256 * 64> pixels;
		if (mode != 0) {
			if (size == 8) {
				renderPatterns8 (vram, planar, patBase, pixels);
			} else {
				renderPatterns16(vram, planar, patBase, pixels);
			}
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 64, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		} else {
			pixels[0] = 0xFF808080; // gray
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		}

		// create grid texture
		int zm = 2 * (1 + zoom);
		auto gColor = ImGui::ColorConvertFloat4ToU32(gridColor);
		if (grid) {
			auto gridSize = size * zm;
			for (auto y : xrange(gridSize)) {
				auto* line = &pixels[y * gridSize];
				for (auto x : xrange(gridSize)) {
					line[x] = (x == 0 || y == 0) ? gColor : 0;
				}
			}
			if (!gridTex.get()) {
				gridTex = gl::Texture(false, true); // no interpolation, with wrapping
			}
			gridTex.bind();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gridSize, gridSize, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		}

		// create checker board texture
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

		im::TreeNode("Sprite patterns", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			auto fullSize = gl::vec2(256, 64) * float(zm);
			im::Child("##pattern", {0, fullSize[1]}, false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysAutoResize, [&]{
				auto pos1 = ImGui::GetCursorPos();
				gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
				ImGui::Image(reinterpret_cast<void*>(patternTex.get()), fullSize);
				bool hovered = ImGui::IsItemHovered() && (mode != 0);
				ImGui::SameLine();
				im::Group([&]{
					gl::vec2 zoomPatSize{float(size * zm)};
					if (hovered) {
						auto gridPos = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / zoomPatSize);
						auto pattern = (size == 16) ? ((16 * gridPos[1]) + gridPos[0]) * 4
						                            : ((32 * gridPos[1]) + gridPos[0]) * 1;
						ImGui::StrCat("pattern: ", pattern);
						auto recipPatTex = recip((size == 16) ? gl::vec2{16, 4} : gl::vec2{32, 8});
						auto uv1 = gl::vec2(gridPos) * recipPatTex;
						auto uv2 = uv1 + recipPatTex;
						auto pos2 = ImGui::GetCursorPos();
						int z = (size == 16) ? 3 : 6;
						ImGui::Image(reinterpret_cast<void*>(patternTex.get()), float(z) * zoomPatSize, uv1, uv2);
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
							ImGui::Image(reinterpret_cast<void*>(zoomGridTex.get()),
							             float(z) * zoomPatSize, {}, gl::vec2{float(size)});
						}
					} else {
						ImGui::Dummy(zoomPatSize);
					}
				});
				if (grid) {
					ImGui::SetCursorPos(pos1);
					ImGui::Image(reinterpret_cast<void*>(gridTex.get()), fullSize,
						{}, (size == 8) ? gl::vec2{32.0f, 8.0f} : gl::vec2{16.0f, 4.0f});
				}
			});
		});
		ImGui::Separator();

		im::TreeNode("Sprite attributes", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			float zoomSize = float(zm * size);
			auto fullSize = zoomSize * gl::vec2(8, 4);
			im::Child("##attrib", {0, fullSize[1]}, false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysAutoResize, [&]{
				if (mode == 0) {
					ImGui::TextUnformatted("No sprites in this screen mode");
				} else {
					gl::vec2 topLeft = ImGui::GetCursorPos();
					gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
					ImGui::Dummy(fullSize);
					bool hovered = ImGui::IsItemHovered();
					if (checkerBoardSize) {
						ImGui::SetCursorPos(topLeft);
						ImGui::Image(reinterpret_cast<void*>(checkerTex.get()), fullSize,
							{}, fullSize / (4.0f * float(checkerBoardSize)));
					}
					for (auto row : xrange(4)) {
						for (auto column : xrange(8)) {
							int sprite = 8 * row + column;
							ImGui::SetCursorPos(topLeft + zoomSize * gl::vec2(float(column), float(row)));
							renderSpriteAttrib(vram, planar, attBase, sprite, mode, size, transparent,
							                   float(zm), palette, reinterpret_cast<void*>(patternTex.get()));
						}
					}
					if (grid) {
						ImGui::SetCursorPos(topLeft);
						ImGui::Image(reinterpret_cast<void*>(gridTex.get()), fullSize,
							{}, gl::vec2{8, 4});
					}
					if (hovered) {
						gl::vec2 zoomPatSize{float(size * zm)};
						auto gridPos = trunc((gl::vec2(ImGui::GetIO().MousePos) - scrnPos) / zoomPatSize);
						auto sprite = 8 * gridPos[1] + gridPos[0];

						ImGui::SameLine();
						im::Group([&]{
							ImGui::StrCat("sprite: ", sprite);
							auto pos = ImGui::GetCursorPos();
							if (checkerBoardSize) {
								ImGui::Image(reinterpret_cast<void*>(checkerTex.get()), 3.0f * zoomPatSize,
									{}, zoomPatSize / (4.0f * float(checkerBoardSize)));
							}
							ImGui::SetCursorPos(pos);
							renderSpriteAttrib(vram, planar, attBase, sprite, mode, size, transparent,
							                   float(3 * zm), palette, reinterpret_cast<void*>(patternTex.get()));
						});
						ImGui::SameLine();
						im::Group([&]{
							int addr = getSpriteAttrAddr(attBase, sprite, mode);
							ImGui::StrCat("x: ", vpeek(vram, planar, addr + 1),
							              "  y: ", vpeek(vram, planar, addr + 0));
							ImGui::StrCat("pattern: ", vpeek(vram, planar, addr + 2));
							if (mode == 1) {
								auto c = vpeek(vram, planar, addr + 3);
								ImGui::StrCat("color: ", c & 15, (c & 80 ? " (EC)" : ""));
							} else {
								int colorBase = getSpriteColorAddr(attBase, sprite, mode);
								im::StyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1), [&]{ // Tighten spacing
									ImGui::TextUnformatted("Colors per line (hex):");
									for (auto y : xrange(4)) {
										for (auto x : xrange(4)) {
											auto line = 4 * y + x;
											auto a = vpeek(vram, planar, colorBase + line);
											ImGui::StrCat(hex_string<1>(line), ": ",
											              hex_string<1>(a & 15),
											              (a & 0xe0 ? '*' : ' '),
											              ' ');
											if (x != 3) ImGui::SameLine();
										}
									}
								});
							}
						});
					}
				}
			});
		});
		ImGui::Separator();

		im::TreeNode("Rendered sprites", [&]{
			ImGui::TextUnformatted("TODO");
		});

	});
}

} // namespace openmsx
