#include "ImGuiVdpRegs.hh"

#include "ImGuiCpp.hh"
#include "ImGuiUtils.hh"

#include "VDP.hh"

#include "strCat.hh"
#include "xrange.hh"

#include "imgui.h"
#include "imgui_internal.h"

#include <array>
#include <bit>

namespace openmsx {

ImGuiVdpRegs::ImGuiVdpRegs(ImGuiManager&)
	//: manager(manager_)
{
}

void ImGuiVdpRegs::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiVdpRegs::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

static const char* modeName(uint32_t v)
{
	// NOTE: bits M1 and M2 are swapped!!
	static constexpr std::array<const char*, 32> modeNames = {
		"Graphic 1 (screen 1)", // 0
		"Multicolor (screen 3)", // 2
		"Text 1 (screen 0, width 40)", // 1
		"3 - invalid", // 3

		"Graphic 2 (screen 2)", // 4
		"Multicolor-Q", // 6
		"Text 1-Q", // 5
		"7 - invalid", // 7

		"Graphic 3 (screen 4)", // 8
		"10 - invalid", // 10
		"Text 2 (screen 0, width 80)", // 9
		"11 - invalid", // 11

		"Graphic 4 (screen 5)", // 12
		"14 - invalid", // 14
		"13 - invalid", // 13
		"15 - invalid", // 15

		"Graphic 5 (screen 6)", // 16
		"18 - invalid", // 18
		"17 - invalid", // 17
		"19 - invalid", // 19

		"Graphic 6 (screen 7)", // 20
		"22 - invalid", // 22
		"21 - invalid", // 21
		"23 - invalid", // 23

		"24 - invalid", // 24
		"26 - invalid", // 26
		"25 - invalid", // 25
		"27 - invalid", // 27

		"Graphic 7 (screen 8)", // 28
		"30 - invalid", // 30
		"29 - invalid", // 29
		"31 - invalid", // 31
	};
	assert(v < 32);
	return modeNames[v];
}

using Bits = std::array<const char*, 8>;
struct RegisterDescription {
	Bits bits;
	const char* name;
};
using RD = RegisterDescription;
static constexpr auto registerDescriptions = std::array{
	RD{{" 0 ", "DG ", "IE2", "IE1", "M5 ", "M4 ", "M3 ", " 0 "}, "Mode register 0"}, // R#0
	RD{{" 0 ", "BL ", "IE0", "M1 ", "M2 ", " 0 ", "SI ", "MAG"}, "Mode register 1"}, // R#1
	RD{{" 0 ", "A16", "A15", "A14", "A13", "A12", "A11", "A10"}, "Pattern name table base address register"}, // R#2
	RD{{"A13", "A12", "A11", "A10", "A9 ", "A8 ", "A7 ", "A6 "}, "Color table base address register low"}, // R#3
	RD{{" 0 ", " 0 ", "A16", "A15", "A14", "A13", "A12", "A11"}, "Pattern generator table base address register"}, // R#4
	RD{{"A14", "A13", "A12", "A11", "A10", "A9 ", "A8 ", "A7 "}, "Sprite attribute table base address register low"}, // R#5
	RD{{" 0 ", " 0 ", "A16", "A15", "A14", "A13", "A12", "A11"}, "Sprite pattern generator table base address register"}, // R#6
	RD{{"TC3", "TC2", "TC1", "TC0", "BD3", "BD2", "BD1", "BD0"}, "Text color/Back drop color register"}, // R#7

	RD{{"MS ", "LP ", "TP ", "CB ", "VR ", " 0 ", "SPD", "BW "}, "Mode register 2"}, // R#8
	RD{{"LN ", " 0 ", "S1 ", "S2 ", "IL ", "EO ", "*NT", "DC "}, "Mode register 3"}, // R#9
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "A16", "A15", "A14"}, "Color table base address register high"}, // R#10
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "A16", "A15"}, "Sprite attribute table base address register high"}, // R#11
	RD{{"T23", "T22", "T21", "T20", "BC3", "BC2", "BC1", "BC0"}, "Text color/Back color register"}, // R#12
	RD{{"ON3", "ON2", "ON1", "ON0", "OF3", "OF2", "OF1", "OF0"}, "Blinking period register"}, // R#13
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "A16", "A15", "A14"}, "VRAM Access base address register"}, // R#14
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "S3 ", "S2 ", "S1 ", "S0 "}, "Status register pointer"}, // R#15

	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "C3 ", "C2 ", "C1 ", "C0 "}, "Color palette address register"}, // R#16
	RD{{"AII", " 0 ", "RS5", "RS4", "RS3", "RS2", "RS1", "RS0"}, "Control register pointer"}, // R#17
	RD{{"V3 ", "V2 ", "V1 ", "V0 ", "H3 ", "H2 ", "H1 ", "H0 "}, "Display adjust register"}, // R#18
	RD{{"IL7", "IL6", "IL5", "IL4", "IL3", "IL2", "IL1", "IL0"}, "Interrupt line register"}, // R#19
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 "}, "Color burst register 1"}, // R#20
	RD{{" 0 ", " 0 ", " 1 ", " 1 ", " 1 ", " 0 ", " 1 ", " 1 "}, "Color burst register 2"}, // R#21
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 1 ", " 0 ", " 1 "}, "Color burst register 3"}, // R#22
	RD{{"DO7", "DO6", "DO5", "DO4", "DO3", "DO2", "DO1", "DO0"}, "Display offset register"}, // R#23

	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#24
	RD{{" 0 ", "CMD", "VDS", "YAE", "YJK", "WTE", "MSK", "SP2"}, "V9958 control register"}, // R#25
	RD{{" 0 ", " 0 ", "HO8", "HO7", "HO6", "HO5", "HO4", "HO3"}, "Horizontal scroll register high"}, // R#26
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "HO2", "HO1", "HO0"}, "Horizontal scroll register low"}, // R#27
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#28
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#29
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#30
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#31

	RD{{"SX7", "SX6", "SX5", "SX4", "SX3", "SX2", "SX1", "SX0"}, "Source X low register"}, // R#32
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "SX8"}, "Source X high register"}, // R#33
	RD{{"SY7", "SY6", "SY5", "SY4", "SY3", "SY2", "SY1", "SY0"}, "Source Y low register"}, // R#34
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "SY9", "SY8"}, "Source Y high register"}, // R#35
	RD{{"DX7", "DX6", "DX5", "DX4", "DX3", "DX2", "DX1", "DX0"}, "Destination X low register"}, // R#36
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "DX8"}, "Destination X high register"}, // R#37
	RD{{"DY7", "DY6", "DY5", "DY4", "DY3", "DY2", "DY1", "DY0"}, "Destination Y low register"}, // R#38
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "DY9", "DY8"}, "Destination Y high register"}, // R#39

	RD{{"NX7", "NX6", "NX5", "NX4", "NX3", "NX2", "NX1", "NX0"}, "Number of dots X low register"}, // R#40
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "NX8"}, "Number of dots X high register"}, // R#41
	RD{{"NY7", "NY6", "NY5", "NY4", "NY3", "NY2", "NY1", "NY0"}, "Number of dots Y low register"}, // R#42
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "NY9", "NY8"}, "Number of dots Y high register"}, // R#43
	RD{{"CH3", "CH2", "CH1", "CH0", "CL3", "CL2", "CL1", "CL0"}, "Color register"}, // R#44
	RD{{" 0 ", "MXC", "MXD", "MXS", "DIY", "DIX", "EQ ", "MAJ"}, "Argument register"}, // R#45
	RD{{"CM3", "CM2", "CM1", "CM0", "LO3", "LO2", "LO1", "LO0"}, "Command register"}, // R#46
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#47

	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#48
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#49
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#50
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#51
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#52
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#53
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#54
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#55

	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#56
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#57
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#58
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#59
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#60
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#61
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#62
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, ""}, // R#63

	// We put info on the status registers in the same table as the normal VDP registers,
	// (as-if they were registers R#64-R73, but that's nonsense of course)
	RD{{" F ", "5S ", " C ", "FS4", "FS3", "FS2", "FS1", "FS0"}, "Status register 0"}, // S#0
	RD{{"FL ", "LPS", "ID4", "ID3", "ID2", "ID1", "ID0", "FH "}, "Status register 1"}, // S#1
	RD{{"TR ", "VR ", "HR ", "BD ", " 1 ", " 1 ", "EO ", "CE "}, "Status register 2"}, // S#2
	RD{{"X7 ", "X6 ", "X5 ", "X4 ", "X3 ", "X2 ", "X1 ", "X0 "}, "Column register low"}, // S#3
	RD{{" 1 ", " 1 ", " 1 ", " 1 ", " 1 ", " 1 ", " 1 ", "X8 "}, "Column register high"}, // S#4
	RD{{"Y7 ", "Y6 ", "Y5 ", "Y4 ", "Y3 ", "Y2 ", "Y1 ", "Y0 "}, "Row register low"}, // S#5
	RD{{" 1 ", " 1 ", " 1 ", " 1 ", " 1 ", " 1 ", "EO ", "Y8 "}, "Row register high"}, // S#6
	RD{{"C7 ", "C6 ", "C5 ", "C4 ", "C3 ", "C2 ", "C1 ", "C0 "}, "Color register"}, // S#7

	RD{{"BX7", "BX6", "BX5", "BX4", "BX3", "BX2", "BX1", "BX0"}, "Border X register low"}, // S#8
	RD{{" 1 ", " 1 ", " 1 ", " 1 ", " 1 ", " 1 ", " 1 ", "BX8"}, "Border X register high"}, // S#9
};

