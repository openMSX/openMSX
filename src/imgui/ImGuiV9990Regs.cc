#include "ImGuiV9990Regs.hh"

#include "ImGuiCpp.hh"
#include "ImGuiUtils.hh"
#include "VDPRegisterViewer.hh"

#include "MSXMotherBoard.hh"
#include "V9990.hh"

#include "strCat.hh"
#include "xrange.hh"
#include "zstring_view.hh"

#include "imgui.h"

#include <array>
#include <string>

namespace openmsx {

ImGuiV9990Regs::ImGuiV9990Regs(ImGuiManager& manager_, size_t index)
	: ImGuiPart(manager_)
	, title(strCat("V9990 registers", strCat_if(index, " (", index + 1, ')')))
{
}

void ImGuiV9990Regs::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiV9990Regs::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

// R#0-R#54 are the control registers; they are followed by the status port
// P#5 and the interrupt flag port P#6 (shown as S#0 and S#1).
static constexpr uint8_t STATUS = 55;
static constexpr uint8_t INT_FLAG = 56;

// The registers of the V9990 (see VDPRegisterViewer.hh).
struct V9990Registers {
	V9990Registers(V9990& vdp_, EmuTime time_)
		: vdp(vdp_), time(time_)
	{
		for (auto reg : xrange(uint8_t(32))) {
			values[reg] = vdp.peekRegister(reg);
		}
		for (auto reg : xrange(uint8_t(32), STATUS)) {
			values[reg] = vdp.peekCmdRegister(reg, time);
		}
		values[STATUS] = vdp.peekStatus(time);
		values[INT_FLAG] = vdp.peekInterruptFlag();
	}
	void write(uint8_t reg, uint8_t value) const
	{
		// Read-only and unused registers can be edited in the table, but
		// writing them would only trigger the invalid register write callback.
		if (V9990::isRegisterWritable(reg)) vdp.changeRegister(reg, value, time);
	}

	[[nodiscard]] bool isP1() const { return (values[6] & 0xC0) == 0x00; }
	[[nodiscard]] bool isP2() const { return (values[6] & 0xC0) == 0x40; }

