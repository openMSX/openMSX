// license:BSD-3-Clause
// copyright-holders:Wilbert Pol, Enik Land
//
// Adapted for openMSX from MAME's src/devices/video/315_5124.cpp.
// See SMSVDPCore.hh for the adaptation notes.

#include "SMSVDPCore.hh"

#include <cassert>

namespace openmsx {

// MAME: line_315_5124
static constexpr std::array<uint8_t, 8> lineTiming315_5124 = {
	24, // VINT_HPOS_IDX
	24, // VINT_FLAG_HPOS_IDX
	26, // HINT_HPOS_IDX
	28, // NMI_HPOS_IDX         (not verified)
	21, // XSCROLL_HPOS_IDX
	23, // VCOUNT_CHANGE_HPOS_IDX
	24, // SPROVR_HPOS_IDX
	59  // SPRCOL_BASEHPOS_IDX
};

static constexpr std::array<uint8_t, 6> ntsc192 = {3, 13, 27, 192, 24, 3};
static constexpr std::array<uint8_t, 6> ntsc224 = {3, 13, 11, 224,  8, 3};
static constexpr std::array<uint8_t, 6> ntsc240 = {3, 13,  3, 240,  0, 3};
static constexpr std::array<uint8_t, 6> pal192  = {3, 13, 54, 192, 48, 3};
static constexpr std::array<uint8_t, 6> pal224  = {3, 13, 38, 224, 32, 3};
static constexpr std::array<uint8_t, 6> pal240  = {3, 13, 30, 240, 24, 3};

// MAME: DISPLAY_DISABLED_HPOS
static constexpr int DISPLAY_DISABLED_HPOS = 24; // not verified

static constexpr std::array<std::array<uint8_t, 3>, 16> tmsColors = {{
	{0, 0, 0}, {0, 0, 0}, {0, 2, 0}, {0, 3, 0},
	{0, 0, 1}, {0, 0, 3}, {1, 0, 0}, {0, 3, 3},
	{2, 0, 0}, {3, 0, 0}, {1, 1, 0}, {3, 3, 0},
	{0, 1, 0}, {3, 0, 3}, {1, 1, 1}, {3, 3, 3}
}};

[[nodiscard]] static constexpr uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
	return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}

[[nodiscard]] static std::array<uint32_t, 80> buildPalette(SMSVDPCore::Variant variant)
{
	std::array<uint32_t, 80> result{};
	if (variant == SMSVDPCore::Variant::SMS1) {
		// MAME: sega315_5124_palette
		static constexpr uint8_t level[4] = {0, 78, 160, 238};
		static constexpr uint8_t blueLevel[4] = {0, 98, 160, 238};
		for (int i = 0; i < 64; ++i) {
			result[i] = rgb(level[i & 0x03], level[(i & 0x0c) >> 2],
			                blueLevel[(i & 0x30) >> 4]);
		}
		for (int i = 0; i < 16; ++i) {
			const auto& c = tmsColors[i];
			result[64 + i] = rgb(level[c[0]], level[c[1]], blueLevel[c[2]]);
		}
	} else {
		// MAME: sega315_5246_palette
		static constexpr uint8_t level[4] = {0, 89, 174, 255};
		for (int i = 0; i < 64; ++i) {
			result[i] = rgb(level[i & 0x03], level[(i & 0x0c) >> 2],
			                level[(i & 0x30) >> 4]);
		}
		for (int i = 0; i < 16; ++i) {
			const auto& c = tmsColors[i];
			result[64 + i] = rgb(level[c[0]], level[c[1]], level[c[2]]);
		}
	}
	return result;
}

SMSVDPCore::SMSVDPCore(Variant variant_, bool isPal)
	: variant(variant_)
	, lineTiming(lineTiming315_5124.data())
	, palette(buildPalette(variant_))
{
	maxSpriteZoomHcount = (variant == Variant::SMS1) ? 4 : 8;
	maxSpriteZoomVcount = 8;
	palFlag = isPal;
	reset();
}

void SMSVDPCore::reset()
{
	// MAME: device_reset
	reg.fill(0x00);
	reg[0x02] = 0x0e;
	reg[0x0a] = 0xff;
	status = pendingStatus = uint8_t(~(STATUS_VINT | STATUS_SPROVR | STATUS_SPRCOL));
	pendingSprcolX = 0;
	pendingControlWrite = false;
	pendingHint = false;
	hintOccurred = false;
	reg8copy = 0;
	reg9copy = 0;
	addrmode = 0;
	addr = 0;
	displayDisabled = false;
	cramDirty = true;
	buffer = 0;
	nintAsserted = false;
	lineCounter = 0;
	hcounter = 0;
	hcounterLatched = false;
	currentPalette.fill(0);
	setDisplaySettings();
	cram.fill(0);
	vram.fill(0);
}

void SMSVDPCore::setPal(bool pal)
{
	palFlag = pal;
	setFrameTiming();
}

// MAME: select_extended_res_mode4
void SMSVDPCore::selectExtendedResMode4(bool m1, bool m2, bool m3)
{
	if (variant == Variant::SMS1) {
		// no extended resolution supported
		return;
	}
	if (m2) {
		if (m1 && !m3) {
			yPixelsValue = 224; // 224-line display
		} else if (!m1 && m3) {
			yPixelsValue = 240; // 240-line display
		}
	}
}

