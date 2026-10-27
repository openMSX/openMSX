#include "ImGuiSMSVDPRegs.hh"

#include "Franky.hh"
#include "SMSVDP.hh"

#include "ImGuiCpp.hh"
#include "ImGuiUtils.hh"

#include "MSXMotherBoard.hh"

#include "strCat.hh"
#include "xrange.hh"
#include "zstring_view.hh"

#include "imgui.h"
#include "imgui_internal.h"

#include <array>
#include <bit>
#include <string>
#include <utility>

namespace openmsx {

ImGuiSMSVDPRegs::ImGuiSMSVDPRegs(ImGuiManager& manager_, size_t index)
	: ImGuiPart(manager_)
	, title(strCat("SMS VDP registers", strCat_if(index, " (", index + 1, ')')))
{
}

void ImGuiSMSVDPRegs::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiSMSVDPRegs::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

// HACK: globals, the explanation functions are plain function pointers.
static const SMSVDP* g_vdp;
static std::array<uint8_t, 64 + 1> registerValues; // 0..10 registers, 64 = status

// The mode as the current register values select it (like
// SMSVDPCore::selectDisplayMode(), but invalid TMS9918 combinations are
// reported as such instead of keeping the previous mode).
struct DisplayMode {
	int mode = -1; // 0-3: TMS9918 modes, 4: mode 4, -1: invalid
	int lines = 192;
};
static bool m1() { return (registerValues[1] & 0x10) != 0; }
static bool m2() { return (registerValues[0] & 0x02) != 0; }
static bool m3() { return (registerValues[1] & 0x08) != 0; }
static bool m4() { return (registerValues[0] & 0x04) != 0; }
static DisplayMode getMode()
{
	if (m4()) {
		if ((g_vdp->getVariant() == SMSVDPCore::Variant::SMS2) && m2()) {
			if ( m1() && !m3()) return {4, 224};
			if (!m1() &&  m3()) return {4, 240};
		}
		return {4};
	}
	if (!m1() && !m2() && !m3()) return {0}; // Graphics I
	if ( m1() && !m2() && !m3()) return {1}; // Text
	if (!m1() &&  m2() && !m3()) return {2}; // Graphics II
	if (!m1() && !m2() &&  m3()) return {3}; // Multicolor
	return {};
}
static bool isMode4() { return getMode().mode == 4; }

static TemporaryString modeName()
{
	auto [mode, lines] = getMode();
	switch (mode) {
	case 0: return TemporaryString("graphics I (mode 0)");
	case 1: return TemporaryString("text (mode 1)");
	case 2: return TemporaryString("graphics II (mode 2)");
	case 3: return TemporaryString("multicolor (mode 3)");
	case 4: return (lines == 192) ? TemporaryString("mode 4")
	                              : tmpStrCat("mode 4, ", lines, " lines");
	default: {
		std::string bits;
		for (auto [set, name] : {std::pair{m1(), "M1"}, std::pair{m2(), "M2"}, std::pair{m3(), "M3"}}) {
			if (!set) continue;
			if (!bits.empty()) bits += '+';
			bits += name;
		}
		return tmpStrCat("invalid mode bits (", bits, ')');
	}
	}
}

// Table addresses, mirroring SMSVDPCore::drawScanlineMode*.
static unsigned nameTableBase()
{
	auto r2 = registerValues[2];
	if (auto [mode, lines] = getMode(); mode == 4) {
		return (lines != 192) ? (((r2 & 0x0c) << 10) | 0x0700)
		                      : ((r2 << 10) & 0x3800);
	}
	return (r2 & 0x0f) << 10;
}
static unsigned colorTableBase() { return (registerValues[3] << 6) & 0x3fff; }
static unsigned patternTableBase() { return (registerValues[4] << 11) & 0x3fff; }
static unsigned spriteAttBase()
{
	auto r5 = registerValues[5];
	return isMode4() ? ((r5 << 7) & 0x3f00) : ((r5 & 0x7f) << 7);
}
static unsigned spritePatBase() { return (registerValues[6] & 0x07) << 11; }

using Bits = std::array<const char*, 8>;
struct RegisterDescription {
	Bits bits;
	std::string_view name;
};
using RD = RegisterDescription;
static constexpr auto registerDescriptions = std::array{
	RD{{"VSL", "HSL", "LC ", "IE1", "SFT", "M4 ", "M2 ", " 0 "}, "Mode register 0"}, // R#0
	RD{{" 0 ", "DE ", "IE0", "M1 ", "M3 ", " 0 ", "SS ", "SZ "}, "Mode register 1"}, // R#1
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "A13", "A12", "A11", "A10"}, "Name table base address register"}, // R#2
	RD{{"A13", "A12", "A11", "A10", "A9 ", "A8 ", "A7 ", "A6 "}, "Color table base address register (modes 0-3) / SMS1 tile mask (mode 4)"}, // R#3
	RD{{" 0 ", " 0 ", " 0 ", "A15", "A14", "A13", "A12", "A11"}, "Pattern table base address register (modes 0-3) / SMS1 tile mask (mode 4)"}, // R#4
	RD{{"A14", "A13", "A12", "A11", "A10", "A9 ", "A8 ", "A7 "}, "Sprite attribute table base address register"}, // R#5
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "A13", "A12", "A11"}, "Sprite pattern table base address register (modes 0-3) / sprite tile bank select bit 2 (mode 4)"}, // R#6
	RD{{"TC3", "TC2", "TC1", "TC0", "BD3", "BD2", "BD1", "BD0"}, "Text color / backdrop color register"}, // R#7
	RD{{"H7 ", "H6 ", "H5 ", "H4 ", "H3 ", "H2 ", "H1 ", "H0 "}, "Horizontal scroll register"}, // R#8
	RD{{"V7 ", "V6 ", "V5 ", "V4 ", "V3 ", "V2 ", "V1 ", "V0 "}, "Vertical scroll register"}, // R#9
	RD{{"L7 ", "L6 ", "L5 ", "L4 ", "L3 ", "L2 ", "L1 ", "L0 "}, "Line counter reload register"}, // R#10
};
static constexpr RegisterDescription statusDescription =
	RD{{" F ", "5S ", " C ", "SN4", "SN3", "SN2", "SN1", "SN0"}, "Status register"};