struct SubFunction {
	uint8_t reg = uint8_t(-1);
	uint8_t mask = 0;
};
struct RegFunction {
	// drawSection() draws this item when 'subs[0].reg' or 'subs[1].reg' is part of 'showRegisters'
	// when 'subs[0].mask == 0' the item cannot be highlighted (typically such items show a fixed string)
	std::array<SubFunction, 2> subs;
	std::string_view toolTip;
	std::string(*displayFunc)(uint32_t);
};
using S = SubFunction;
using R = RegFunction;
using namespace std::literals;
static std::string noExplanation(uint32_t) { return {}; }
static std::string spacing(uint32_t) { return "\n"s; }

static std::array<uint8_t, 64 + 10> registerValues; // HACK: global!!
static uint8_t getMode() { return ((registerValues[1] & 0x18) >> 3) | ((registerValues[0] & 0x0E) << 1); }
static bool isText2Mode() { return getMode() == 0b01010; } // Note: M1 and M2 swapped!
static bool isGraph23Mode() { return getMode() == one_of(0b00100, 0b01000); }
static bool isSprite2Mode() { return getMode() == one_of(0b01000, 0b01100, 0b10000, 0b10100, 0b11100); }
static bool isBitmapMode() { return getMode() >= 0x0C; }
static bool isPlanarMode() { return (getMode() & 0x14) == 0x14; }