// MAME: select_display_mode
void SMSVDPCore::selectDisplayMode()
{
	const bool m1 = (reg[0x01] & 0x10) != 0;
	const bool m2 = (reg[0x00] & 0x02) != 0;
	const bool m3 = (reg[0x01] & 0x08) != 0;
	const bool m4 = (reg[0x00] & 0x04) != 0;

	if (m4) {
		vdpMode = 4;
		selectExtendedResMode4(m1, m2, m3);
	} else {
		if (!m1 && !m2 && !m3) {
			vdpMode = 0; // Graphics I
		} else if (m1 && !m2 && !m3) {
			vdpMode = 1; // Text
		} else if (!m1 && m2 && !m3) {
			vdpMode = 2; // Graphics II
		} else if (!m1 && !m2 && m3) {
			vdpMode = 3; // Multicolor
		}
		// Unknown modes keep the previous value (MAME only logs).
	}
}

// MAME: set_display_settings
void SMSVDPCore::setDisplaySettings()
{
	yPixelsValue = 192;
	selectDisplayMode();
	setFrameTiming();
	cramDirty = true;
}

// MAME: set_frame_timing
void SMSVDPCore::setFrameTiming()
{
	const std::array<uint8_t, 6>* timing = nullptr;
	switch (yPixelsValue) {
	case 192: timing = palFlag ? &pal192 : &ntsc192; break;
	case 224: timing = palFlag ? &pal224 : &ntsc224; break;
	case 240: timing = palFlag ? &pal240 : &ntsc240; break;
	default:  timing = palFlag ? &pal192 : &ntsc192; break;
	}
	frameTiming = timing->data();
}

// MAME: vcount
uint8_t SMSVDPCore::vcount(int vpos, int hpos) const
{
	const int activeScrStart =
		frameTiming[VERTICAL_SYNC] + frameTiming[TOP_BLANKING] + frameTiming[TOP_BORDER];
	if (hpos < lineTiming[VCOUNT_CHANGE_HPOS_IDX]) {
		--vpos;
		if (vpos < 0) vpos += height();
	}
	return uint8_t((vpos - activeScrStart) & 0xff);
}

uint8_t SMSVDPCore::readVCounter(int vpos, int hpos) const
{
	return vcount(vpos, hpos);
}

// MAME: hcount
uint8_t SMSVDPCore::hcount(int hpos) const
{
	// The hcount value returned by the VDP seems to be based on the previous hpos
	int hclock = hpos - 1;
	if (hclock < 0) hclock += WIDTH;
	return uint8_t(((hclock - 46) >> 1) & 0xff);
}

// MAME: hcount_read
uint8_t SMSVDPCore::readHCounter()
{
	hcounterLatched = false;
	return hcounter;
}

// MAME: hcount_latch
void SMSVDPCore::latchHCounter(int hpos)
{
	hcounter = hcount(hpos);
	hcounterLatched = true;
}

// MAME: trigger_hint + trigger_vint, evaluated at the current hpos.
void SMSVDPCore::updateInterrupts(int hpos)
{
	bool wantInt = false;
	if ((reg[0x00] & 0x10) && (hpos >= lineTiming[HINT_HPOS_IDX]) &&
	    (pendingHint || hintOccurred)) {
		wantInt = true;
	}
	if ((reg[0x01] & 0x20) && (hpos >= lineTiming[VINT_HPOS_IDX]) &&
	    ((pendingStatus & STATUS_VINT) || (status & STATUS_VINT))) {
		wantInt = true;
	}
	nintAsserted = wantInt;
}

// MAME: check_pending_flags
void SMSVDPCore::checkPendingFlags(int hpos)
{
	if (!(pendingStatus & (STATUS_VINT | STATUS_SPROVR | STATUS_SPRCOL)) && !pendingHint) {
		return;
	}

	if (pendingHint && (hpos >= lineTiming[HINT_HPOS_IDX])) {
		pendingHint = false;
		hintOccurred = true;
	}
	if ((pendingStatus & STATUS_VINT) && (hpos >= lineTiming[VINT_FLAG_HPOS_IDX])) {
		pendingStatus &= ~STATUS_VINT;
		status |= STATUS_VINT;
	}
	if ((pendingStatus & STATUS_SPROVR) && (hpos >= lineTiming[SPROVR_HPOS_IDX])) {
		pendingStatus &= ~STATUS_SPROVR;
		status |= STATUS_SPROVR;
		// copy and reset the pending bits that were based on the number
		// of the first sprite that overflowed.
		status &= uint8_t(pendingStatus | (STATUS_VINT | STATUS_SPROVR | STATUS_SPRCOL));
		pendingStatus |= uint8_t(~(STATUS_VINT | STATUS_SPROVR | STATUS_SPRCOL));
	}
	if ((pendingStatus & STATUS_SPRCOL) && (hpos >= pendingSprcolX)) {
		pendingStatus &= ~STATUS_SPRCOL;
		status |= STATUS_SPRCOL;
		pendingSprcolX = 0;
	}
}

// MAME: data_read
uint8_t SMSVDPCore::readData()
{
	const uint8_t result = buffer;
	pendingControlWrite = false;
	buffer = vram[addr & 0x3fff];
	addr = uint16_t(addr + 1);
	return result;
}

// MAME: control_read
uint8_t SMSVDPCore::readControl(int hpos)
{
	checkPendingFlags(hpos);
	const uint8_t result = status;

	pendingControlWrite = false;
	hintOccurred = false;
	status = uint8_t(~(STATUS_VINT | STATUS_SPROVR | STATUS_SPRCOL));
	nintAsserted = false;
	return result;
}

