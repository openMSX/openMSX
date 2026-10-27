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
static constexpr uint8_t STATUS = 11; // index of the status register, after R#0..R#10
static std::array<uint8_t, STATUS + 1> registerValues;

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
// The SMS1 uses some otherwise unused mode 4 register bits as masks.
static bool sms1Mode4()
{
	return isMode4() && (g_vdp->getVariant() == SMSVDPCore::Variant::SMS1);
}

static TemporaryString modeName()
{
	auto [mode, lines] = getMode();
	switch (mode) {
	case 0: return TemporaryString("graphics I (mode 0)");
	case 1: return TemporaryString("text (mode 1)");
	case 2: return TemporaryString("graphics II (mode 2)");
	case 3: return TemporaryString("multicolor (mode 3)");
	case 4: return tmpStrCat("mode 4", strCat_if(lines != 192, ", ", lines, " lines"));
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
// In graphics II, R#3 bit 7 and R#4 bit 2 select the base; the other bits
// mask the address within the table.
static unsigned colorTableBase()
{
	auto r3 = registerValues[3];
	return (getMode().mode == 2) ? ((r3 & 0x80) << 6) : ((r3 << 6) & 0x3fff);
}
static unsigned colorTableMask() { return ((registerValues[3] & 0x7f) << 6) | 0x3f; }
static unsigned patternTableBase()
{
	auto r4 = registerValues[4];
	return (getMode().mode == 2) ? ((r4 & 0x04) << 11) : ((r4 << 11) & 0x3fff);
}
static unsigned patternTableMask() { return ((registerValues[4] & 0x03) << 11) | 0x7ff; }
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
	RD{{" 0 ", "DE ", "IE0", "M1 ", "M3 ", " 0 ", "SI ", "MAG"}, "Mode register 1"}, // R#1
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "A13", "A12", "A11", "A10"}, "Name table base address register / SMS1 row mask bit 0 (mode 4)"}, // R#2
	RD{{"A13", "A12", "A11", "A10", "A9 ", "A8 ", "A7 ", "A6 "}, "Color table base address register (modes 0 and 2) / SMS1 tile mask for planes 0-1 (mode 4)"}, // R#3
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "A13", "A12", "A11"}, "Pattern table base address register (modes 0-3) / SMS1 tile mask for planes 2-3 (mode 4)"}, // R#4
	RD{{" 0 ", "A13", "A12", "A11", "A10", "A9 ", "A8 ", "A7 "}, "Sprite attribute table base address register / SMS1 x/tile offset bit 0 (mode 4)"}, // R#5
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "A13", "A12", "A11"}, "Sprite pattern table base address register (modes 0-3) / sprite tile bank select bit 2 and SMS1 sprite tile mask bits 1-0 (mode 4)"}, // R#6
	RD{{"TC3", "TC2", "TC1", "TC0", "BD3", "BD2", "BD1", "BD0"}, "Text color / backdrop color register"}, // R#7
	RD{{"H7 ", "H6 ", "H5 ", "H4 ", "H3 ", "H2 ", "H1 ", "H0 "}, "Horizontal scroll register"}, // R#8
	RD{{"V7 ", "V6 ", "V5 ", "V4 ", "V3 ", "V2 ", "V1 ", "V0 "}, "Vertical scroll register"}, // R#9
	RD{{"L7 ", "L6 ", "L5 ", "L4 ", "L3 ", "L2 ", "L1 ", "L0 "}, "Line counter reload register"}, // R#10
};
static_assert(registerDescriptions.size() == STATUS);
static constexpr RegisterDescription statusDescription =
	RD{{" F ", "5S ", " C ", "FS4", "FS3", "FS2", "FS1", "FS0"}, "Status register"};

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

	R{{S{0, 0x80}}, "vertical scroll lock for the rightmost 8 columns, 24-31 (mode 4)",
	  [](uint32_t v) { return tmpStrCat("v-scroll lock: ", v ? "yes"sv : "no"sv); }},
	R{{S{0, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{0, 0x40}}, "horizontal scroll lock for the top 2 rows, lines 0-15 (mode 4)",
	  [](uint32_t v) { return tmpStrCat("h-scroll lock: ", v ? "yes"sv : "no"sv, '\n'); }},

	// table base registers
	R{{S{2, 0xFF}}, "name table address; on SMS1 in mode 4, bit 0 masks the name-table row",
	  [](uint32_t v) {
		bool rowMask = sms1Mode4() && !(v & 0x01);
		return tmpStrCat("name table:     0x", hex_string<4>(nameTableBase()),
		                 rowMask ? " (SMS1: rows 16-27 repeat rows 0-11)"sv : ""sv, '\n');
	  }},
	R{{S{3, 0xFF}}, "color table address (modes 0 and 2); in mode 2 bit 7 selects the base and bits 6-0 mask the address; on SMS1 in mode 4, tile mask for bit planes 0-1",
	  [](uint32_t v) {
		switch (auto mode = getMode().mode) {
		case 4:
			if (auto mask = (v << 1) | 1; sms1Mode4() && (mask != 0x1ff)) {
				return tmpStrCat("SMS1 tile mask 1: 0x", hex_string<3>(mask), " (planes 0-1)\n");
			}
			[[fallthrough]];
		case 1: case 3:
			return tmpStrCat("color table:    (not used in mode ", mode, ")\n");
		case 2:
			return tmpStrCat("color table:    0x", hex_string<4>(colorTableBase()),
			                 " (mask 0x", hex_string<4>(colorTableMask()), ")\n");
		default:
			return tmpStrCat("color table:    0x", hex_string<4>(colorTableBase()), '\n');
		}
	  }},
	R{{S{4, 0xFF}}, "pattern table address (modes 0-3); in mode 2 bit 2 selects the base and bits 1-0 mask the address; on SMS1 in mode 4, bits 2-0 are the tile mask for bit planes 2-3",
	  [](uint32_t v) {
		switch (getMode().mode) {
		case 2:
			return tmpStrCat("pattern table:  0x", hex_string<4>(patternTableBase()),
			                 " (mask 0x", hex_string<4>(patternTableMask()), ")\n");
		case 4:
			if (auto mask = ((v & 0x07) << 6) | 0x3f; sms1Mode4() && (mask != 0x1ff)) {
				return tmpStrCat("pattern table:  (fixed at 0x0000 in mode 4)\n"
				                 "SMS1 tile mask 2: 0x", hex_string<3>(mask), " (planes 2-3)\n");
			}
			return tmpStrCat("pattern table:  (fixed at 0x0000 in mode 4)\n");
		default:
			return tmpStrCat("pattern table:  0x", hex_string<4>(patternTableBase()), '\n');
		}
	  }},
	R{{S{5, 0xFF}}, "sprite attribute table address; on SMS1 in mode 4, bit 0 selects where the x/tile entries are",
	  [](uint32_t v) {
		bool lowXTile = sms1Mode4() && !(v & 0x01);
		return tmpStrCat("sprite attr:    0x", hex_string<4>(spriteAttBase()),
		                 lowXTile ? " (SMS1: x/tile at +0x00 instead of +0x80)"sv : ""sv, '\n');
	  }},
	// One item for both modes, so that hovering any of these bits
	// highlights the line that is actually shown.
	R{{S{6, 0x07}}, "sprite pattern table address (modes 0-3) / sprite tile bank select, bit 2 (mode 4); on SMS1 in mode 4, bits 1-0 mask the sprite tile number",
	  [](uint32_t v) {
		if (isMode4()) {
			auto bank = (v & 0x04) ? "256-511"sv : "0-255"sv;
			if (auto mask = ((v & 0x03) << 6) | 0x3f; sms1Mode4() && (mask != 0xff)) {
				return tmpStrCat("sprite tiles:   ", bank, " (SMS1 tile mask 0x", hex_string<2>(mask), ")\n");
			}
			return tmpStrCat("sprite tiles:   ", bank, '\n');
		}
		return tmpStrCat("sprite pattern: 0x", hex_string<4>(spritePatBase()), '\n');
	  }},

	// scroll registers
	R{{S{8, 0xFF}}, "horizontal scroll (mode 4)",
	  [](uint32_t v) { return tmpStrCat("h-scroll: ", v); }},
	R{{S{8, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{9, 0xFF}}, "vertical scroll (mode 4)",
	  [](uint32_t v) { return tmpStrCat("v-scroll: ", v, '\n'); }},

	// color register
	R{{S{7, 0xF0}}, "text foreground color (mode 1)",
	  [](uint32_t v) {
		if (getMode().mode != 1) return TemporaryString{};
		return tmpStrCat("text fg: color ", v, ", ");
	  }},
	R{{S{7, 0x0F}}, "backdrop color, also the text background in mode 1; in mode 4 it selects color 16-31 (the sprite palette)",
	  [](uint32_t v) {
		switch (getMode().mode) {
		case 1: return tmpStrCat("bg/backdrop: color ", v, '\n');
		case 4: return tmpStrCat("backdrop: color ", v + 16, '\n');
		default: return tmpStrCat("backdrop: color ", v, '\n');
		}
	  }},

	// line counter
	R{{S{10, 0xFF}}, "line counter reload value; the internal counter decrements on each visible line and reloads (raising HINT) when it underflows",
	  [](uint32_t v) {
		return tmpStrCat("line counter: reload=", v, " (HINT every ", v + 1, " lines)\n",
		                 "live counter: ", g_vdp->peekLineCounter(), '\n');
	  }},

	// status register
	R{{S{STATUS, 0x80}}, "V-Blank interrupt pending",
	  [](uint32_t v) { return tmpStrCat("VINT: ", v); }},
	R{{S{STATUS, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{STATUS, 0x40}}, "sprite overflow: more than 4 (modes 0-3) or 8 (mode 4) sprites on a line",
	  [](uint32_t v) { return tmpStrCat(v ? ""sv : "no "sv, "sprite overflow"); }},
	R{{S{STATUS, 0x1F}}, "fifth sprite number: the first sprite that did not fit on a line (modes 0-3); undefined in mode 4",
	  [](uint32_t v) { return tmpStrCat(" (5th sprite: ", v, ')'); }},
	R{{S{STATUS, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{STATUS, 0x20}}, "sprite collision",
	  [](uint32_t v) { return tmpStrCat(v ? ""sv : "no "sv, "sprite collision\n"); }},
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
			const auto& rd = (reg < STATUS) ? registerDescriptions[reg] : statusDescription;
			if (ImGui::TableNextColumn()) {
				auto name = (reg < STATUS) ? strCat("R#", int(reg)) : std::string("S#0");
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(name);
				simpleToolTip(rd.name);
			}
			uint8_t value = regValues[reg];
			bool writeReg = false;
			if (ImGui::TableNextColumn()) {
				if (reg < STATUS) {
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
			if (writeReg && (reg < STATUS)) {
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
						if (auto reg = func.subs[0].reg; reg < STATUS) { // non-status-register
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
		for (auto reg : xrange(STATUS)) {
			registerValues[reg] = vdp.peekReg(reg);
		}
		registerValues[STATUS] = vdp.peekControl(time);

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
			static constexpr auto statusRegs = std::to_array<uint8_t>({STATUS});
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
