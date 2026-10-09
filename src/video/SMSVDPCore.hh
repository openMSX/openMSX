// license:BSD-3-Clause
// copyright-holders:Wilbert Pol, Enik Land
//
// This file is adapted for openMSX from MAME's src/devices/video/315_5124.cpp
// (BSD-3-Clause, copyright Wilbert Pol and Enik Land):
//   "Implementation of video hardware chips used by Sega System E,
//    Master System, and Game Gear."
//
// Only the Master System variants (315-5124 "SMS1" and 315-5246 "SMS2") are
// implemented here; the Game Gear (315-5377) and Mega Drive (315-5313 mode 4)
// variants and the integrated SN76489 are intentionally left to other code
// (openMSX uses its own SN76489 for the PSG). The port keeps MAME's register,
// VRAM, CRAM, sprite and scanline semantics, including the documented timing
// quirks, but replaces MAME's device/timer/screen framework with a small
// pull/event interface driven by openMSX's scheduler.
//
// This class deliberately has no openMSX dependencies (only standard
// headers), so the chip emulation can be unit-tested in isolation; all
// openMSX glue lives in SMSVDP.

#ifndef SMSVDPCORE_HH
#define SMSVDPCORE_HH

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <span>

namespace openmsx {

/** Sega Master System VDP core (315-5124 / 315-5246).
  */
class SMSVDPCore
{
public:
	enum class Variant : uint8_t { SMS1, SMS2 };

	// Chip/frame geometry (from MAME).
	static constexpr int WIDTH = 342;         // pixels per scanline
	static constexpr int HEIGHT_NTSC = 262;   // lines per frame
	static constexpr int HEIGHT_PAL = 313;
	static constexpr int ACTIVE_WIDTH = 256;  // visible pixels
	static constexpr int OUTPUT_WIDTH = 320;  // openMSX output
	static constexpr int OUTPUT_HEIGHT = 240;

	// Status register bits.
	static constexpr uint8_t STATUS_VINT = 0x80;
	static constexpr uint8_t STATUS_SPROVR = 0x40;
	static constexpr uint8_t STATUS_SPRCOL = 0x20;

	// Line timing indexes (the actual hpos values live in the lineTiming
	// table; use the vintHpos()/hintHpos()/intHpos() accessors below).
	enum {
		VINT_HPOS_IDX = 0,
		VINT_FLAG_HPOS_IDX,
		HINT_HPOS_IDX,
		NMI_HPOS_IDX,
		XSCROLL_HPOS_IDX,
		VCOUNT_CHANGE_HPOS_IDX,
		SPROVR_HPOS_IDX,
		SPRCOL_BASEHPOS_IDX
	};

	// Frame timing indexes (values in frameTiming).
	enum {
		VERTICAL_SYNC = 0,
		TOP_BLANKING,
		TOP_BORDER,
		ACTIVE_DISPLAY_V,
		BOTTOM_BORDER,
		BOTTOM_BLANKING
	};

	/** @param nintCallback Called with the /INT line state (true = asserted;
	  *   the pin is active low on the real chip) every time the chip
	  *   evaluates the line. Must be a valid callback. Mirrors MAME's
	  *   n_int_cb; the listener (SMSVDP's IRQHelper) owns the state.
	  */
	explicit SMSVDPCore(Variant variant, bool isPal,
	                    std::function<void(bool)> nintCallback);

	void reset();
	void setPal(bool pal);
	[[nodiscard]] bool isPal() const { return palFlag; }
	[[nodiscard]] Variant getVariant() const { return variant; }
	[[nodiscard]] int height() const { return palFlag ? HEIGHT_PAL : HEIGHT_NTSC; }
	[[nodiscard]] int activeStart() const {
		return frameTiming[VERTICAL_SYNC] + frameTiming[TOP_BLANKING] + frameTiming[TOP_BORDER];
	}
	[[nodiscard]] int yPixels() const { return yPixelsValue; }

	// hpos at which the /INT line is evaluated. The chip has a single /INT
	// pin shared by the VINT and HINT logic, so one moment covers both.
	[[nodiscard]] int vintHpos() const { return lineTiming[VINT_HPOS_IDX]; }
	[[nodiscard]] int hintHpos() const { return lineTiming[HINT_HPOS_IDX]; }
	[[nodiscard]] int intHpos() const {
		return std::max(vintHpos(), hintHpos());
	}