// Side-effect-free version of readControl(): returns the status value a
// real read would produce at 'hpos', without promoting/clearing anything.
uint8_t SMSVDPCore::peekControl(int hpos) const
{
	uint8_t result = status;
	uint8_t pending = pendingStatus;
	if ((pending & STATUS_VINT) && (hpos >= lineTiming[VINT_FLAG_HPOS_IDX])) {
		pending &= uint8_t(~STATUS_VINT);
		result |= STATUS_VINT;
	}
	if ((pending & STATUS_SPROVR) && (hpos >= lineTiming[SPROVR_HPOS_IDX])) {
		pending &= uint8_t(~STATUS_SPROVR);
		result |= STATUS_SPROVR;
		result &= uint8_t(pending | (STATUS_VINT | STATUS_SPROVR | STATUS_SPRCOL));
		pending |= uint8_t(~(STATUS_VINT | STATUS_SPROVR | STATUS_SPRCOL));
	}
	if ((pending & STATUS_SPRCOL) && (hpos >= pendingSprcolX)) {
		result |= STATUS_SPRCOL;
	}
	return result;
}

// MAME: write_memory
void SMSVDPCore::writeMemory(uint8_t data)
{
	switch (addrmode) {
	case 0x00:
	case 0x01:
	case 0x02:
		vram[addr & 0x3fff] = data;
		break;
	case 0x03:
		cramWrite(data);
		break;
	}
	// data written to data port loads the data buffer
	buffer = data;
}

// MAME: data_write
void SMSVDPCore::writeData(uint8_t value)
{
	pendingControlWrite = false;
	writeMemory(value);
	addr = uint16_t(addr + 1);
}

// MAME: load_vram_addr
void SMSVDPCore::loadVramAddr(uint8_t data)
{
	// Seems like the latched data is passed straight through to the address
	// register when in the middle of doing a command (Cosmic Spacehead).
	if (pendingControlWrite) {
		addr = uint16_t((addr & 0xff00) | data);
	} else {
		addr = uint16_t((data << 8) | (addr & 0xff));
	}
}

// MAME: control_write
void SMSVDPCore::writeControl(uint8_t value, int hpos)
{
	if (!pendingControlWrite) {
		pendingControlWrite = true;
		loadVramAddr(value);
		return;
	}

	pendingControlWrite = false;
	addrmode = (value >> 6) & 0x03;
	loadVramAddr(value);
	switch (addrmode) {
	case 0: // VRAM reading mode
		buffer = vram[addr & 0x3fff];
		addr = uint16_t(addr + 1);
		break;
	case 1: // VRAM writing mode
		break;
	case 2: // VDP register write
		writeRegister(uint8_t(value & 0x0f), uint8_t(addr & 0xff), hpos);
		addrmode = 0;
		break;
	case 3: // CRAM writing mode
		break;
	}
}

void SMSVDPCore::writeRegister(uint8_t regNum, uint8_t value, int hpos)
{
	assert(regNum < 16);
	reg[regNum] = value;
	switch (regNum) {
	case 0:
		setDisplaySettings();
		break;
	case 1:
		setDisplaySettings();
		if (hpos <= DISPLAY_DISABLED_HPOS) {
			displayDisabled = !(reg[0x01] & 0x40);
		}
		break;
	case 8:
		if (hpos <= lineTiming[XSCROLL_HPOS_IDX]) {
			reg8copy = reg[0x08];
		}
		break;
	}

	checkPendingFlags(hpos);

	// MAME's special-case INT handling on register 0/1 writes
	// (see the comments in 315_5124.cpp for the test ROMs/games
	// that require this).
	if ((regNum == 0 && hintOccurred) || (regNum == 1 && (status & STATUS_VINT))) {
		nintAsserted = (regNum == 0) ? ((reg[0x00] & 0x10) != 0)
		                             : ((reg[0x01] & 0x20) != 0);
	}
}

// MAME: process_line_timer
void SMSVDPCore::processLine(int vpos)
{
	int vposLimit = frameTiming[VERTICAL_SYNC] + frameTiming[TOP_BLANKING] +
	                frameTiming[TOP_BORDER] + frameTiming[ACTIVE_DISPLAY_V] +
	                frameTiming[BOTTOM_BORDER] + frameTiming[BOTTOM_BLANKING];

	// copy current values in case they are not changed until latch time
	displayDisabled = !(reg[0x01] & 0x40);
	reg8copy = reg[0x08];

	// (The /CSYNC signal is not exposed to openMSX, so MAME's /CSYNC
	// handling is omitted here.)

	vposLimit -= frameTiming[BOTTOM_BLANKING];
	if (vpos >= vposLimit) { // below the bottom border
		lineCounter = reg[0x0a];
		return;
	}

	vposLimit -= frameTiming[BOTTOM_BORDER];
	if (vpos >= vposLimit) { // bottom border area
		if (vpos == vposLimit) {
			if (lineCounter == 0x00) {
				lineCounter = reg[0x0a];
				pendingHint = true;
			} else {
				--lineCounter;
			}
		} else {
			lineCounter = reg[0x0a];
		}

		// vposLimit + 1 because VINT fires at the end of the first logical
		// line of the bottom border.
		if (vpos == vposLimit + 1) {
			pendingStatus |= STATUS_VINT;
		}

		selectSprites(vpos - (vposLimit - frameTiming[ACTIVE_DISPLAY_V]));
		return;
	}

	vposLimit -= frameTiming[ACTIVE_DISPLAY_V];
	if (vpos >= vposLimit) { // active display area
		if (vpos == vposLimit) {
			reg9copy = reg[0x09];
		}
		if (lineCounter == 0x00) {
			lineCounter = reg[0x0a];
			pendingHint = true;
		} else {
			--lineCounter;
		}
		selectSprites(vpos - vposLimit);
		return;
	}

	vposLimit -= frameTiming[TOP_BORDER];
	if (vpos >= vposLimit) { // top border area
		lineCounter = reg[0x0a];
		if (vpos == vposLimit + frameTiming[TOP_BORDER] - 1) {
			hcounterLatched = false;
		}
		selectSprites(vpos - (vposLimit + frameTiming[TOP_BORDER]));
		return;
	}

	// vertical sync or top blanking areas
	lineCounter = reg[0x0a];
}