struct SubFunction {
	uint8_t reg = uint8_t(-1);
	uint8_t mask = 0;
};
struct RegFunction {
	// drawSection() draws this item when 'subs[0].reg' or 'subs[1].reg' is
	// part of 'showRegisters'; when 'subs[0].mask == 0' the item cannot be
	// highlighted (typically such items show a fixed string).
	std::array<SubFunction, 2> subs;
	std::string_view toolTip;
	TemporaryString(*displayFunc)(uint32_t);
};
using S = SubFunction;
using R = RegFunction;
using namespace std::literals;

static constexpr auto regFunctions = std::array{
	// mode registers
	R{{S{0, 0}}, "", [](uint32_t) { return TemporaryString("screen: "); }},
	R{{S{1, 0x40}}, "display enable/disable",
	  [](uint32_t v) { return TemporaryString(v ? "display enabled" : "display disabled"); }},
	R{{S{0, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{1, 0x18}, S{0, 0x06}}, "display mode",
	  [](uint32_t) { return modeName(); }},
	R{{S{0, 0}}, "", [](uint32_t) { return TemporaryString("\n"); }},

	R{{S{1, 0x20}}, "V-Blank interrupt enable/disable",
	  [](uint32_t v) { return tmpStrCat("VINT: ", v ? "enabled"sv : "disabled"sv); }},
	R{{S{0, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{0, 0x10}}, "line interrupt enable/disable",
	  [](uint32_t v) { return tmpStrCat("HINT: ", v ? "enabled"sv : "disabled"sv, '\n'); }},

	R{{S{1, 0x02}}, "sprite size: 8x8 or 16x16 (modes 0-3), 8x8 or 8x16 (mode 4)",
	  [](uint32_t v) {
		if (isMode4()) return tmpStrCat("sprites: size=", v ? "8x16"sv : "8x8"sv);
		return tmpStrCat("sprites: size=", v ? "16x16"sv : "8x8"sv);
	  }},
	R{{S{0, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{1, 0x01}}, "sprite magnification enable/disable",
	  [](uint32_t v) { return TemporaryString(v ? "magnified" : "not magnified"); }},
	R{{S{0, 0}}, "", [](uint32_t) { return TemporaryString("\n"); }},

	R{{S{0, 0x08}}, "sprite shift, 8 pixels to the left (mode 4)",
	  [](uint32_t v) { return tmpStrCat("sprite shift: ", v ? "yes"sv : "no"sv); }},
	R{{S{0, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{0, 0x20}}, "hide leftmost column (mode 4)",
	  [](uint32_t v) { return tmpStrCat("leftmost column: ", v ? "hidden"sv : "shown"sv, '\n'); }},

	R{{S{0, 0x80}}, "vertical scroll lock for the top 2 rows",
	  [](uint32_t v) { return tmpStrCat("v-scroll lock: ", v ? "yes"sv : "no"sv); }},
	R{{S{0, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{0, 0x40}}, "horizontal scroll lock for the top 2 rows",
	  [](uint32_t v) { return tmpStrCat("h-scroll lock: ", v ? "yes"sv : "no"sv, '\n'); }},

	// table base registers
	R{{S{2, 0xFF}}, "name table address",
	  [](uint32_t) { return tmpStrCat("name table:     0x", hex_string<5>(nameTableBase()), '\n'); }},
	R{{S{3, 0xFF}}, "color table address (modes 0-3)",
	  [](uint32_t) {
		if (isMode4()) return tmpStrCat("color table:    (not used in mode 4)\n");
		return tmpStrCat("color table:    0x", hex_string<5>(colorTableBase()), '\n');
	  }},
	R{{S{4, 0xFF}}, "pattern table address (modes 0-3)",
	  [](uint32_t) {
		if (isMode4()) return tmpStrCat("pattern table:  (fixed at 0x0000 in mode 4)\n");
		return tmpStrCat("pattern table:  0x", hex_string<5>(patternTableBase()), '\n');
	  }},
	R{{S{5, 0xFF}}, "sprite attribute table address",
	  [](uint32_t) { return tmpStrCat("sprite attr:    0x", hex_string<5>(spriteAttBase()), '\n'); }},
	// One item for both modes, so that hovering any of these bits
	// highlights the line that is actually shown.
	R{{S{6, 0x07}}, "sprite pattern table address (modes 0-3) / sprite tile bank select, bit 2 (mode 4)",
	  [](uint32_t v) {
		if (isMode4()) {
			return tmpStrCat("sprite tiles:   ", (v & 0x04) ? "256-511"sv : "0-255"sv, '\n');
		}
		return tmpStrCat("sprite pattern: 0x", hex_string<5>(spritePatBase()), '\n');
	  }},

	// scroll registers
	R{{S{8, 0xFF}}, "horizontal scroll",
	  [](uint32_t v) { return tmpStrCat("h-scroll: ", v); }},
	R{{S{8, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{9, 0xFF}}, "vertical scroll",
	  [](uint32_t v) { return tmpStrCat("v-scroll: ", v, '\n'); }},

	// color register
	R{{S{7, 0xF0}}, "text foreground color (mode 1)",
	  [](uint32_t v) { return tmpStrCat("text fg: color ", v >> 4); }},
	R{{S{7, 0x0F}}, "text background (mode 1) / backdrop color (mode 4)",
	  [](uint32_t v) { return tmpStrCat("  bg/backdrop: color ", v, '\n'); }},

	// line counter
	R{{S{10, 0xFF}}, "line counter reload value; the internal counter decrements on each visible line and reloads (raising HINT) when it underflows",
	  [](uint32_t v) {
		return tmpStrCat("line counter: reload=", v, " (HINT every ", v + 1, " lines)\n",
		                 "live counter: ", g_vdp->peekLineCounter(), '\n');
	  }},

	// status register
	R{{S{64, 0x80}}, "V-Blank interrupt pending",
	  [](uint32_t v) { return tmpStrCat("VINT: ", v); }},
	R{{S{64, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{64, 0x40}}, "sprite overflow",
	  [](uint32_t v) { return tmpStrCat(v ? ""sv : "no "sv, "sprite overflow"); }},
	R{{S{64, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{64, 0x20}}, "sprite collision",
	  [](uint32_t v) { return tmpStrCat(v ? ""sv : "no "sv, "sprite collision"); }},
	R{{S{64, 0x1F}}, "sprite number",
	  [](uint32_t v) { return tmpStrCat(" (sprite number=", v, ")\n"); }},
};

static int lookupFunction(uint8_t reg, uint8_t mask)
{
	int i = 0;
	for (const auto& f : regFunctions) {
		for (const auto& s : f.subs) {
			if ((reg == s.reg) && (mask & s.mask)) {
				return i;
			}
		}
		++i;
	}
	return -1;
}

void ImGuiSMSVDPRegs::drawSection(std::span<const uint8_t> showRegisters,
                                     std::span<const uint8_t> regValues,
                                     SMSVDP& vdp, EmuTime time)
{
	ImGui::SameLine();
	HelpMarker("Click to toggle bits, or edit the hex-field.\n"
	           "Right-click to show/hide register explanation.");

	im::Table("table", 10, 0, [&]{
		// isCellHovered:
		//   This uses an internal API, see here for a discussion
		//       https://github.com/ocornut/imgui/issues/6347
		//   In other words: this solution is suggested by the Dear ImGui
		//   developers until something better gets implemented.
		auto mouse_pos = ImGui::GetMousePos();
		const auto* table = ImGui::GetCurrentTable();
		auto isCellHovered = [&] {
			return ImGui::TableGetCellBgRect(table, table->CurrentColumn).Contains(mouse_pos);
		};

		im::ID_for_range(showRegisters.size(), [&](int i) {
			auto reg = showRegisters[i];
			// note:  0..10  regular register
			//       64      status register
			const auto& rd = (reg < 64) ? registerDescriptions[reg] : statusDescription;
			if (ImGui::TableNextColumn()) {
				auto name = tmpStrCat(reg < 64 ? "R#"sv : "S#"sv, reg < 64 ? reg : reg - 64);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(static_cast<std::string_view>(name));
				simpleToolTip(rd.name);
			}
			uint8_t value = regValues[reg];
			bool writeReg = false;
			if (ImGui::TableNextColumn()) {
				if (reg < 64) {
					if (ImGui::InputScalar("##value", ImGuiDataType_U8, &value, nullptr, nullptr, "%02X", ImGuiInputTextFlags_CharsHexadecimal)) {
						writeReg = true;
					}
				} else {
					ImGui::Text("%02X", value);
				}
			}
			const auto& bits = rd.bits;
			im::ID_for_range(8, [&](int bit_) {
				if (ImGui::TableNextColumn()) {
					int bit = 7 - bit_;
					auto mask = narrow<uint8_t>(1 << bit);
					int f = lookupFunction(reg, mask);
					if (f != -1 && f == hoveredFunction) {
						ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
							getColor(imColor::YELLOW));
					}
					im::StyleColor(ImGuiCol_Button, getColor(value & mask ? imColor::KEY_ACTIVE : imColor::KEY_NOT_ACTIVE), [&]{
						if (ImGui::Button(bits[bit_], {40.0f, 0.0f})) {
							value ^= mask;
							writeReg = true;
						}
					});
					if ((f != -1) && isCellHovered()) {
						newHoveredFunction = f;
						if (auto tip = regFunctions[f].toolTip; !tip.empty()) {
							simpleToolTip(tip);
						}
					}
				}
			});
			if (writeReg && (reg < 64)) {
				vdp.changeRegister(reg, value, time);
			}
		});
	});
	if (explanation) {
		auto* drawList = ImGui::GetWindowDrawList();
		gl::vec2 framePadding = ImGui::GetStyle().FramePadding;
		for (auto f : xrange(narrow<int>(regFunctions.size()))) {
			const auto& func = regFunctions[f];
			if (!(contains(showRegisters, func.subs[0].reg) ||
			      contains(showRegisters, func.subs[1].reg))) {
				continue;
			}
			uint32_t value = 0;
			int shift = 0;
			for (const auto& sub : func.subs) {
				if (sub.reg == uint8_t(-1)) break;
				auto tmp = ((regValues[sub.reg] & sub.mask) >> std::countr_zero(sub.mask));
				value |= tmp << shift;
				shift += std::popcount(sub.mask);
			}
			if (auto s = func.displayFunc(value); !s.empty()) {
				auto textColor = getColor(imColor::TEXT);
				bool canHover = func.subs[0].mask != 0;
				if (canHover && (f == hoveredFunction)) {
					gl::vec2 textSize = ImGui::CalcTextSize(s);
					gl::vec2 pos = ImGui::GetCursorScreenPos();
					textColor = getColor(imColor::BLACK);
					drawList->AddRectFilled(pos - framePadding, pos + textSize + 2.0f * framePadding, getColor(imColor::YELLOW));
				}
				im::StyleColor(ImGuiCol_Text, textColor, [&]{
					ImGui::TextUnformatted(s);
				});
				if (canHover) {
					if (ImGui::IsItemHovered()) {
						newHoveredFunction = f;
					}
					if (shift == 1) { // single bit
						if (auto reg = func.subs[0].reg; reg < 64) { // non-status-register
							if (ImGui::IsItemClicked()) {
								auto mask = func.subs[0].mask;
								vdp.changeRegister(reg, regValues[reg] ^ mask, time);
							}
						}
					}
				}
				if (!s.ends_with('\n')) ImGui::SameLine();
			}
		}
	}
}

void ImGuiSMSVDPRegs::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	ImGui::SetNextWindowSize(gl::vec2{41, 29} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window(title.c_str(), &show, [&]{
		auto* franky = dynamic_cast<Franky*>(motherBoard->findDevice("Franky"));
		if (!franky) return;
		auto& vdp = franky->getVdp();
		auto time = motherBoard->getCurrentTime();

		g_vdp = &vdp;
		for (auto reg : xrange(11)) {
			registerValues[reg] = vdp.peekReg(reg);
		}
		registerValues[64] = vdp.peekStatus();

		newHoveredFunction = -1;
		auto make_span = [](const auto& array) {
			return std::span<const uint8_t, std::dynamic_extent>(std::span(array));
		};
		im::TreeNode("Mode registers", &openMode, [&]{
			static constexpr auto modeRegs = std::to_array<uint8_t>({0, 1});
			drawSection(make_span(modeRegs), registerValues, vdp, time);
		});
		im::TreeNode("Table base registers", &openBase, [&]{
			static constexpr auto tableRegs = std::to_array<uint8_t>({2, 3, 4, 5, 6});
			drawSection(make_span(tableRegs), registerValues, vdp, time);
		});
		im::TreeNode("Scroll registers", &openScroll, [&]{
			static constexpr auto scrollRegs = std::to_array<uint8_t>({8, 9});
			drawSection(make_span(scrollRegs), registerValues, vdp, time);
		});
		im::TreeNode("Color register", &openColor, [&]{
			static constexpr auto colorRegs = std::to_array<uint8_t>({7});
			drawSection(make_span(colorRegs), registerValues, vdp, time);
		});
		im::TreeNode("Line counter", &openLineCounter, [&]{
			static constexpr auto counterRegs = std::to_array<uint8_t>({10});
			drawSection(make_span(counterRegs), registerValues, vdp, time);
		});
		im::TreeNode("Status register", &openStatus, [&]{
			static constexpr auto statusRegs = std::to_array<uint8_t>({64});
			drawSection(make_span(statusRegs), registerValues, vdp, time);
		});
		hoveredFunction = newHoveredFunction;

		if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
			ImGui::OpenPopup("context");
		}
		im::Popup("context", [&]{
			if (ImGui::Checkbox("Show explanation ", &explanation)) {
				ImGui::CloseCurrentPopup();
			}
		});
	});
}

} // namespace openmsx