	// CPU interface (ports 0x88/0x89), hpos = pixel position within the line.
	[[nodiscard]] uint8_t readData();
	void writeData(uint8_t value);
	[[nodiscard]] uint8_t readControl(int hpos);
	void writeControl(uint8_t value, int hpos);

	/** Write a VDP register and run its side effects. The CPU reaches this
	  * via the two-byte control port sequence; the debugger uses it
	  * directly (so it does not disturb the CPU's control write latch).
	  */
	void writeRegister(uint8_t regNum, uint8_t value, int hpos);

	// Counter accesses (ports 0x48/0x49).
	[[nodiscard]] uint8_t readVCounter(int vpos, int hpos) const;
	[[nodiscard]] uint8_t readHCounter();
	void latchHCounter(int hpos);

	// Are interrupts enabled? VINT (R#1 bit 5) or HINT (R#0 bit 4).
	[[nodiscard]] bool interruptsEnabled() const {
		return (reg[0x01] & 0x20) || (reg[0x00] & 0x10);
	}

	// Scheduler entry points.
	// processLine() mirrors MAME's process_line_timer (MAME schedules it at
	// hpos 14; we run it at the start of the line, which is equivalent for
	// the latched values because writeControl() handles the mid-line
	// updates of R#1/R#8 itself).
	void processLine(int vpos);
	// checkPendingFlags() promotes pending flags; call with the current hpos.
	void checkPendingFlags(int hpos);
	// updateInterrupts() mirrors MAME's trigger_hint/trigger_vint timers;
	// scheduled once per line (at/after HINT_HPOS and VINT_HPOS).
	void updateInterrupts(int hpos);
	// drawLine() renders one output scanline as 320 palette indices
	// (values 0..79; see paletteColors()).
	void drawLine(int outLine, std::span<uint8_t, OUTPUT_WIDTH> paletteIndices);

	/** The 80 chip colors (RGB888): indices 0..63 are the mode-4 CRAM
	  * colors, 64..79 the fixed TMS colors. Used to translate the palette
	  * indices to actual colors (e.g. with the video settings applied).
	  */
	[[nodiscard]] std::span<const uint32_t, 80> paletteColors() const {
		return palette;
	}

	// Debug/test accessors.
	[[nodiscard]] uint8_t peekReg(int i) const { return reg[i]; }
	[[nodiscard]] uint8_t peekStatus() const { return status; }
	[[nodiscard]] uint8_t peekVram(uint16_t a) const { return vram[a & 0x3fff]; }
	[[nodiscard]] uint8_t peekCram(int i) const { return cram[i & 0x1f]; }
	[[nodiscard]] int peekMode() const { return vdpMode; }

	// Side-effect-free versions of the counter/data/control reads (used by
	// peekIO()); peekControl() simulates the pending-flag promotion that a
	// real read would perform, without clearing anything.
	[[nodiscard]] uint8_t peekHCounter() const { return hcounter; }
	[[nodiscard]] uint8_t peekData() const { return buffer; }
	[[nodiscard]] uint8_t peekControl(int hpos) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize_blob("vram", vram);
		ar.serialize_blob("cram", cram);
		ar.serialize("reg", reg);
		ar.serialize("status", status);
		ar.serialize("pendingStatus", pendingStatus);
		ar.serialize("reg8copy", reg8copy);
		ar.serialize("reg9copy", reg9copy);
		ar.serialize("addrmode", addrmode);
		ar.serialize("addr", addr);
		ar.serialize("cramDirty", cramDirty);
		ar.serialize("hcounterLatched", hcounterLatched);
		ar.serialize("hintOccurred", hintOccurred);
		ar.serialize("pendingHint", pendingHint);
		ar.serialize("pendingControlWrite", pendingControlWrite);
		ar.serialize("pendingSprcolX", pendingSprcolX);
		ar.serialize("buffer", buffer);
		ar.serialize("vdpMode", vdpMode);
		ar.serialize("yPixelsValue", yPixelsValue);
		ar.serialize("lineCounter", lineCounter);
		ar.serialize("hcounter", hcounter);
		ar.serialize("displayDisabled", displayDisabled);
		ar.serialize("spriteAttributeBase", spriteAttributeBase);
		ar.serialize("spritePatternLine", spritePatternLine);
		ar.serialize("spriteTileSelected", spriteTileSelected);
		ar.serialize("spriteX", spriteX);
		ar.serialize("spriteFlags", spriteFlags);
		ar.serialize("spriteCount", spriteCount);
		ar.serialize("spriteHeight", spriteHeight);
		ar.serialize("spriteZoomScale", spriteZoomScale);
		ar.serialize("currentPalette", currentPalette);
		ar.serialize("palFlag", palFlag);
		// frameTiming/lineTiming are derived; rebuilt on load
		if constexpr (Archive::IS_LOADER) {
			setFrameTiming();
		}
	}

private:
	// helpers
	void setNint(bool asserted);
	void selectExtendedResMode4(bool m1, bool m2, bool m3);
	void selectDisplayMode();
	void setDisplaySettings();
	void setFrameTiming();
	[[nodiscard]] uint8_t vcount(int vpos, int hpos) const;
	[[nodiscard]] uint8_t hcount(int hpos) const;
	void writeMemory(uint8_t data);
	void loadVramAddr(uint8_t data);
	void updatePalette();
	void cramWrite(uint8_t data);
	[[nodiscard]] uint8_t backdropColor() const {
		return uint8_t((vdpMode == 4 ? 0x10 : 0x00) + (reg[0x07] & 0x0f));
	}