// MAME: update_palette
void SMSVDPCore::updatePalette()
{
	if (!cramDirty) {
		return;
	}
	cramDirty = false;
	if (vdpMode != 4) {
		for (int i = 0; i < 16; ++i) {
			currentPalette[i] = 64 + i;
		}
		return;
	}
	for (int i = 0; i < 32; ++i) {
		currentPalette[i] = cram[i] & 0x3f;
	}
}

// MAME: cram_write
void SMSVDPCore::cramWrite(uint8_t data)
{
	const unsigned address = addr & 0x1f;
	if (data != cram[address]) {
		cram[address] = data;
		cramDirty = true;
	}
}

// MAME: name_row_mode4, tile1_select_mode4, tile2_select_mode4,
//       sprite_attribute_extra_offset_mode4, sprite_tile_select_mode4
uint16_t SMSVDPCore::nameRowMode4(uint16_t row) const
{
	if (variant == Variant::SMS1) {
		return row & uint16_t(((reg[0x02] & 0x01) << 10) | 0x3bff);
	}
	return row;
}

uint16_t SMSVDPCore::tile1SelectMode4(uint16_t tileNumber) const
{
	if (variant == Variant::SMS1) {
		return tileNumber & uint16_t((reg[0x03] << 1) | 1);
	}
	return tileNumber;
}

uint16_t SMSVDPCore::tile2SelectMode4(uint16_t tileNumber) const
{
	if (variant == Variant::SMS1) {
		return tileNumber & uint16_t(((reg[0x04] & 0x07) << 6) | 0x03f);
	}
	return tileNumber;
}

uint8_t SMSVDPCore::spriteAttributeExtraOffsetMode4(uint8_t offset) const
{
	if (variant == Variant::SMS1) {
		return offset & uint8_t(((reg[0x05] & 0x01) << 7) | 0x7f);
	}
	return offset;
}

uint8_t SMSVDPCore::spriteTileSelectMode4(uint8_t tileNumber) const
{
	if (variant == Variant::SMS1) {
		return tileNumber & uint8_t(((reg[0x06] & 0x03) << 6) | 0x3f);
	}
	return tileNumber;
}

// MAME: draw_leftmost_pixels_mode4
void SMSVDPCore::drawLeftmostPixelsMode4(int* lineBuffer, int* prioritySelected,
                                         int fineXScroll, int paletteSelected, int tileLine)
{
	// To draw the leftmost pixels when they aren't part of a tile column
	// due to scrolling, the SMS has a weird behaviour to select which palette
	// will be used to obtain the color in entry 0, depending on the content
	// of tile 0x100 (or the palette selected by the next tile for the
	// Mega Drive, which is not implemented here).
	(void)paletteSelected;

	const int pixelX = 4;
	int parseLine = tileLine - 1;
	const int tileSelected = 0x100;
	const int tmpBitPlane1 = vram[((tileSelected << 5) + ((parseLine & 0x07) << 2) + 0x01) & 0x3fff];
	const uint8_t penBit1 = (tmpBitPlane1 >> (7 - pixelX)) & 1;

	for (int pixelPlotX = 0; pixelPlotX < fineXScroll; ++pixelPlotX) {
		lineBuffer[pixelPlotX] = currentPalette[penBit1 ? 0x10 : 0x00];
		prioritySelected[pixelPlotX] = 0;
	}
}