	V9990& vdp;
	EmuTime time;
	std::array<uint8_t, INT_FLAG + 1> values; // R#0-R#54, followed by P#5 and P#6
};

// Bit names, b7 first. Taken from the V9990 manual:
//   http://msxbanzai.tni.nl/v9990/manual.html  ("V9990 REGISTER SPECIFICATIONS")
// Multi-bit fields repeat the field name; '0'/'1' are fixed bits, '-' is unused.
using RD = RegisterDescription;
static constexpr auto registerDescriptions = std::array{
	RD{{"WA7", "WA6", "WA5", "WA4", "WA3", "WA2", "WA1", "WA0"}, "VRAM write address (bit 7-0)"},   // R#0
	RD{{"WAf", "WAe", "WAd", "WAc", "WAb", "WAa", "WA9", "WA8"}, "VRAM write address (bit 15-8)"},  // R#1
	RD{{"AII", " 0 ", " 0 ", " 0 ", " 0 ", "WAi", "WAh", "WAg"}, "VRAM write address (bit 18-16) + address increment inhibit"}, // R#2
	RD{{"RA7", "RA6", "RA5", "RA4", "RA3", "RA2", "RA1", "RA0"}, "VRAM read address (bit 7-0)"},    // R#3
	RD{{"RAf", "RAe", "RAd", "RAc", "RAb", "RAa", "RA9", "RA8"}, "VRAM read address (bit 15-8)"},   // R#4
	RD{{"AII", " 0 ", " 0 ", " 0 ", " 0 ", "RAi", "RAh", "RAg"}, "VRAM read address (bit 18-16) + address increment inhibit"}, // R#5

	RD{{"DSP", "DSP", "DCK", "DCK", "XIM", "XIM", "CLR", "CLR"}, "Screen mode 0: display mode, pixel clock, image width, bits/pixel"}, // R#6
	RD{{" 0 ", "C25", "SM1", "SM ", "PAL", "EO ", "IL ", "HSC"}, "Screen mode 1: timing, interlace, high scan"}, // R#7
	RD{{"DSP", "SPD", "YSE", "VWT", "VWM", " 0 ", " 1 ", " 0 "}, "Control: display/sprite enable, YS output, digitize"}, // R#8

	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "IEC", "IEH", "IEV"}, "Interrupt enable"},               // R#9
	RD{{"IL7", "IL6", "IL5", "IL4", "IL3", "IL2", "IL1", "IL0"}, "Interrupt line (bit 7-0)"},       // R#10
	RD{{"IHM", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "IL9", "IL8"}, "Interrupt line (bit 9-8) + every-line mode"}, // R#11
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "IX3", "IX2", "IX1", "IX0"}, "Interrupt X position"},           // R#12

	RD{{"PLM", "PLM", "YAE", "AIH", "PO5", "PO4", "PO3", "PO2"}, "Palette control: palette mode, YAE, palette offset"}, // R#13
	RD{{"PA5", "PA4", "PA3", "PA2", "PA1", "PA0", "PP1", "PP0"}, "Palette pointer: palette number + R/G/B selector"},   // R#14
	RD{{" 0 ", " 0 ", "BD5", "BD4", "BD3", "BD2", "BD1", "BD0"}, "Back drop color"},                // R#15
	RD{{"AV3", "AV2", "AV1", "AV0", "AH3", "AH2", "AH1", "AH0"}, "Display adjust (4-bit signed vertical/horizontal)"},  // R#16

	RD{{"AY7", "AY6", "AY5", "AY4", "AY3", "AY2", "AY1", "AY0"}, "Scroll A: Y (bit 7-0)"},          // R#17
	RD{{"R51", "R25", " 0 ", "AYc", "AYb", "AYa", "AY9", "AY8"}, "Scroll A: Y (bit 12-8) + roll page size"}, // R#18
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "AX2", "AX1", "AX0"}, "Scroll A: X (bit 2-0)"},          // R#19
	RD{{"AXa", "AX9", "AX8", "AX7", "AX6", "AX5", "AX4", "AX3"}, "Scroll A: X (bit 10-3)"},         // R#20
	RD{{"BY7", "BY6", "BY5", "BY4", "BY3", "BY2", "BY1", "BY0"}, "Scroll B: Y (bit 7-0)"},          // R#21
	RD{{"SDA", "SDB", " 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "BY8"}, "Scroll B: Y (bit 8) + layer disable (undocumented)"}, // R#22
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "BX2", "BX1", "BX0"}, "Scroll B: X (bit 2-0)"},          // R#23
	RD{{" 0 ", " 0 ", "BX8", "BX7", "BX6", "BX5", "BX4", "BX3"}, "Scroll B: X (bit 8-3)"},          // R#24

	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "G18", "G17", "G16", "G15"}, "Sprite/cursor generator base address (P1: bits 17-15 in b3-1)"}, // R#25
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, "LCD control (keep 0)"},           // R#26
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "PY1", "PY0", "PX1", "PX0"}, "Priority control (P1): layer A/B split point"},      // R#27
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "CO5", "CO4", "CO3", "CO2"}, "Cursor/sprite palette offset"},   // R#28

	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, "not used"},                       // R#29
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, "not used"},                       // R#30
	RD{{" - ", " - ", " - ", " - ", " - ", " - ", " - ", " - "}, "not used"},                       // R#31

	RD{{"SX7", "SX6", "SX5", "SX4", "SX3", "SX2", "SX1", "SX0"}, "Command source X / linear address (bit 7-0)"},   // R#32
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "SXa", "SX9", "SX8"}, "Command source X (bit 10-8)"},    // R#33
	RD{{"SY7", "SY6", "SY5", "SY4", "SY3", "SY2", "SY1", "SY0"}, "Command source Y / linear address (bit 15-8)"},  // R#34
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "SYb", "SYa", "SY9", "SY8"}, "Command source Y (bit 11-8) / linear address (bit 18-16)"}, // R#35
	RD{{"DX7", "DX6", "DX5", "DX4", "DX3", "DX2", "DX1", "DX0"}, "Command destination X / linear address (bit 7-0)"},   // R#36
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "DXa", "DX9", "DX8"}, "Command destination X (bit 10-8)"},    // R#37
	RD{{"DY7", "DY6", "DY5", "DY4", "DY3", "DY2", "DY1", "DY0"}, "Command destination Y / linear address (bit 15-8)"},  // R#38
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "DYb", "DYa", "DY9", "DY8"}, "Command destination Y (bit 11-8) / linear address (bit 18-16)"}, // R#39
	RD{{"NX7", "NX6", "NX5", "NX4", "NX3", "NX2", "NX1", "NX0"}, "Command number X / bytes / line major (bit 7-0)"},    // R#40
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "MJb", "NXa", "NX9", "NX8"}, "Command number X (bit 10-8) / line major (bit 11-8)"}, // R#41
	RD{{"NY7", "NY6", "NY5", "NY4", "NY3", "NY2", "NY1", "NY0"}, "Command number Y / bytes (bit 15-8) / line minor"},   // R#42
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "NYb", "NYa", "NY9", "NY8"}, "Command number Y (bit 11-8) / bytes (bit 18-16) / line minor (bit 11-8)"}, // R#43
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", "DIY", "DIX", "NEQ", "MAJ"}, "Command argument: direction, search equality, line major axis"}, // R#44
	RD{{" 0 ", " 0 ", " 0 ", "TP ", "L11", "L10", "L01", "L00"}, "Command logical operation"},      // R#45
	RD{{"WM7", "WM6", "WM5", "WM4", "WM3", "WM2", "WM1", "WM0"}, "Command write mask (bit 7-0)"},   // R#46
	RD{{"WMf", "WMe", "WMd", "WMc", "WMb", "WMa", "WM9", "WM8"}, "Command write mask (bit 15-8)"},  // R#47
	RD{{"FC7", "FC6", "FC5", "FC4", "FC3", "FC2", "FC1", "FC0"}, "Command foreground color (bit 7-0)"},  // R#48
	RD{{"FCf", "FCe", "FCd", "FCc", "FCb", "FCa", "FC9", "FC8"}, "Command foreground color (bit 15-8)"}, // R#49
	RD{{"BC7", "BC6", "BC5", "BC4", "BC3", "BC2", "BC1", "BC0"}, "Command background color (bit 7-0)"},  // R#50
	RD{{"BCf", "BCe", "BCd", "BCc", "BCb", "BCa", "BC9", "BC8"}, "Command background color (bit 15-8)"}, // R#51
	RD{{"OP3", "OP2", "OP1", "OP0", "AYM", "AYE", "AXM", "AXE"}, "Command opcode + pointer advance"},    // R#52
	RD{{"BX7", "BX6", "BX5", "BX4", "BX3", "BX2", "BX1", "BX0"}, "SEARCH border X (bit 7-0), read-only"},   // R#53
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "BXa", "BX9", "BX8"}, "SEARCH border X (bit 10-8), read-only"},  // R#54

	RD{{"TR ", "VR ", "HR ", "BD ", " 0 ", "MCS", "EO ", "CE "}, "Status (port P#5)"},         // S#0
	RD{{" 0 ", " 0 ", " 0 ", " 0 ", " 0 ", "CE ", "HI ", "VI "}, "Interrupt flags (port P#6)"}, // S#1
};
static_assert(registerDescriptions.size() == INT_FLAG + 1);