static constexpr auto regFunctions = std::array{
	// mode registers
	R{{S{0, 0}}, "", [](uint32_t) { return "screen: "s; }},
	R{{S{1, 0x40}}, "screen display enable/disable",
	  [](uint32_t v) { return std::string(v ? "enabled" : "disabled"); }},
	R{{S{0, 0}}, "", [](uint32_t) { return ", "s; }},
	R{{S{1, 0x18}, {0, 0x0E}}, "display mode",
	  [](uint32_t v) { return std::string(modeName(v)); }},
	R{{S{9, 0}}, "", [](uint32_t) { return ", "s; }},
	R{{S{9, 0x02}}, "select PAL or NTSC",
	  [](uint32_t v) { return std::string(v ? "PAL (50Hz)" : "NTSC (60Hz)"); }},
	R{{S{0, 0}}, "", [](uint32_t) { return "\n"s; }},

	R{{S{9, 0x80}}, "select 192 or 212 lines",
	  [](uint32_t v) { return std::string(v ? "212 lines" : "192 lines"); }},
	R{{S{9, 0}}, "", [](uint32_t) { return ", "s; }},
	R{{S{9, 0x08}}, "interlace enable/disable",
	  [](uint32_t v) { return std::string(v ? "interlaced" : "non-interlaced"); }},
	R{{S{9, 0}}, "", [](uint32_t) { return ", "s; }},
	R{{S{9, 0x04}}, "alternate odd/even pages enable/disable",
	  [](uint32_t v) { return strCat(v ? "" : "do not", " alternate odd/even pages\n"); }},

	R{{S{1, 0}}, "", [](uint32_t) { return "sprites: "s; }},
	R{{S{8, 0x02}}, "sprite enable/disable",
	  [](uint32_t v) { return std::string(v ? "disabled" : "enabled"); }},
	R{{S{8, 0}}, "", [](uint32_t) { return ", "s; }},
	R{{S{1, 0x02}}, "sprite size, 8x8 or 16x16",
	  [](uint32_t v) { return strCat("size=", v ? "16x16" : "8x8"); }},
	R{{S{1, 0}}, "", [](uint32_t) { return ", "s; }},
	R{{S{1, 0x01}}, "sprite magnification enable/disable",
	  [](uint32_t v) { return std::string(v ? "magnified\n" : "not-magnified\n"); }},

	R{{S{8, 0x20}}, "color 0 transparent or not",
	  [](uint32_t v) { return strCat("color 0 is ", v ? "a regular color\n" : "transparent\n"); }},

	R{{S{1, 0x20}}, "V-Blank interrupt enable/disable",
	  [](uint32_t v) { return strCat("V-Blank-IRQ: ", v ? "enabled" : "disabled"); }},
	R{{S{1, 0}}, "", [](uint32_t) { return ", "s; }},
	R{{S{0, 0x10}}, "line interrupt enable/disable",
	  [](uint32_t v) { return strCat("line-IRQ: ", v ? "enabled" : "disabled", '\n'); }},

	R{{S{0, 0x40}}, "digitize", &noExplanation},
	R{{S{0, 0x20}}, "light pen interrupt enable", &noExplanation},
	R{{S{1, 0x04}}, "fast blink mode", &noExplanation},
	R{{S{8, 0x80}}, "mouse", &noExplanation},
	R{{S{8, 0x40}}, "light pen", &noExplanation},
	R{{S{8, 0x10}}, "set color bus input/output", &noExplanation},
	R{{S{8, 0x08}}, "select VRAM type", &noExplanation},
	R{{S{8, 0x01}}, "select black&white output", &noExplanation},
	R{{S{9, 0x30}}, "simultaneous mode", &noExplanation},
	R{{S{9, 0x01}}, "set DLCLK input/output", &noExplanation},
	R{{S{0, 0}}, "", &spacing},

	// Table base registers
	R{{S{2, 0x7F}}, "name table address", [](uint32_t v) {
		if (isText2Mode()) {
			v &= ~0x03;
		} else if (isBitmapMode()) {
			v &= ~0x1F;
			if (isPlanarMode()) {
				v *= 2;
			}
		}
		return strCat("name table:    0x", hex_string<5>(v << 10), '\n');
	  }},
	R{{S{3, 0xFF}, S{10, 0x07}}, "color table address", [](uint32_t v) {
		if (isText2Mode()) v &= ~0x07;
		if (isGraph23Mode()) v &= ~0x7F;
		return strCat("color table:   0x", hex_string<5>(v << 6), '\n');
	  }},
	R{{S{4, 0x3F}}, "pattern table address", [](uint32_t v) {
		if (isGraph23Mode()) v &= ~0x03;
		return strCat("pattern table: 0x", hex_string<5>(v << 11), '\n');
	  }},
	R{{S{5, 0xFF}, S{11, 0x03}}, "sprite attribute table address", [](uint32_t v) {
		if (isSprite2Mode()) v &= ~0x07;
		return strCat("sprite attribute table: 0x", hex_string<5>(v << 7), '\n');
	  }},
	R{{S{6, 0x3F}}, "sprite pattern table address", [](uint32_t v) {
		return strCat("sprite pattern table:   0x", hex_string<5>(v << 11), '\n');
	  }},
	R{{S{2, 0}}, "", &spacing},

	// Color registers
	R{{S{13, 0}}, "", [](uint32_t) { return "text color:"s; }},
	R{{S{7, 0xF0}}, "text color",
	  [](uint32_t v) { return strCat(" foreground=", v); }},
	R{{S{7, 0x0F}}, "background color",
	  [](uint32_t v) { return strCat(" background=", v, '\n'); }},
	R{{S{13, 0}}, "", [](uint32_t) { return "text blink color:"s; }},
	R{{S{12, 0xF0}}, "text blink color",
	  [](uint32_t v) { return strCat(" foreground=", v); }},
	R{{S{12, 0x0F}}, "background blink color",
	  [](uint32_t v) { return strCat(" background=", v, '\n'); }},
	R{{S{13, 0}}, "", [](uint32_t) { return "blink period:"s; }},
	R{{S{13, 0xF0}}, "blink period on",
	  [](uint32_t v) { return strCat(" on=", 10 * v); }},
	R{{S{13, 0x0F}}, "blink period off",
	  [](uint32_t v) { return strCat(" off=", 10 * v); }},
	R{{S{13, 0}}, "", [](uint32_t) { return " frames\n"s; }},
	R{{S{7, 0}}, "", &spacing},

	// Display registers
	R{{S{18, 0}}, "", [](uint32_t) { return "set-adjust:"s; }},
	R{{S{18, 0x0F}}, "horizontal set-adjust",
	  [](uint32_t v) { return strCat(" horizontal=", int(v ^ 7) - 7); }},
	R{{S{18, 0xF0}}, "vertical set-adjust",
	  [](uint32_t v) { return strCat(" vertical=", int(v ^ 7) - 7, '\n'); }},
	R{{S{19, 0xFF}}, "line number for line-interrupt",
	  [](uint32_t v) { return strCat("line interrupt=", v, '\n'); }},
	R{{S{23, 0xFF}}, "vertical scroll (line number for 1st line)",
	  [](uint32_t v) { return strCat("vertical scroll=", v, '\n'); }},
	R{{S{18, 0}}, "", &spacing},

	// Access registers
	R{{S{14, 0x07}}, "VRAM access base address",
	  [](uint32_t v) { return strCat("VRAM access base address: ", hex_string<5>(v << 14), '\n'); }},
	R{{S{15, 0x0F}}, "select status register",
	  [](uint32_t v) { return strCat("selected status register: ", v, '\n'); }},
	R{{S{16, 0x0F}}, "select palette entry",
	  [](uint32_t v) { return strCat("selected palette entry: ", v, '\n'); }},
	R{{S{17, 0x3F}}, "indirect register access",
	  [](uint32_t v) { return strCat("selected indirect register: ", v); }},
	R{{S{17, 0}}, "", [](uint32_t) { return ", "s; }},
	R{{S{17, 0x80}}, "auto-increment",
	  [](uint32_t v) { return strCat(v ? "no " : "", "auto-increment\n"); }},
	R{{S{14, 0}}, "", &spacing},

	// V9958 registers
	R{{S{27, 0x07}, S{26, 0x3F}}, "horizontal scroll", [](uint32_t v) {
		auto s = (v & ~7) - (v & 7);
		return strCat("horizontal scroll: ", s);
	}},
	R{{S{25, 0}}, "", [](uint32_t) { return ", "s; }},
	R{{S{25, 0x01}}, "2-page scroll enable/disable",
	  [](uint32_t v) { return strCat(v + 1, "-page"); }},
	R{{S{25, 0}}, "", [](uint32_t) { return ", "s; }},
	R{{S{25, 0x02}}, "mask 8 left dots",
	  [](uint32_t v) { return strCat(v ? "" : "not-", "masked\n"); }},
	R{{S{25, 0x18}}, "YJK/YAE mode",
	  [](uint32_t v) { return strCat((v & 1) ? ((v & 2) ? "YJK+YAE" : "YJK") : "RGB", "-mode\n"); }},
	R{{S{25, 0x40}}, "expand command mode",
	  [](uint32_t v) { return strCat("commands ", (v ? "work in all " : "only work in bitmap-"), "modes\n"); }},
	R{{S{25, 0x20}}, "output select between CPUCLK and /VDS", &noExplanation},
	R{{S{25, 0x04}}, "enable/disable WAIT (not used on MSX)", &noExplanation},
	R{{S{25, 0}}, "", &spacing},

	// Command registers
	R{{S{46, 0xF0}}, "command code", [](uint32_t v) {
		static constexpr std::array<std::string_view, 16> COMMANDS = {
			" ABRT"," ????"," ????"," ????","POINT"," PSET"," SRCH"," LINE",
			" LMMV"," LMMM"," LMCM"," LMMC"," HMMV"," HMMM"," YMMM"," HMMC"
		};
		return std::string(COMMANDS[v]);
	}},
	R{{S{46, 0}}, "", [](uint32_t) { return "-"s; }},
	R{{S{46, 0x0F}}, "command code", [](uint32_t v) {
		static constexpr std::array<std::string_view, 16> OPS = {
			"IMP ","AND ","OR  ","XOR ","NOT ","NOP ","NOP ","NOP ",
			"TIMP","TAND","TOR ","TXOR","TNOT","NOP ","NOP ","NOP "
		};
		return std::string(OPS[v]);
	}},
	R{{S{32, 0}}, "", [](uint32_t) { return "("s; }},
	R{{S{32, 0xFF}, S{33, 0x01}}, "source X", [](uint32_t v) { return strCat(v); }},
	R{{S{32, 0}}, "", [](uint32_t) { return ","s; }},
	R{{S{34, 0xFF}, S{35, 0x03}}, "source Y", [](uint32_t v) { return strCat(v); }},
	R{{S{32, 0}}, "", [](uint32_t) { return ")->("s; }},
	R{{S{36, 0xFF}, S{37, 0x01}}, "destination X", [](uint32_t v) { return strCat(v); }},
	R{{S{32, 0}}, "", [](uint32_t) { return ","s; }},
	R{{S{38, 0xFF}, S{39, 0x03}}, "destination Y", [](uint32_t v) { return strCat(v); }},
	R{{S{32, 0}}, "", [](uint32_t) { return "),"s; }},
	R{{S{44, 0xFF}}, "color", [](uint32_t v) { return strCat(v); }},
	R{{S{32, 0}}, "", [](uint32_t) { return " ["s; }},
	R{{S{45, 0x04}}, "direction X", [](uint32_t v) { return std::string(v ? "-" : "+"); }},
	R{{S{40, 0xFF}, S{41, 0x01}}, "number of dots X", [](uint32_t v) { return strCat(v); }},
	R{{S{32, 0}}, "", [](uint32_t) { return ","s; }},
	R{{S{45, 0x08}}, "direction Y", [](uint32_t v) { return std::string(v ? "-" : "+"); }},
	R{{S{42, 0xFF}, S{43, 0x03}}, "number of dots Y", [](uint32_t v) { return strCat(v); }},
	R{{S{32, 0}}, "", [](uint32_t) { return "]\n"s; }},

	R{{S{45, 0x40}}, "expanded VRAM for CPU access", &noExplanation},
	R{{S{45, 0x20}}, "expanded VRAM for destination", &noExplanation},
	R{{S{45, 0x10}}, "expanded VRAM for source", &noExplanation},
	R{{S{45, 0x02}}, "select equal/not-equal for SRCH command", &noExplanation},
	R{{S{45, 0x01}}, "select major direction (X/Y) for LINE command", &noExplanation},
	R{{S{32, 0}}, "", &spacing},

	// Status registers
	R{{S{64, 0x80}}, "VBLANK interrupt pending",
	  [](uint32_t v) { return strCat("VBLANK-IRQ: ", v); }},
	R{{S{65, 0}}, "", [](uint32_t) { return " "s; }},
	R{{S{65, 0x01}}, "line interrupt pending",
	  [](uint32_t v) { return strCat("line-IRQ: ", v); }},
	R{{S{64, 0}}, "", [](uint32_t) { return "\n"s; }},

	R{{S{64, 0x40}}, "5th/9th sprite detected",
	  [](uint32_t v) { return strCat(v ? "" : "no ", "5th/9th sprite per line"); }},
	R{{S{64, 0x1F}}, "5th/9th sprite number",
	  [](uint32_t v) { return strCat(" (sprite number=", v, ")\n"); }},

	R{{S{64, 0x20}}, "sprite collision",
	  [](uint32_t v) { return strCat(v ? "" : "no ", "sprite collision"); }},
	R{{S{67, 0}}, "", [](uint32_t) { return " (at coordinate "s; }},
	R{{S{67, 0xFF}, S{68, 0x01}}, "x-coordinate",
	  [](uint32_t v) { return strCat("x=", v); }},
	R{{S{69, 0xFF}, S{70, 0x03}}, "y-coordinate",
	  [](uint32_t v) { return strCat("y=", v); }},
	R{{S{67, 0}}, "", [](uint32_t) { return ")"s; }},
	R{{S{64, 0}}, "", [](uint32_t) { return "\n"s; }},

	R{{S{66, 0}}, "", [](uint32_t) { return "command: "s; }},
	R{{S{66, 0x01}}, "command executing",
	  [](uint32_t v) { return strCat(v ? "" : "not ", "running"); }},
	R{{S{66, 0}}, "", [](uint32_t) { return " "s; }},
	R{{S{66, 0x80}}, "transfer ready flag",
	  [](uint32_t v) { return strCat(v ? "" : "not ", "ready to transfer\n"); }},

	R{{S{66, 0}}, "", [](uint32_t) { return "scan position: "s; }},
	R{{S{66, 0x20}}, "horizontal border",
	  [](uint32_t v) { return strCat(v ? "" : "not ", "in horizontal border"); }},
	R{{S{66, 0}}, "", [](uint32_t) { return " "s; }},
	R{{S{66, 0x40}}, "vertical border",
	  [](uint32_t v) { return strCat(v ? "" : "not ", "in vertical border\n"); }},

	R{{S{65, 0x80}}, "light pen (not used on MSX)", &noExplanation},
	R{{S{65, 0x40}}, "light pen switch (not used on MSX)", &noExplanation},
	R{{S{65, 0x3E}}, "VDP identification (V9938 or V9958)", &noExplanation},
	R{{S{66, 0x10}}, "SRCH command detected border color", &noExplanation},
	R{{S{66, 0x02}}, "odd/even field", &noExplanation},
	R{{S{71, 0xFF}}, "result of POINT command", &noExplanation},
	R{{S{72, 0xFF}, S{73, 0x01}}, "result of SRCH command", &noExplanation},
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

void ImGuiVdpRegs::drawSection(std::span<const uint8_t> showRegisters, std::span<const uint8_t> regValues,
                               VDP& vdp, EmuTime::param time)
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
		auto isCellHovered = [&]() {
			return ImGui::TableGetCellBgRect(table, table->CurrentColumn).Contains(mouse_pos);
		};

		im::ID_for_range(showRegisters.size(), [&](int i) {
			auto reg = showRegisters[i];
			// note:  0..63  regular register
			//       64..73  status register
			const auto& rd = registerDescriptions[reg];
			if (ImGui::TableNextColumn()) {
				auto name = tmpStrCat(reg < 64 ? "R#" : "S#", reg < 64 ? reg : reg - 64);
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
							value ^=  mask;
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

void ImGuiVdpRegs::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	ImGui::SetNextWindowSize(gl::vec2{41, 29} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("VDP registers", &show, [&]{
		VDP* vdp = dynamic_cast<VDP*>(motherBoard->findDevice("VDP")); // TODO name based OK?
		if (!vdp) return;
		auto time = motherBoard->getCurrentTime();
		const bool tms99x8 = vdp->isMSX1VDP();
		const bool v9958 = vdp->hasYJK();

		for (auto reg : xrange(64)) {
			registerValues[reg] = vdp->peekRegister(reg);
		}
		for (auto reg : xrange(uint8_t(10))) {
			registerValues[reg + 64] = vdp->peekStatusReg(reg, time);
		}

		newHoveredFunction = -1;
		auto make_span = [](const auto& array) {
			return std::span<const uint8_t, std::dynamic_extent>(std::span(array));
		};
		im::TreeNode("Control registers", &openControl, [&]{
			im::TreeNode("Mode registers", &openMode, [&]{
				static constexpr auto modeRegs1 = std::to_array<uint8_t>({0, 1});
				static constexpr auto modeRegs2 = std::to_array<uint8_t>({0, 1, 8, 9});
				auto modeRegs = tms99x8 ? make_span(modeRegs1) : make_span(modeRegs2);
				drawSection(modeRegs, registerValues, *vdp, time);
			});
			im::TreeNode("Table base registers", &openBase, [&]{
				static constexpr auto tableRegs1 = std::to_array<uint8_t>({2, 3, 4, 5, 6});
				static constexpr auto tableRegs2 = std::to_array<uint8_t>({2, 3, 10, 4, 5, 11, 6});
				auto tableRegs = tms99x8 ? make_span(tableRegs1) : make_span(tableRegs2);
				drawSection(tableRegs, registerValues, *vdp, time);
			});
			im::TreeNode("Color registers", &openColor, [&]{
				static constexpr auto colorRegs1 = std::to_array<uint8_t>({7});
				static constexpr auto colorRegs2 = std::to_array<uint8_t>({7, 12, 13, 20, 21, 22});
				auto colorRegs = tms99x8 ? make_span(colorRegs1) : make_span(colorRegs2);
				drawSection(colorRegs, registerValues, *vdp, time);
			});
			if (!tms99x8) {
				im::TreeNode("Display registers", &openDisplay, [&]{
					static constexpr auto displayRegs = std::to_array<uint8_t>({18, 19, 23});
					drawSection(displayRegs, registerValues, *vdp, time);
				});
				im::TreeNode("Access registers", &openAccess, [&]{
					static constexpr auto accessRegs = std::to_array<uint8_t>({14, 15, 16, 17});
					drawSection(accessRegs, registerValues, *vdp, time);
				});
			}
			if (v9958) {
				im::TreeNode("V9958 registers", &openV9958, [&]{
					static constexpr auto v9958Regs = std::to_array<uint8_t>({25, 26, 27});
					drawSection(v9958Regs, registerValues, *vdp, time);
				});
			}
		});
		if (!tms99x8) {
			im::TreeNode("Command registers", &openCommand, [&]{
				static constexpr auto cmdRegs = std::to_array<uint8_t>({
					32, 33, 34, 35,  36, 37, 38, 39,  40, 41, 42, 43,  44, 45, 46
				});
				drawSection(cmdRegs, registerValues, *vdp, time);
			});
		}
		im::TreeNode("Status registers", &openStatus, [&]{
			static constexpr auto statusRegs1 = std::to_array<uint8_t>({64});
			static constexpr auto statusRegs2 = std::to_array<uint8_t>({
				64, 65, 66, 67, 68, 69, 70, 71, 72, 73
			});
			auto statusRegs = tms99x8 ? make_span(statusRegs1) : make_span(statusRegs2);
			drawSection(statusRegs, registerValues, *vdp, time);
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