// MAME: draw_scanline_mode4
void SMSVDPCore::drawScanlineMode4(int* lineBuffer, int* prioritySelected, int line)
{
	const int xScroll = (((reg[0x00] & 0x40) != 0) && (line < 16)) ? 0 : reg8copy;
	const int xScrollStartColumn = 32 - (xScroll >> 3);
	const int fineXScroll = xScroll & 0x07;

	int scrollMod;
	uint16_t nameBase;
	if (yPixelsValue != 192) {
		nameBase = uint16_t(((reg[0x02] & 0x0c) << 10) | 0x0700);
		scrollMod = 256;
	} else {
		nameBase = uint16_t((reg[0x02] << 10) & 0x3800);
		scrollMod = 224;
	}

	for (int tileColumn = 0; tileColumn < 32; ++tileColumn) {
		const int tableColumn = ((tileColumn + xScrollStartColumn) & 0x1f) << 1;

		const int yScroll = (((reg[0x00] & 0x80) != 0) && (tileColumn > 23)) ? 0 : reg9copy;

		const uint16_t row = uint16_t((((line + yScroll) % scrollMod) >> 3) << 6);
		const uint16_t nameRow = nameRowMode4(row);
		const unsigned address = (nameBase + nameRow + tableColumn) & 0x3fff;
		const uint16_t tileData = uint16_t(vram[address] | (vram[(address + 1) & 0x3fff] << 8));

		const int tile1Selected = tile1SelectMode4(tileData & 0x01ff);
		const int tile2Selected = tile2SelectMode4(tileData & 0x01ff);
		const int prioritySelect = tileData & 0x1000;
		const int paletteSelected = (tileData >> 11) & 1;
		const int vertSelected = (tileData >> 10) & 1;
		const int horizSelected = (tileData >> 9) & 1;

		int tileLine = line - ((0x07 - (yScroll & 0x07)) + 1);
		if (vertSelected) {
			tileLine = 0x07 - tileLine;
		}

		const int addr0 = ((tile1Selected << 5) + ((tileLine & 0x07) << 2));
		const uint8_t bitPlane0 = vram[(addr0 + 0) & 0x3fff];
		const uint8_t bitPlane1 = vram[(addr0 + 1) & 0x3fff];
		const int addr2 = ((tile2Selected << 5) + ((tileLine & 0x07) << 2));
		const uint8_t bitPlane2 = vram[(addr2 + 2) & 0x3fff];
		const uint8_t bitPlane3 = vram[(addr2 + 3) & 0x3fff];

		if ((tileColumn == 0) && (fineXScroll > 0)) {
			drawLeftmostPixelsMode4(lineBuffer, prioritySelected, fineXScroll,
			                        paletteSelected, tileLine);
		}

		for (int pixelX = 0; pixelX < 8; ++pixelX) {
			const uint8_t penBit0 = (bitPlane0 >> (7 - pixelX)) & 1;
			const uint8_t penBit1 = (bitPlane1 >> (7 - pixelX)) & 1;
			const uint8_t penBit2 = (bitPlane2 >> (7 - pixelX)) & 1;
			const uint8_t penBit3 = (bitPlane3 >> (7 - pixelX)) & 1;
			uint8_t penSelected = uint8_t((penBit3 << 3) | (penBit2 << 2) |
			                              (penBit1 << 1) | penBit0);
			if (paletteSelected) {
				penSelected |= 0x10;
			}

			int pixelPlotX = !horizSelected ? pixelX : (7 - pixelX);
			pixelPlotX = fineXScroll + (tileColumn << 3) + pixelPlotX;
			if (pixelPlotX < 256) {
				lineBuffer[pixelPlotX] = currentPalette[penSelected];
				prioritySelected[pixelPlotX] = prioritySelect | (penSelected & 0x0f);
			}
		}
	}
}

// MAME: draw_scanline_mode0 (Graphics I)
void SMSVDPCore::drawScanlineMode0(int* lineBuffer, int line)
{
	const uint16_t nameBase = uint16_t((reg[0x02] & 0x0f) << 10);
	const uint16_t colorBase = uint16_t((reg[0x03] << 6) & 0x3fff);
	const uint16_t patternBase = uint16_t((reg[0x04] << 11) & 0x3fff);
	const uint16_t nameRowBase = uint16_t(nameBase + ((line >> 3) * 32));

	for (int tileColumn = 0; tileColumn < 32; ++tileColumn) {
		const uint8_t name = vram[(nameRowBase + tileColumn) & 0x3fff];
		const uint8_t pattern = vram[(patternBase + (name << 3) + (line & 0x07)) & 0x3fff];
		const uint8_t colors = vram[(colorBase + (name >> 3)) & 0x3fff];

		for (int pixelX = 0; pixelX < 8; ++pixelX) {
			const int pixelPlotX = (tileColumn << 3) + pixelX;
			int penSelected;
			if ((pattern >> (7 - pixelX)) & 1) {
				penSelected = colors >> 4;
			} else {
				penSelected = colors & 0x0f;
			}
			if (!penSelected) {
				penSelected = backdropColor();
			}
			lineBuffer[pixelPlotX] = currentPalette[penSelected];
		}
	}
}

// MAME: draw_scanline_mode1 (Text)
void SMSVDPCore::drawScanlineMode1(int* lineBuffer, int line)
{
	const uint16_t nameBase = uint16_t((reg[0x02] & 0x0f) << 10);
	const uint16_t patternBase = uint16_t((reg[0x04] << 11) & 0x3fff);
	const uint16_t nameRowBase = uint16_t(nameBase + ((line >> 3) * 32));

	for (int pixelPlotX = 0; pixelPlotX < 8; ++pixelPlotX) {
		lineBuffer[pixelPlotX] = currentPalette[backdropColor()];
	}

	for (int tileColumn = 0; tileColumn < 40; ++tileColumn) {
		const uint8_t name = vram[(nameRowBase + tileColumn) & 0x3fff];
		const uint8_t pattern = vram[(patternBase + (name << 3) + (line & 0x07)) & 0x3fff];

		for (int pixelX = 0; pixelX < 6; ++pixelX) {
			const int pixelPlotX = (tileColumn * 6) + pixelX + 8;
			int penSelected;
			if ((pattern >> (7 - pixelX)) & 1) {
				penSelected = reg[0x07] >> 4;
			} else {
				penSelected = reg[0x07] & 0x0f;
			}
			if (!penSelected) {
				penSelected = backdropColor();
			}
			lineBuffer[pixelPlotX] = currentPalette[penSelected];
		}
	}

	for (int pixelPlotX = 248; pixelPlotX < 256; ++pixelPlotX) {
		lineBuffer[pixelPlotX] = currentPalette[backdropColor()];
	}
}