using Regs = V9990Registers;
using S = RegSubFunction;
using R = RegFunction<Regs>;
using namespace std::literals;

static constexpr auto regFunctions = std::array{
	// screen mode
	R{{S{6, 0xC0}}, "display mode",
	  [](uint32_t v) {
		static constexpr auto names = std::array{"P1", "P2", "bitmap (Bx)", "stand-by"};
		return tmpStrCat("display mode: ", names[v]);
	  }},
	R{{S{6, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{6, 0x0C}}, "image width (in pixels)",
	  [](uint32_t v) { return tmpStrCat("image width: ", 256 << v); }},
	R{{S{6, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{6, 0x03}}, "bits per pixel (bitmap modes only)",
	  [](uint32_t v) { return tmpStrCat("bits/pixel: ", 2 << v); }},
	R{{S{6, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{6, 0x30}}, "pixel clock: master clock divided by 4, 2 or 1",
	  [](uint32_t v) {
		static constexpr auto names = std::array{"/4", "/2", "/1", "reserved"};
		return tmpStrCat("clock: ", names[v], '\n');
	  }},
	R{{S{7, 0x08}}, "PAL or NTSC timing",
	  [](uint32_t v) { return TemporaryString(v ? "PAL" : "NTSC"); }},
	R{{S{7, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{7, 0x02}}, "interlace",
	  [](uint32_t v) { return tmpStrCat("interlace: ", yesNo(v)); }},
	R{{S{7, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{7, 0x01}}, "high scan (double the number of lines)",
	  [](uint32_t v) { return tmpStrCat("high scan: ", yesNo(v)); }},
	R{{S{7, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{7, 0x40}}, "select the 640x480 mode (B6)",
	  [](uint32_t v) { return tmpStrCat("640x480 (B6): ", yesNo(v), '\n'); }},
	R{{S{16, 0x0F}}, "horizontal display adjust (signed)",
	  [](uint32_t v) { return tmpStrCat("display adjust: h=", int((v ^ 7)) - 8); }},
	R{{S{16, 0xF0}}, "vertical display adjust (signed)",
	  [](uint32_t v) { return tmpStrCat(" v=", int((v ^ 7)) - 8, '\n'); }},

	// control / interrupt
	R{{S{8, 0x80}}, "display enable",
	  [](uint32_t v) { return TemporaryString(v ? "display: on" : "display: off (backdrop only)"); }},
	R{{S{8, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{8, 0x40}}, "sprite/cursor disable",
	  [](uint32_t v) { return tmpStrCat("sprites/cursors: ", v ? "disabled"sv : "enabled"sv); }},
	R{{S{8, 0}}, "", [](uint32_t) { return TemporaryString(", "); }},
	R{{S{8, 0x20}}, "YS output; only superimposes when an external video source is present "
	                "(in openMSX: the Video9000 with its layer register, port 0x6F, set to superimpose)",
	  [](uint32_t v, const Regs& regs) {
		return tmpStrCat("YS: ", v ? "on"sv : "off"sv,
		                 " (superimposing: ", yesNo(regs.vdp.isSuperimposing()), ")\n");
	  }},
	R{{S{9, 0x04}}, "command end interrupt enable",
	  [](uint32_t v) { return tmpStrCat("interrupts: command end=", yesNo(v)); }},
	R{{S{9, 0x02}}, "display position (line) interrupt enable",
	  [](uint32_t v) { return tmpStrCat(", line=", yesNo(v)); }},
	R{{S{9, 0x01}}, "vertical blank interrupt enable",
	  [](uint32_t v) { return tmpStrCat(", vblank=", yesNo(v), '\n'); }},
	R{{S{10, 0xFF}, S{11, 0x03}}, "interrupt line",
	  [](uint32_t v) { return tmpStrCat("interrupt line: ", v); }},
	R{{S{11, 0x80}}, "line interrupt on every line",
	  [](uint32_t v) { return TemporaryString(v ? " (every line)" : ""); }},
	R{{S{12, 0x0F}}, "interrupt X position (in units of 64 master clocks)",
	  [](uint32_t v) { return tmpStrCat(", interrupt X: ", v, '\n'); }},

	// palette
	R{{S{13, 0xC0}}, "palette mode",
	  [](uint32_t v) {
		static constexpr auto names = std::array{"palette", "256 color", "YJK", "YUV"};
		return tmpStrCat("palette mode: ", names[v]);
	  }},
	R{{S{13, 0x20}}, "YAE: mix RGB (palette) pixels into YJK/YUV",
	  [](uint32_t v) { return tmpStrCat(", YAE: ", yesNo(v)); }},
	R{{S{13, 0x0F}}, "palette offset: bits 1-0 for layer A / even bytes, bits 3-2 for layer B / odd bytes",
	  [](uint32_t v) { return tmpStrCat(", palette offset: A=", v & 3, " B=", v >> 2, '\n'); }},
	R{{S{14, 0xFC}}, "palette pointer: palette entry",
	  [](uint32_t v) { return tmpStrCat("palette pointer: entry ", v); }},
	R{{S{14, 0x03}}, "palette pointer: color component",
	  [](uint32_t v) {
		static constexpr auto names = std::array{"R", "G", "B", "-"};
		return tmpStrCat(" ", names[v], '\n');
	  }},
	R{{S{15, 0x3F}}, "backdrop color (palette entry)",
	  [](uint32_t v) { return tmpStrCat("backdrop color: ", v, '\n'); }},

	// scroll
	R{{S{17, 0xFF}, S{18, 0x1F}}, "layer A vertical scroll",
	  [](uint32_t v) { return tmpStrCat("layer A scroll: y=", v); }},
	R{{S{19, 0x07}, S{20, 0xFF}}, "layer A horizontal scroll",
	  [](uint32_t v) { return tmpStrCat(" x=", v); }},
	R{{S{18, 0xC0}}, "roll: the vertical scroll wraps at 256 or 512 lines, or at the image size",
	  [](uint32_t v) {
		return tmpStrCat(", roll: ", (v & 2) ? "512 lines"sv : (v & 1) ? "256 lines"sv : "image size"sv, '\n');
	  }},
	R{{S{21, 0xFF}, S{22, 0x01}}, "layer B vertical scroll (P1 only)",
	  [](uint32_t v) { return tmpStrCat("layer B scroll: y=", v); }},
	R{{S{23, 0x07}, S{24, 0x3F}}, "layer B horizontal scroll (P1 only)",
	  [](uint32_t v) { return tmpStrCat(" x=", v, '\n'); }},
	R{{S{22, 0x80}}, "disable layer A (undocumented)",
	  [](uint32_t v) { return tmpStrCat("layer A disabled: ", yesNo(v)); }},
	R{{S{22, 0x40}}, "disable layer B (undocumented)",
	  [](uint32_t v) { return tmpStrCat(", layer B disabled: ", yesNo(v), '\n'); }},

	// sprites / cursor / priority
	R{{S{25, 0x0F}}, "sprite pattern table address: P1 uses bits 3-1, P2 bits 3-0",
	  [](uint32_t v, const Regs& regs) {
		if (regs.isP1()) return tmpStrCat("sprite patterns: 0x", hex_string<5>((v & 0x0E) << 14), '\n');
		if (regs.isP2()) return tmpStrCat("sprite patterns: 0x", hex_string<5>(v << 15), '\n');
		return TemporaryString("sprite patterns: (P1/P2 only)\n");
	  }},
	R{{S{27, 0x03}}, "priority control (P1): layer A is in front left of this x position",
	  [](uint32_t v) { return tmpStrCat("priority split (P1): x=", (v == 0) ? 256 : (v << 6)); }},
	R{{S{27, 0x0C}}, "priority control (P1): layer A is in front above this y position",
	  [](uint32_t v) { return tmpStrCat(" y=", (v == 0) ? 256 : (v << 6), '\n'); }},
	R{{S{28, 0x0F}}, "sprite/cursor palette offset",
	  [](uint32_t v) { return tmpStrCat("sprite/cursor palette offset: ", v << 2, '\n'); }},

	// VRAM access
	R{{S{0, 0xFF}, S{1, 0xFF}}, "VRAM write address",
	  [](uint32_t v, const Regs& regs) {
		return tmpStrCat("write address: 0x", hex_string<5>(((regs.values[2] & 0x07) << 16) | v));
	  }},
	R{{S{2, 0x80}}, "don't increment the write address",
	  [](uint32_t v) { return TemporaryString(v ? " (no increment)\n" : "\n"); }},
	R{{S{3, 0xFF}, S{4, 0xFF}}, "VRAM read address",
	  [](uint32_t v, const Regs& regs) {
		return tmpStrCat("read address: 0x", hex_string<5>(((regs.values[5] & 0x07) << 16) | v));
	  }},
	R{{S{5, 0x80}}, "don't increment the read address",
	  [](uint32_t v) { return TemporaryString(v ? " (no increment)\n" : "\n"); }},

	// command
	R{{S{52, 0xF0}}, "command opcode",
	  [](uint32_t v) {
		static constexpr auto names = std::array{
			"STOP", "LMMC", "LMMV", "LMCM", "LMMM", "CMMC", "CMMK", "CMMM",
			"CMMV", "BMXL", "BMLX", "BMLL", "LINE", "SRCH", "POINT", "PSET"};
		return tmpStrCat("opcode: ", names[v]);
	  }},
	R{{S{44, 0x0C}}, "direction: DIY (up/down), DIX (left/right)",
	  [](uint32_t v) {
		return tmpStrCat(", direction: ", (v & 2) ? "up"sv : "down"sv, '/', (v & 1) ? "left"sv : "right"sv);
	  }},
	R{{S{45, 0x1F}}, "logical operation; bit 4 (TP) makes color 0 transparent",
	  [](uint32_t v) {
		return tmpStrCat(", logop: 0x", hex_string<2>(v & 0x0F), (v & 0x10) ? " (transparent)"sv : ""sv, '\n');
	  }},
	R{{S{32, 0xFF}, S{33, 0x07}}, "source X",
	  [](uint32_t v) { return tmpStrCat("source: x=", v); }},
	R{{S{34, 0xFF}, S{35, 0x0F}}, "source Y",
	  [](uint32_t v) { return tmpStrCat(" y=", v); }},
	R{{S{36, 0xFF}, S{37, 0x07}}, "destination X",
	  [](uint32_t v) { return tmpStrCat(", destination: x=", v); }},
	R{{S{38, 0xFF}, S{39, 0x0F}}, "destination Y",
	  [](uint32_t v) { return tmpStrCat(" y=", v); }},
	R{{S{40, 0xFF}, S{41, 0x0F}}, "number of pixels in X",
	  [](uint32_t v) { return tmpStrCat(", size: NX=", v); }},
	R{{S{42, 0xFF}, S{43, 0x0F}}, "number of pixels in Y",
	  [](uint32_t v) { return tmpStrCat(" NY=", v, '\n'); }},
	R{{S{46, 0xFF}, S{47, 0xFF}}, "write mask",
	  [](uint32_t v) { return tmpStrCat("write mask: 0x", hex_string<4>(v)); }},
	R{{S{48, 0xFF}, S{49, 0xFF}}, "foreground color",
	  [](uint32_t v) { return tmpStrCat(", fg: 0x", hex_string<4>(v)); }},
	R{{S{50, 0xFF}, S{51, 0xFF}}, "background color",
	  [](uint32_t v) { return tmpStrCat(", bg: 0x", hex_string<4>(v), '\n'); }},
	R{{S{53, 0xFF}, S{54, 0x07}}, "SRCH result: the border X position that was found",
	  [](uint32_t v) { return tmpStrCat("search border X: ", v, '\n'); }},

	// status ports
	R{{S{STATUS, 0x80}}, "TR: ready to transfer command data",
	  [](uint32_t v) { return tmpStrCat("transfer ready: ", yesNo(v)); }},
	R{{S{STATUS, 0x40}}, "VR: vertical blanking",
	  [](uint32_t v) { return tmpStrCat(", vblank: ", yesNo(v)); }},
	R{{S{STATUS, 0x20}}, "HR: horizontal blanking",
	  [](uint32_t v) { return tmpStrCat(", hblank: ", yesNo(v)); }},
	R{{S{STATUS, 0x10}}, "BD: SRCH found the border color",
	  [](uint32_t v) { return tmpStrCat(", border detected: ", yesNo(v), '\n'); }},
	R{{S{STATUS, 0x04}}, "MCS: the master clock (R#7 C25)",
	  [](uint32_t v) { return tmpStrCat("master clock: ", v ? "MCKIN"sv : "XTAL1"sv); }},
	R{{S{STATUS, 0x02}}, "EO: second field of an interlaced frame. openMSX toggles it every frame "
	                     "even when interlace is off, where it has no effect on the display",
	  [](uint32_t v, const Regs& regs) {
		return tmpStrCat(", interlace field: ", !regs.vdp.isInterlaced() ? "n/a"sv : v ? "2nd"sv : "1st"sv);
	  }},
	R{{S{STATUS, 0x01}}, "CE: a command is executing",
	  [](uint32_t v) { return tmpStrCat(", command executing: ", yesNo(v), '\n'); }},
	R{{S{INT_FLAG, 0x04}}, "command end interrupt pending",
	  [](uint32_t v) { return tmpStrCat("pending interrupts: command end=", yesNo(v)); }},
	R{{S{INT_FLAG, 0x02}}, "line interrupt pending",
	  [](uint32_t v) { return tmpStrCat(", line=", yesNo(v)); }},
	R{{S{INT_FLAG, 0x01}}, "vertical blank interrupt pending",
	  [](uint32_t v) { return tmpStrCat(", vblank=", yesNo(v), '\n'); }},
};

static constexpr VDPRegisterTables<Regs> vdpRegisters{registerDescriptions, regFunctions, STATUS};

void ImGuiV9990Regs::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	ImGui::SetNextWindowSize(gl::vec2{41, 40} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window(title.c_str(), &show, [&]{
		auto* vdp = dynamic_cast<V9990*>(motherBoard->findDevice("Sunrise GFX9000"));
		if (!vdp) return;
		auto time = motherBoard->getCurrentTime();

		Regs regs(*vdp, time);
		drawVDPRegisters(vdpRegisters, regs, hoveredFunction, explanation, [&](auto& viewer) {
			im::TreeNode("Screen mode", &openScreen, [&]{
				static constexpr auto screenRegs = std::to_array<uint8_t>({6, 7, 16});
				viewer.drawSection(screenRegs);
			});
			im::TreeNode("Control / interrupt", &openControl, [&]{
				static constexpr auto controlRegs = std::to_array<uint8_t>({8, 9, 10, 11, 12});
				viewer.drawSection(controlRegs);
			});
			im::TreeNode("Palette", &openPalette, [&]{
				static constexpr auto paletteRegs = std::to_array<uint8_t>({13, 14, 15});
				viewer.drawSection(paletteRegs);
			});
			im::TreeNode("Scroll", &openScroll, [&]{
				static constexpr auto scrollRegs = std::to_array<uint8_t>({17, 18, 19, 20, 21, 22, 23, 24});
				viewer.drawSection(scrollRegs);
			});
			im::TreeNode("Sprites / cursor / priority", &openSprite, [&]{
				static constexpr auto spriteRegs = std::to_array<uint8_t>({25, 26, 27, 28});
				viewer.drawSection(spriteRegs);
			});
			im::TreeNode("VRAM access", &openVramAccess, [&]{
				static constexpr auto vramAccessRegs = std::to_array<uint8_t>({0, 1, 2, 3, 4, 5});
				viewer.drawSection(vramAccessRegs);
			});
			im::TreeNode("Command", &openCommand, [&]{
				ImGui::TextDisabledUnformatted(
					"R#32-54 are write-only; the values are read back from the command engine."sv);
				static constexpr auto commandRegs = std::to_array<uint8_t>({
					32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
					45, 46, 47, 48, 49, 50, 51, 52, 53, 54});
				viewer.drawSection(commandRegs);
			});
			im::TreeNode("Status ports (S#0 = P#5, S#1 = P#6)", &openStatus, [&]{
				static constexpr auto statusRegs = std::to_array<uint8_t>({STATUS, INT_FLAG});
				viewer.drawSection(statusRegs);
			});
		});
	});
}

} // namespace openmsx