	// variant-dependent selection helpers
	[[nodiscard]] uint16_t nameRowMode4(uint16_t row) const;
	[[nodiscard]] uint16_t tile1SelectMode4(uint16_t tileNumber) const;
	[[nodiscard]] uint16_t tile2SelectMode4(uint16_t tileNumber) const;
	[[nodiscard]] uint8_t spriteAttributeExtraOffsetMode4(uint8_t offset) const;
	[[nodiscard]] uint8_t spriteTileSelectMode4(uint8_t tileNumber) const;

	// sprite handling
	void selectSprites(int line);
	void spriteCountOverflow(int line, int spriteIndex);
	void spriteCollision(int line, int spriteColX);
	void drawSpritesMode4(std::span<uint8_t, 256> lineBuffer,
	                      std::span<int, 256> prioritySelected, int line);
	void drawSpritesTms9918Mode(std::span<uint8_t, 256> lineBuffer, int line);

	// background scanline drawing
	void drawLeftmostPixelsMode4(std::span<uint8_t, 256> lineBuffer,
	                             std::span<int, 256> prioritySelected,
	                             int fineXScroll, int paletteSelected, int tileLine);
	void drawScanlineMode4(std::span<uint8_t, 256> lineBuffer,
	                       std::span<int, 256> prioritySelected, int line);
	void drawScanlineMode3(std::span<uint8_t, 256> lineBuffer, int line);
	void drawScanlineMode2(std::span<uint8_t, 256> lineBuffer, int line);
	void drawScanlineMode1(std::span<uint8_t, 256> lineBuffer, int line);
	void drawScanlineMode0(std::span<uint8_t, 256> lineBuffer, int line);

private:
	Variant variant;

	// Frame/line timing (spans into the static tables).
	std::span<const uint8_t, 6> frameTiming;
	std::span<const uint8_t, 8> lineTiming;

	std::array<uint8_t, 0x4000> vram;   // 16 KiB
	std::array<uint8_t, 32> cram;       // 32 colors x 1 byte

	std::array<uint8_t, 16> reg{};
	uint8_t status = 0;
	uint8_t pendingStatus = 0;
	uint8_t reg8copy = 0;
	uint8_t reg9copy = 0;
	uint8_t addrmode = 0;
	uint16_t addr = 0;
	bool cramDirty = true;
	bool hcounterLatched = false;
	bool hintOccurred = false;
	bool pendingHint = false;
	bool pendingControlWrite = false;
	int pendingSprcolX = 0;
	uint8_t buffer = 0;
	std::function<void(bool)> nintCallback;
	int vdpMode = 0;
	int yPixelsValue = 192;
	uint8_t lineCounter = 0;
	uint8_t hcounter = 0;
	bool displayDisabled = false;

	// sprites (selected for the current line)
	uint16_t spriteAttributeBase = 0;
	std::array<uint16_t, 8> spritePatternLine{};
	std::array<int, 8> spriteTileSelected{};
	std::array<int, 8> spriteX{};
	std::array<uint8_t, 8> spriteFlags{};
	int spriteCount = 0;
	int spriteHeight = 8;
	int spriteZoomScale = 1;
	int maxSpriteZoomHcount = 8;
	int maxSpriteZoomVcount = 8;

	std::array<uint8_t, 32> currentPalette{};
	std::array<uint32_t, 80> palette{};

	bool palFlag = false;
};

} // namespace openmsx

#endif