// MAME: draw_scanline_mode2 (Graphics II)
void SMSVDPCore::drawScanlineMode2(int* lineBuffer, int line)
{
	const uint16_t nameBase = uint16_t((reg[0x02] & 0x0f) << 10);
	const uint16_t colorBase = uint16_t((reg[0x03] & 0x80) << 6);
	const int colorMask = ((reg[0x03] & 0x7f) << 3) | 0x07;
	const uint16_t patternBase = uint16_t((reg[0x04] & 0x04) << 11);
	const int patternMask = ((reg[0x04] & 0x03) << 8) | 0xff;
	const int patternOffset = (line & 0xc0) << 2;
	const uint16_t nameRowBase = uint16_t(nameBase + ((line >> 3) * 32));

	for (int tileColumn = 0; tileColumn < 32; ++tileColumn) {
		const uint8_t name = vram[(nameRowBase + tileColumn) & 0x3fff];
		const uint8_t pattern = vram[(patternBase + (((patternOffset + name) & patternMask) * 8) + (line & 0x07)) & 0x3fff];
		const uint8_t colors = vram[(colorBase + (((patternOffset + name) & colorMask) * 8) + (line & 0x07)) & 0x3fff];

		for (int pixelX = 0; pixelX < 8; ++pixelX) {
			const int pixelPlotX = (tileColumn << 3) + pixelX;
			uint8_t penSelected;
			if ((pattern >> (7 - pixelX)) & 1) {
				penSelected = colors >> 4;
			} else {
				penSelected = colors & 0x0f;
			}
			if (!penSelected) {
				penSelected = backdropColor();
			}
			lineBuffer[pixelPlotX] = currentPalette[penSelected];
		}
	}
}

// MAME: draw_scanline_mode3 (Multicolor)
void SMSVDPCore::drawScanlineMode3(int* lineBuffer, int line)
{
	const uint16_t nameBase = uint16_t((reg[0x02] & 0x0f) << 10);
	const uint16_t patternBase = uint16_t((reg[0x04] << 11) & 0x3fff);
	const uint16_t nameRowBase = uint16_t(nameBase + ((line >> 3) * 32));

	for (int tileColumn = 0; tileColumn < 32; ++tileColumn) {
		const uint8_t name = vram[(nameRowBase + tileColumn) & 0x3fff];
		const uint8_t pattern = vram[(patternBase + (name << 3) + (((line >> 3) & 3) << 1) + ((line & 4) >> 2)) & 0x3fff];

		for (int pixelX = 0; pixelX < 8; ++pixelX) {
			const int pixelPlotX = (tileColumn << 3) + pixelX;
			uint8_t penSelected = (pattern >> ((~pixelX) & 4)) & 0x0f;
			if (!penSelected) {
				penSelected = backdropColor();
			}
			lineBuffer[pixelPlotX] = currentPalette[penSelected];
		}
	}
}

// MAME: select_sprites
void SMSVDPCore::selectSprites(int line)
{
	spriteCount = 0;
	if (vdpMode == 1) {
		// Text mode, no sprite processing
		return;
	}

	spriteHeight = (reg[0x01] & 0x02) ? 16 : 8;
	spriteZoomScale = (reg[0x01] & 0x01) ? 2 : 1;

	if (vdpMode < 4) {
		// TMS9918 compatibility sprites
		const int maxSprites = 4;
		spriteAttributeBase = uint16_t((reg[0x05] & 0x7f) << 7);

		for (int spriteIndex = 0; spriteIndex < 32 * 4; spriteIndex += 4) {
			// At this point the VDP vcount still doesn't refer the new line,
			// because the logical start point is slightly shifted on the scanline
			int parseLine = line - 1;

			int spriteY = vram[(spriteAttributeBase + spriteIndex) & 0x3fff];
			if (spriteY == 0xd0) {
				break;
			}
			if (spriteY >= 240) {
				spriteY -= 256; // wrap from top if y position is >= 240
			}
			if ((spriteZoomScale > 1) && (spriteCount < maxSpriteZoomVcount)) {
				// Divide before use the value for comparison, or else an
				// off-by-one bug could occur, as seen with Tarzan on GG.
				parseLine >>= 1;
				spriteY >>= 1;
			}
			if ((parseLine >= spriteY) && (parseLine < (spriteY + spriteHeight))) {
				if (spriteCount < maxSprites) {
					const int spriteXPos = vram[(spriteAttributeBase + spriteIndex + 1) & 0x3fff];
					int tileSelected = vram[(spriteAttributeBase + spriteIndex + 2) & 0x3fff];
					const uint8_t flags = vram[(spriteAttributeBase + spriteIndex + 3) & 0x3fff];
					int spriteLine = parseLine - spriteY;

					if (spriteHeight == 16) {
						tileSelected &= 0xfc;
						if (spriteLine > 0x07) {
							tileSelected += 1;
							spriteLine -= 8;
						}
					}

					spriteX[spriteCount] = spriteXPos;
					spriteTileSelected[spriteCount] = tileSelected;
					spriteFlags[spriteCount] = flags;
					spritePatternLine[spriteCount] = uint16_t(((reg[0x06] & 0x07) << 11) + spriteLine);
					++spriteCount;
				} else {
					spriteCountOverflow(line, spriteIndex);
				}
			}
		}
	} else {
		// Regular sprites
		const int maxSprites = 8;
		spriteAttributeBase = uint16_t((reg[0x05] << 7) & 0x3f00);
		const uint16_t spriteAttributeExtraBase = uint16_t(
			spriteAttributeBase + spriteAttributeExtraOffsetMode4(0x80));

		for (int spriteIndex = 0; spriteIndex < 64; ++spriteIndex) {
			int parseLine = line - 1;
			int spriteY = vram[(spriteAttributeBase + spriteIndex) & 0x3fff];
			if ((yPixelsValue == 192) && (spriteY == 0xd0)) {
				break;
			}
			if (spriteY >= 240) {
				spriteY -= 256;
			}
			if ((spriteZoomScale > 1) && (spriteCount < maxSpriteZoomVcount)) {
				parseLine >>= 1;
				spriteY >>= 1;
			}
			if ((parseLine >= spriteY) && (parseLine < (spriteY + spriteHeight))) {
				if (spriteCount < maxSprites) {
					const int spriteLine = parseLine - spriteY;
					int spriteXPos = vram[(spriteAttributeExtraBase + (spriteIndex << 1)) & 0x3fff];
					const int spriteTileNumber = vram[(spriteAttributeExtraBase + (spriteIndex << 1) + 1) & 0x3fff];

					int tileSelected = spriteTileSelectMode4(uint8_t(spriteTileNumber));

					if (reg[0x00] & 0x08) {
						spriteXPos -= 0x08; // sprite shift
					}
					if (reg[0x06] & 0x04) {
						tileSelected += 256; // pattern table select
					}
					if (spriteHeight == 16) {
						tileSelected &= 0x01fe; // force even index
					}
					if (spriteLine > 0x07) {
						tileSelected += 1;
					}

					spriteX[spriteCount] = spriteXPos;
					spriteTileSelected[spriteCount] = tileSelected;
					spritePatternLine[spriteCount] = uint16_t((spriteLine & 0x07) << 2);
					++spriteCount;
				} else {
					spriteCountOverflow(line, spriteIndex);
				}
			}
		}
	}
}

// MAME: sprite_count_overflow
void SMSVDPCore::spriteCountOverflow(int line, int spriteIndex)
{
	// Overflow is flagged only on active display and when VINT isn't active
	if (!(status & STATUS_VINT) && (line >= 0) && (line < frameTiming[ACTIVE_DISPLAY_V])) {
		uint8_t spriteNumberBits;
		pendingStatus |= STATUS_SPROVR;
		if (spriteIndex < 14) {
			spriteNumberBits = uint8_t((spriteIndex + 1) / 2);
		} else {
			spriteNumberBits = uint8_t(spriteIndex / 2);
		}
		pendingStatus &= uint8_t(spriteNumberBits | (STATUS_VINT | STATUS_SPROVR | STATUS_SPRCOL));
	}
}

// MAME: sprite_collision
void SMSVDPCore::spriteCollision(int /*line*/, int spriteColX)
{
	// SMS/GG: collisions don't occur on column 0 if it is disabled.
	if ((reg[0x00] & 0x20) && (spriteColX < 8)) {
		return;
	}
	pendingStatus |= STATUS_SPRCOL;
	pendingSprcolX = lineTiming[SPRCOL_BASEHPOS_IDX] + spriteColX;
}

// MAME: draw_sprites_mode4
void SMSVDPCore::drawSpritesMode4(int* lineBuffer, int* prioritySelected, int line)
{
	if (displayDisabled || (spriteCount == 0)) {
		return;
	}

	bool spriteColOccurred = false;
	int spriteColX = 255;
	std::array<bool, 256> collisionBuffer{};

	for (int spriteBufferIndex = spriteCount - 1; spriteBufferIndex >= 0; --spriteBufferIndex) {
		const int spriteXPos = spriteX[spriteBufferIndex];
		const int tileSelected = spriteTileSelected[spriteBufferIndex];
		const uint16_t patternLine = spritePatternLine[spriteBufferIndex];
		const int zoomScale = (spriteBufferIndex < maxSpriteZoomHcount) ? spriteZoomScale : 1;

		const unsigned patternAddr = (tileSelected << 5) + patternLine;
		const uint8_t bitPlane0 = vram[(patternAddr + 0) & 0x3fff];
		const uint8_t bitPlane1 = vram[(patternAddr + 1) & 0x3fff];
		const uint8_t bitPlane2 = vram[(patternAddr + 2) & 0x3fff];
		const uint8_t bitPlane3 = vram[(patternAddr + 3) & 0x3fff];

		for (int pixelX = 0; pixelX < 8; ++pixelX) {
			const uint8_t penBit0 = (bitPlane0 >> (7 - pixelX)) & 1;
			const uint8_t penBit1 = (bitPlane1 >> (7 - pixelX)) & 1;
			const uint8_t penBit2 = (bitPlane2 >> (7 - pixelX)) & 1;
			const uint8_t penBit3 = (bitPlane3 >> (7 - pixelX)) & 1;
			const uint8_t penSelected = uint8_t((penBit3 << 3) | (penBit2 << 2) |
			                                    (penBit1 << 1) | penBit0 | 0x10);

			if (penSelected == 0x10) { // transparent, skip
				continue;
			}

			int pixelPlotX;
			if (zoomScale > 1) {
				pixelPlotX = spriteXPos + (pixelX << 1);
			} else {
				pixelPlotX = spriteXPos + pixelX;
			}

			for (int zoom = 0; zoom < zoomScale; ++zoom) {
				pixelPlotX += zoom;
				if (pixelPlotX < 0 || pixelPlotX > 255) {
					continue;
				}
				if (!(prioritySelected[pixelPlotX] & 0x1000)) {
					lineBuffer[pixelPlotX] = currentPalette[penSelected];
					prioritySelected[pixelPlotX] = penSelected;
				} else if (prioritySelected[pixelPlotX] == 0x1000) {
					lineBuffer[pixelPlotX] = currentPalette[penSelected];
					prioritySelected[pixelPlotX] = penSelected;
				}
				if (!collisionBuffer[pixelPlotX]) {
					collisionBuffer[pixelPlotX] = true;
				} else {
					spriteColOccurred = true;
					spriteColX = std::min(spriteColX, pixelPlotX);
				}
			}
		}

		if (spriteColOccurred) {
			spriteCollision(line, spriteColX);
		}
	}
}

// MAME: draw_sprites_tms9918_mode
void SMSVDPCore::drawSpritesTms9918Mode(int* lineBuffer, int line)
{
	if (displayDisabled || (spriteCount == 0)) {
		return;
	}

	bool spriteColOccurred = false;
	int spriteColX = 255;
	std::array<bool, 256> collisionBuffer{};

	for (int spriteBufferIndex = spriteCount - 1; spriteBufferIndex >= 0; --spriteBufferIndex) {
		int spriteXPos = spriteX[spriteBufferIndex];
		int tileSelected = spriteTileSelected[spriteBufferIndex];
		const uint16_t patternLine = spritePatternLine[spriteBufferIndex];
		const uint8_t flags = spriteFlags[spriteBufferIndex];
		const int penSelected = (flags & 0x0f); // palette offset is 0
		const int zoomScale = (spriteBufferIndex < maxSpriteZoomHcount) ? spriteZoomScale : 1;

		if (flags & 0x80) {
			spriteXPos -= 32;
		}

		for (int spriteHeightPart = 8; spriteHeightPart <= spriteHeight; spriteHeightPart += 8) {
			if (spriteHeightPart == 16) {
				tileSelected += 2;
				spriteXPos += (zoomScale > 1 ? 16 : 8);
			}

			const uint8_t pattern = vram[(patternLine + tileSelected * 8) & 0x3fff];

			for (int pixelX = 0; pixelX < 8; ++pixelX) {
				if (penSelected && ((pattern >> (7 - pixelX)) & 1)) {
					int pixelPlotX;
					if (zoomScale > 1) {
						pixelPlotX = spriteXPos + (pixelX << 1);
					} else {
						pixelPlotX = spriteXPos + pixelX;
					}

					for (int zoom = 0; zoom < zoomScale; ++zoom) {
						pixelPlotX += zoom;
						if (pixelPlotX < 0 || pixelPlotX > 255) {
							continue;
						}
						lineBuffer[pixelPlotX] = currentPalette[penSelected];
						if (collisionBuffer[pixelPlotX] != 1) {
							collisionBuffer[pixelPlotX] = 1;
						} else {
							spriteColOccurred = true;
							spriteColX = std::min(spriteColX, pixelPlotX);
						}
					}
				}
			}
		}

		if (spriteColOccurred) {
			spriteCollision(line, spriteColX);
		}
	}
}

// MAME: draw_scanline + blit_scanline, but rendering into a fixed
// 320x240 output frame (active 256 pixels centered, active lines centered).
void SMSVDPCore::drawLine(int outLine, std::span<uint32_t, OUTPUT_WIDTH> pixels)
{
	updatePalette();

	const uint32_t backdrop = penColor(backdropColor());
	std::fill(pixels.begin(), pixels.end(), backdrop);

	const int line = outLine - ((OUTPUT_HEIGHT - yPixelsValue) / 2);

	if (displayDisabled) {
		return;
	}

	if (line < frameTiming[ACTIVE_DISPLAY_V]) {
		std::array<int, 256> lineBuffer{};
		std::array<int, 256> prioritySelected;
		std::fill(prioritySelected.begin(), prioritySelected.end(), 1);

		switch (vdpMode) {
		case 0:
			if (line >= 0) drawScanlineMode0(lineBuffer.data(), line);
			if ((line >= 0) || ((line >= -13) && (yPixelsValue == 192)))
				drawSpritesTms9918Mode(lineBuffer.data(), line);
			break;
		case 1:
			if (line >= 0) drawScanlineMode1(lineBuffer.data(), line);
			// Text mode, no sprite drawing
			break;
		case 2:
			if (line >= 0) drawScanlineMode2(lineBuffer.data(), line);
			if ((line >= 0) || ((line >= -13) && (yPixelsValue == 192)))
				drawSpritesTms9918Mode(lineBuffer.data(), line);
			break;
		case 3:
			if (line >= 0) drawScanlineMode3(lineBuffer.data(), line);
			if ((line >= 0) || ((line >= -13) && (yPixelsValue == 192)))
				drawSpritesTms9918Mode(lineBuffer.data(), line);
			break;
		case 4:
		default:
			if (line >= 0) drawScanlineMode4(lineBuffer.data(), prioritySelected.data(), line);
			if ((line >= 0) || ((line >= -13) && (yPixelsValue == 192)))
				drawSpritesMode4(lineBuffer.data(), prioritySelected.data(), line);
			break;
		}

		if ((line >= 0) && (line < frameTiming[ACTIVE_DISPLAY_V])) {
			int x = 0;
			if ((vdpMode == 4) && (reg[0x00] & 0x20)) {
				// Fill column 0 with overscan color
				do {
					pixels[32 + x] = penColor(backdropColor());
				} while (++x < 8);
			}
			do {
				pixels[32 + x] = colorAt(lineBuffer[x]);
			} while (++x < 256);
		}
	}
}

} // namespace openmsx
