#include "V9990.hh"
#include "Display.hh"
#include "RendererFactory.hh"
#include "V9990Renderer.hh"
#include "Reactor.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <array>
#include <cassert>
#include <memory>

namespace openmsx {

static constexpr byte ALLOW_READ  = 1;
static constexpr byte ALLOW_WRITE = 2;
static constexpr byte NO_ACCESS = 0;
static constexpr byte RD_ONLY   = ALLOW_READ;
static constexpr byte WR_ONLY   = ALLOW_WRITE;
static constexpr byte RD_WR     = ALLOW_READ | ALLOW_WRITE;
static constexpr std::array<byte, 64> regAccess = {
	WR_ONLY, WR_ONLY, WR_ONLY,          // VRAM Write Address
	WR_ONLY, WR_ONLY, WR_ONLY,          // VRAM Read Address
	RD_WR, RD_WR,                       // Screen Mode
	RD_WR,                              // Control
	RD_WR, RD_WR, RD_WR, RD_WR,         // Interrupt
	WR_ONLY,                            // Palette Control
	WR_ONLY,                            // Palette Pointer
	RD_WR,                              // Back Drop Color
	RD_WR,                              // Display Adjust
	RD_WR, RD_WR, RD_WR, RD_WR,         // Scroll Control A
	RD_WR, RD_WR, RD_WR, RD_WR,         // Scroll Control B
	RD_WR,                              // Sprite Pattern Table Address
	RD_WR,                              // LCD Control
	RD_WR,                              // Priority Control
	WR_ONLY,                            // Sprite Palette Control
	NO_ACCESS, NO_ACCESS, NO_ACCESS,    // 3x not used
	WR_ONLY, WR_ONLY, WR_ONLY, WR_ONLY, // Cmd Parameter Src XY
	WR_ONLY, WR_ONLY, WR_ONLY, WR_ONLY, // Cmd Parameter Dest XY
	WR_ONLY, WR_ONLY, WR_ONLY, WR_ONLY, // Cmd Parameter Size XY
	WR_ONLY, WR_ONLY, WR_ONLY, WR_ONLY, // Cmd Parameter Arg, LogOp, WrtMask
	WR_ONLY, WR_ONLY, WR_ONLY, WR_ONLY, // Cmd Parameter Font Color
	WR_ONLY, RD_ONLY, RD_ONLY,          // Cmd Parameter OpCode, Border X
	NO_ACCESS, NO_ACCESS, NO_ACCESS,    // registers 55-63
	NO_ACCESS, NO_ACCESS, NO_ACCESS,
	NO_ACCESS, NO_ACCESS, NO_ACCESS
};

// -------------------------------------------------------------------------
// Constructor & Destructor
// -------------------------------------------------------------------------

V9990::V9990(const DeviceConfig& config)
	: MSXDevice(config)
	, syncVSync(*this)
	, syncDisplayStart(*this)
	, syncVScan(*this)
	, syncHScan(*this)
	, syncSetMode(*this)
	, syncCmdEnd(*this)
	, v9990RegDebug(*this)
	, v9990PalDebug(*this)
	, irq(getMotherBoard(), getName() + ".IRQ")
	, display(getReactor().getDisplay())
	, vram(*this, getCurrentTime())
	, cmdEngine(*this, getCurrentTime(), display.getRenderSettings())
	, frameStartTime(getCurrentTime())
	, hScanSyncTime(getCurrentTime())
	, pendingIRQs(0)
	, externalVideoSource(false)
{
	// clear regs TODO find realistic init values
	ranges::fill(regs, 0);
	setDisplayMode(calcDisplayMode());

	// initialize palette
	for (auto i : xrange(64)) {
		palette[4 * i + 0] = 0x9F;
		palette[4 * i + 1] = 0x1F;
		palette[4 * i + 2] = 0x1F;
		palette[4 * i + 3] = 0x00;
	}

	vram.setCmdEngine(cmdEngine);

	// Start with NTSC timing
	palTiming = false;
	interlaced = false;
	setVerticalTiming();

	// Initialise rendering system
	isDisplayArea = false;
	displayEnabled = false; // avoid UMR (used by createRenderer())
	superimposing  = false; // avoid UMR
	EmuTime::param time = getCurrentTime();
	createRenderer(time);

	powerUp(time);
	display.attach(*this);
}

V9990::~V9990()
{
	display.detach(*this);
}

PostProcessor* V9990::getPostProcessor() const
{
	return renderer->getPostProcessor();
}

// -------------------------------------------------------------------------
// MSXDevice
// -------------------------------------------------------------------------

void V9990::powerUp(EmuTime::param time)
{
	vram.clear();
	reset(time);
}

void V9990::reset(EmuTime::param time)
{
	syncVSync       .removeSyncPoint();
	syncDisplayStart.removeSyncPoint();
	syncVScan       .removeSyncPoint();
	syncHScan       .removeSyncPoint();
	syncSetMode     .removeSyncPoint();
	syncCmdEnd      .removeSyncPoint();

	// Clear registers / ports
	ranges::fill(regs, 0);
	status = 0;
	regSelect = 0xFF; // TODO check value for power-on and reset
	vramWritePtr = 0;
	vramReadPtr = 0;
	vramReadBuffer = 0;
	systemReset = false; // verified on real MSX
	setDisplayMode(calcDisplayMode());

	isDisplayArea = false;
	displayEnabled = false;
	superimposing = false;

	// Reset IRQs
	writeIO(INTERRUPT_FLAG, 0xFF, time);

	palTiming = false;
	// Reset sub-systems
	cmdEngine.sync(time);
	renderer->reset(time);
	cmdEngine.reset(time);

	// Init scheduling
	frameStart(time);
}

byte V9990::readIO(word port, EmuTime::param time)
{
	port &= 0x0F;

	// calculate return value (mostly uses peekIO)
	byte result = [&] {
		switch (port) {
		case COMMAND_DATA:
			return cmdEngine.getCmdData(time);
		case VRAM_DATA:
		case PALETTE_DATA:
		case REGISTER_DATA:
		case INTERRUPT_FLAG:
		case STATUS:
		case KANJI_ROM_0:
		case KANJI_ROM_1:
		case KANJI_ROM_2:
		case KANJI_ROM_3:
		case REGISTER_SELECT:
		case SYSTEM_CONTROL:
		default:
			return peekIO(port, time);
		}
	}();
	// TODO verify this, especially REGISTER_DATA
	if (systemReset) return result; // no side-effects

	// execute side-effects
	switch (port) {
	case VRAM_DATA:
		if (!(regs[VRAM_READ_ADDRESS_2] & 0x80)) {
			vramReadPtr = getVRAMAddr(VRAM_READ_ADDRESS_0) + 1;
			setVRAMAddr(VRAM_READ_ADDRESS_0, vramReadPtr);
			// Update read buffer. TODO: timing?
			vramReadBuffer = vram.readVRAMCPU(vramReadPtr, time);
		}
		break;

	case PALETTE_DATA:
		if (!(regs[PALETTE_CONTROL] & 0x10)) {
			byte& palPtr = regs[PALETTE_POINTER];
			switch (palPtr & 3) {
			case 0:  palPtr += 1; break; // red
			case 1:  palPtr += 1; break; // green
			case 2:  palPtr += 2; break; // blue
			default: palPtr -= 3; break; // checked on real V9990
			}
		}
		break;

	case REGISTER_DATA:
		if (!(regSelect & 0x40)) {
			//regSelect = ( regSelect      & 0xC0) |
			//            ((regSelect + 1) & 0x3F);
			regSelect = (regSelect + 1) & ~0x40;
		}
		break;
	}
	return result;
}

byte V9990::peekIO(word port, EmuTime::param time) const
{
	switch (port & 0x0F) {
	case VRAM_DATA: {
		// TODO in 'systemReset' mode, this seems to hang the MSX
		// V9990 fetches from read buffer instead of directly from VRAM.
		// The read buffer is the reason why it is impossible to fill
		// vram by copying a block from "addr" to "addr+1".
		return vramReadBuffer;
	}
	case PALETTE_DATA:
		return palette[regs[PALETTE_POINTER]];

	case COMMAND_DATA:
		return cmdEngine.peekCmdData(time);

	case REGISTER_DATA:
		return readRegister(regSelect & 0x3F, time);

	case INTERRUPT_FLAG:
		return pendingIRQs;

	case STATUS: {
		unsigned left   = getLeftBorder();
		unsigned right  = getRightBorder();
		unsigned top    = getTopBorder();
		unsigned bottom = getBottomBorder();
		unsigned ticks = getUCTicksThisFrame(time);
		unsigned x = ticks % V9990DisplayTiming::UC_TICKS_PER_LINE;
		unsigned y = ticks / V9990DisplayTiming::UC_TICKS_PER_LINE;
		bool hr = (x < left) || (right  <= x);
		bool vr = (y < top)  || (bottom <= y);

		return cmdEngine.getStatus(time) |
		       (vr ? 0x40 : 0x00) |
		       (hr ? 0x20 : 0x00) |
		       (status & 0x06);
	}
	case KANJI_ROM_1:
	case KANJI_ROM_3:
		// not used in Gfx9000
		return 0xFF; // TODO check

	case REGISTER_SELECT:
	case SYSTEM_CONTROL:
	case KANJI_ROM_0:
	case KANJI_ROM_2:
	default:
		// write-only
		return 0xFF;
	}
}

void V9990::writeIO(word port, byte val, EmuTime::param time)
{
	port &= 0x0F;
	switch (port) {
		case VRAM_DATA: {
			// write VRAM
			if (systemReset) {
				// TODO writes in systemReset mode seem to have
				//  'some' effect but it's not immediately clear
				//  what the exact behaviour is
				return;
			}
			unsigned addr = getVRAMAddr(VRAM_WRITE_ADDRESS_0);
			vram.writeVRAMCPU(addr, val, time);
			if (!(regs[VRAM_WRITE_ADDRESS_2] & 0x80)) {
				setVRAMAddr(VRAM_WRITE_ADDRESS_0, addr + 1);
			}
			break;
		}
		case PALETTE_DATA: {
			if (systemReset) {
				// Equivalent to writing 0 and keeping palPtr = 0
				// The above interpretation makes it similar to
				// writes to REGISTER_DATA, REGISTER_SELECT.
				writePaletteRegister(0, 0, time);
				return;
			}
			byte& palPtr = regs[PALETTE_POINTER];
			writePaletteRegister(palPtr, val, time);
			switch (palPtr & 3) {
				case 0:  palPtr += 1; break; // red
				case 1:  palPtr += 1; break; // green
				case 2:  palPtr += 2; break; // blue
				default: palPtr -= 3; break; // checked on real V9990
			}
			break;
		}
		case COMMAND_DATA:
			// systemReset state doesn't matter:
			//   command below has no effect in systemReset mode
			//assert(cmdEngine);
			cmdEngine.setCmdData(val, time);
			break;

		case REGISTER_DATA: {
			// write register
			if (systemReset) {
				// In systemReset mode, write has no effect,
				// but 'regSelect' is increased.
				// I don't know if write is ignored or a write
				// with val=0 is executed. Though both have the
				// same effect and the latter is more in line
				// with writes to PALETTE_DATA and
				// REGISTER_SELECT.
				val = 0;
			}
			writeRegister(regSelect & 0x3F, val, time);
			if (!(regSelect & 0x80)) {
				regSelect = ( regSelect      & 0xC0) |
				            ((regSelect + 1) & 0x3F);
			}
			break;
		}
		case REGISTER_SELECT:
			if (systemReset) {
				// Tested on real MSX. Also when no write is done
				// to this port, regSelect is not RESET when
				// entering/leaving systemReset mode.
				// This behavior is similar to PALETTE_DATA and
				// REGISTER_DATA.
				val = 0;
			}
			regSelect = val;
			break;

		case STATUS:
			// read-only, ignore writes
			break;

		case INTERRUPT_FLAG:
			// systemReset state doesn't matter:
			//   stuff below has no effect in systemReset mode
			pendingIRQs &= ~val;
			if (!(pendingIRQs & regs[INTERRUPT_0])) {
				irq.reset();
			}
			scheduleHscan(time);
			break;

		case SYSTEM_CONTROL: {
			// TODO investigate: does switching overscan mode
			//      happen at next line or next frame
			status = (status & 0xFB) | ((val & 1) << 2);
			syncAtNextLine(syncSetMode, time);

			bool newSystemReset = (val & 2) != 0;
			if (newSystemReset != systemReset) {
				systemReset = newSystemReset;
				if (systemReset) {
					// Enter systemReset mode
					//   Verified on real MSX: palette data
					//   and VRAM content are NOT reset.
					for (auto i : xrange(64)) {
						writeRegister(i, 0, time);
					}
					// TODO verify IRQ behaviour
					writeIO(INTERRUPT_FLAG, 0xFF, time);
				}
			}
			break;
		}
		case KANJI_ROM_0:
		case KANJI_ROM_1:
		case KANJI_ROM_2:
		case KANJI_ROM_3:
			// not used in Gfx9000, ignore
			break;

		default:
			// ignore
			break;
	}
}

// =========================================================================
// Private stuff
// =========================================================================

// -------------------------------------------------------------------------
// Schedulable
// -------------------------------------------------------------------------

void V9990::execVSync(EmuTime::param time)
{
	// Transition from one frame to the next
	renderer->frameEnd(time);
	frameStart(time);
}

void V9990::execDisplayStart(EmuTime::param time)
{
	if (displayEnabled) {
		renderer->updateDisplayEnabled(true, time);
	}
	isDisplayArea = true;
}

void V9990::execVScan(EmuTime::param time)
{
	if (isDisplayEnabled()) {
		renderer->updateDisplayEnabled(false, time);
	}
	isDisplayArea = false;
	raiseIRQ(VER_IRQ);
}

void V9990::execHScan()
{
	raiseIRQ(HOR_IRQ);
}

void V9990::execSetMode(EmuTime::param time)
{
	auto newMode = calcDisplayMode();
	renderer->setDisplayMode(newMode, time);
	renderer->setColorMode(getColorMode(), time);
	setDisplayMode(newMode);
}

void V9990::execCheckCmdEnd(EmuTime::param time)
{
	cmdEngine.sync(time);
	scheduleCmdEnd(time); // in case of underestimation
}

void V9990::scheduleCmdEnd(EmuTime::param time)
{
	if (regs[INTERRUPT_0] & 4) {
		auto next = cmdEngine.estimateCmdEnd();
		if (next > time) {
			syncCmdEnd.setSyncPoint(next);
		}
	}
}

// -------------------------------------------------------------------------
// VideoSystemChangeListener
// -------------------------------------------------------------------------

void V9990::preVideoSystemChange() noexcept
{
	renderer.reset();
}

void V9990::postVideoSystemChange() noexcept
{
	EmuTime::param time = getCurrentTime();
	createRenderer(time);
	renderer->frameStart(time);
}

// -------------------------------------------------------------------------
// RegDebug
// -------------------------------------------------------------------------

V9990::RegDebug::RegDebug(V9990& v9990_)
	: SimpleDebuggable(v9990_.getMotherBoard(),
	                   v9990_.getName() + " regs", "V9990 registers", 0x40)
{
}

byte V9990::RegDebug::read(unsigned address)
{
	auto& v9990 = OUTER(V9990, v9990RegDebug);
	return v9990.regs[address];
}

void V9990::RegDebug::write(unsigned address, byte value, EmuTime::param time)
{
	auto& v9990 = OUTER(V9990, v9990RegDebug);
	v9990.writeRegister(address, value, time);
}

// -------------------------------------------------------------------------
// PalDebug
// -------------------------------------------------------------------------

V9990::PalDebug::PalDebug(V9990& v9990_)
	: SimpleDebuggable(v9990_.getMotherBoard(),
	                   v9990_.getName() + " palette",
	                   "V9990 palette (format is R, G, B, 0).", 0x100)
{
}

byte V9990::PalDebug::read(unsigned address)
{
	auto& v9990 = OUTER(V9990, v9990PalDebug);
	return v9990.palette[address];
}

void V9990::PalDebug::write(unsigned address, byte value, EmuTime::param time)
{
	auto& v9990 = OUTER(V9990, v9990PalDebug);
	v9990.writePaletteRegister(address, value, time);
}

// -------------------------------------------------------------------------
// Private methods
// -------------------------------------------------------------------------

inline unsigned V9990::getVRAMAddr(RegisterId base) const
{
	return   regs[base + 0] +
	        (regs[base + 1] << 8) +
	       ((regs[base + 2] & 0x07) << 16);
}

inline void V9990::setVRAMAddr(RegisterId base, unsigned addr)
{
	regs[base + 0] =   addr &     0xFF;
	regs[base + 1] =  (addr &   0xFF00) >> 8;
	regs[base + 2] = ((addr & 0x070000) >> 16) | (regs[base + 2] & 0x80);
	// TODO check
}

byte V9990::readRegister(byte reg, EmuTime::param time) const
{
	// TODO sync(time) (if needed at all)
	if (systemReset) return 255; // verified on real MSX

	assert(reg < 64);
	if (regAccess[reg] & ALLOW_READ) {
		if (reg < CMD_PARAM_BORDER_X_0) {
			return regs[reg];
		} else {
			word borderX = cmdEngine.getBorderX(time);
			return (reg == CMD_PARAM_BORDER_X_0)
			       ? (borderX & 0xFF) : (borderX >> 8);
		}
	} else {
		return 0xFF;
	}
}

void V9990::syncAtNextLine(SyncBase& type, EmuTime::param time)
{
	int line = getUCTicksThisFrame(time) / V9990DisplayTiming::UC_TICKS_PER_LINE;
	int ticks = (line + 1) * V9990DisplayTiming::UC_TICKS_PER_LINE;
	EmuTime nextTime = frameStartTime + ticks;
	type.setSyncPoint(nextTime);
}

void V9990::writeRegister(byte reg, byte val, EmuTime::param time)
{
	// Found this table by writing 0xFF to a register and reading
	// back the value (only works for read/write registers)
	static constexpr std::array<byte, 32> regWriteMask = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0x87, 0xFF, 0x83, 0x0F, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xDF, 0x07, 0xFF, 0xFF, 0xC1, 0x07,
		0x3F, 0xCF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};

	assert(reg < 64);
	if (!(regAccess[reg] & ALLOW_WRITE)) {
		// register not writable
		return;
	}
	if (reg >= CMD_PARAM_SRC_ADDRESS_0) {
		cmdEngine.setCmdReg(reg, val, time);
		if (reg == CMD_PARAM_OPCODE) {
			scheduleCmdEnd(time);
		}
		return;
	}

	val &= regWriteMask[reg];

	// This optimization is not valid for the vertical scroll registers
	// TODO is this optimization still useful for other registers?
	//if (!change) return;

	// Perform additional tasks before new value becomes active
	// note: no update for SCROLL_CONTROL_AY1, SCROLL_CONTROL_BY1
	switch (reg) {
		case SCREEN_MODE_0:
		case SCREEN_MODE_1:
			// TODO verify this on real V9990
			syncAtNextLine(syncSetMode, time);
			break;
		case PALETTE_CONTROL:
			renderer->setColorMode(getColorMode(val), time);
			break;
		case BACK_DROP_COLOR:
			renderer->updateBackgroundColor(val & 63, time);
			break;
		case SCROLL_CONTROL_AY0:
			renderer->updateScrollAYLow(time);
			break;
		case SCROLL_CONTROL_BY0:
			renderer->updateScrollBYLow(time);
			break;
		case SCROLL_CONTROL_AX0:
		case SCROLL_CONTROL_AX1:
			renderer->updateScrollAX(time);
			break;
		case SCROLL_CONTROL_BX0:
		case SCROLL_CONTROL_BX1:
			renderer->updateScrollBX(time);
			break;
		case DISPLAY_ADJUST:
			// TODO verify on real V9990: when exactly does a
			//  change in horizontal/vertical adjust take place
			break;
	}
	// commit the change
	regs[reg] = val;

	// Perform additional tasks after new value became active
	switch (reg) {
		case VRAM_WRITE_ADDRESS_2:
			// write pointer is only updated on R#2 write
			vramWritePtr = getVRAMAddr(VRAM_WRITE_ADDRESS_0);
			break;
		case VRAM_READ_ADDRESS_2:
			// write pointer is only updated on R#5 write
			vramReadPtr = getVRAMAddr(VRAM_READ_ADDRESS_0);
			// update read buffer immediately after read pointer changes. TODO: timing?
			vramReadBuffer = vram.readVRAMCPU(vramReadPtr, time);
			break;
		case SCREEN_MODE_0:
		case SCREEN_MODE_1:
		case CONTROL:
			// These influence the command timing
			scheduleCmdEnd(time);
			break;
		case INTERRUPT_0:
			irq.set((pendingIRQs & val) != 0);
			scheduleCmdEnd(time);
			break;
		case INTERRUPT_1:
		case INTERRUPT_2:
		case INTERRUPT_3:
			scheduleHscan(time);
			break;
	}
}

void V9990::writePaletteRegister(byte reg, byte val, EmuTime::param time)
{
	switch (reg & 3) {
		case 0: val &= 0x9F; break;
		case 1: val &= 0x1F; break;
		case 2: val &= 0x1F; break;
		case 3: val  = 0x00; break;
	}
	palette[reg] = val;
	reg &= ~3;
	byte index = reg / 4;
	bool ys = isSuperimposing() && (palette[reg] & 0x80);
	renderer->updatePalette(index, palette[reg + 0] & 0x1F, palette[reg + 1],
	                        palette[reg + 2], ys, time);
	if (index == regs[BACK_DROP_COLOR]) {
		renderer->updateBackgroundColor(index, time);
	}
}

V9990::GetPaletteResult V9990::getPalette(int index) const
{
	byte r = palette[4 * index + 0] & 0x1F;
	byte g = palette[4 * index + 1];
	byte b = palette[4 * index + 2];
	bool ys = isSuperimposing() && (palette[4 * index + 0] & 0x80);
	return {r, g, b, ys};
}

void V9990::createRenderer(EmuTime::param time)
{
	assert(!renderer);
	renderer = RendererFactory::createV9990Renderer(*this, display);
	renderer->reset(time);
}

void V9990::frameStart(EmuTime::param time)
{
	// Update setings that are fixed at the start of a frame
	displayEnabled = (regs[CONTROL]       & 0x80) != 0;
	palTiming      = (regs[SCREEN_MODE_1] & 0x08) != 0;
	interlaced     = (regs[SCREEN_MODE_1] & 0x02) != 0;
	scrollAYHigh   = regs[SCROLL_CONTROL_AY1];
	scrollBYHigh   = regs[SCROLL_CONTROL_BY1];
	setVerticalTiming();
	status ^= 0x02; // flip EO bit

	bool newSuperimposing = (regs[CONTROL] & 0x20) && externalVideoSource;
	if (superimposing != newSuperimposing) {
		superimposing = newSuperimposing;
		renderer->updateSuperimposing(superimposing, time);
	}

	frameStartTime.reset(time);

	// schedule next VSYNC
	syncVSync.setSyncPoint(
		frameStartTime + V9990DisplayTiming::getUCTicksPerFrame(palTiming));

	// schedule DISPLAY_START
	syncDisplayStart.setSyncPoint(
	    frameStartTime + getTopBorder() * V9990DisplayTiming::UC_TICKS_PER_LINE);

	// schedule VSCAN
	syncVScan.setSyncPoint(
	    frameStartTime + getBottomBorder() * V9990DisplayTiming::UC_TICKS_PER_LINE);

	renderer->frameStart(time);
}

void V9990::raiseIRQ(IRQType irqType)
{
	pendingIRQs |= irqType;
	if (pendingIRQs & regs[INTERRUPT_0]) {
		irq.set();
	}
}

void V9990::setHorizontalTiming()
{
	switch (mode) {
	case P1: case P2:
	case B1: case B3: case B7:
		horTiming = &V9990DisplayTiming::lineMCLK;
		break;
	case B0: case B2: case B4:
		horTiming = &V9990DisplayTiming::lineXTAL;
	case B5: case B6:
		break;
	default:
		UNREACHABLE;
	}
}

void V9990::setVerticalTiming()
{
	switch (mode) {
	case P1: case P2:
	case B1: case B3: case B7:
		verTiming = isPalTiming()
		          ? &V9990DisplayTiming::displayPAL_MCLK
		          : &V9990DisplayTiming::displayNTSC_MCLK;
		break;
	case B0: case B2: case B4:
		verTiming = isPalTiming()
		          ? &V9990DisplayTiming::displayPAL_XTAL
		          : &V9990DisplayTiming::displayNTSC_XTAL;
	case B5: case B6:
		break;
	default:
		UNREACHABLE;
	}
}

V9990ColorMode V9990::getColorMode(byte pal_ctrl) const
{
	if (!(regs[SCREEN_MODE_0] & 0x80)) {
		return BP4;
	} else {
		switch (regs[SCREEN_MODE_0] & 0x03) {
			case 0x00: return BP2;
			case 0x01: return BP4;
			case 0x02:
				switch (pal_ctrl & 0xC0) {
					case 0x00: return BP6;
					case 0x40: return BD8;
					case 0x80: return BYJK;
					case 0xC0: return BYUV;
				}
				break;
			case 0x03: return BD16;
		}
	}
	UNREACHABLE;
	return INVALID_COLOR_MODE;
}

V9990ColorMode V9990::getColorMode() const
{
	return getColorMode(regs[PALETTE_CONTROL]);
}

V9990DisplayMode V9990::calcDisplayMode() const
{
	switch (regs[SCREEN_MODE_0] & 0xC0) {
		case 0x00:
			return P1;
		case 0x40:
			return P2;
		case 0x80:
			if (status & 0x04) { // MCLK timing
				switch(regs[SCREEN_MODE_0] & 0x30) {
					case 0x00: return B0;
					case 0x10: return B2;
					case 0x20: return B4;
				}
			} else { // XTAL1 timing
				switch(regs[SCREEN_MODE_0] & 0x30) {
					case 0x00: return B1;
					case 0x10: return B3;
					case 0x20: return B7;
				}
			}
			break;
	}
	// invalid display mode
	return P1; // TODO Check
}

void V9990::setDisplayMode(V9990DisplayMode newMode)
{
	mode = newMode;
	setHorizontalTiming();
}

void V9990::scheduleHscan(EmuTime::param time)
{
	// remove pending HSCAN, if any
	if (hScanSyncTime > time) {
		syncHScan.removeSyncPoint();
		hScanSyncTime = time;
	}

	if (pendingIRQs & HOR_IRQ) {
		// flag already set, no need to schedule
		return;
	}

	int ticks = frameStartTime.getTicksTill_fast(time);
	int offset = [&] {
		if (regs[INTERRUPT_2] & 0x80) {
			// every line
			return ticks - (ticks % V9990DisplayTiming::UC_TICKS_PER_LINE);
		} else {
			int line = regs[INTERRUPT_1] + 256 * (regs[INTERRUPT_2] & 3) +
				   getTopBorder();
			return line * V9990DisplayTiming::UC_TICKS_PER_LINE;
		}
	}();
	int mult = (status & 0x04) ? 3 : 2; // MCLK / XTAL1
	offset += (regs[INTERRUPT_3] & 0x0F) * 64 * mult;
	if (offset <= ticks) {
		offset += V9990DisplayTiming::getUCTicksPerFrame(palTiming);
	}

	hScanSyncTime = frameStartTime + offset;
	syncHScan.setSyncPoint(hScanSyncTime);
}

static constexpr std::initializer_list<enum_string<V9990DisplayMode>> displayModeInfo = {
	{ "INVALID", INVALID_DISPLAY_MODE },
	{ "P1", P1 }, { "P2", P2 },
	{ "B0", B0 }, { "B1", B1 }, { "B2", B2 }, { "B3", B3 },
	{ "B4", B4 }, { "B5", B5 }, { "B6", B6 }, { "B7", B7 }
};
SERIALIZE_ENUM(V9990DisplayMode, displayModeInfo);

// version 1: initial version
// version 2: added systemReset
// version 3: added vramReadPtr, vramWritePtr, vramReadBuffer
// version 4: removed 'userData' from Schedulable
// version 5: added syncCmdEnd
template<typename Archive>
void V9990::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);

	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("syncVSync",        syncVSync,
		             "syncDisplayStart", syncDisplayStart,
		             "syncVScan",        syncVScan,
		             "syncHScan",        syncHScan,
		             "syncSetMode",      syncSetMode);
	} else {
		Schedulable::restoreOld(ar,
			{&syncVSync, &syncDisplayStart, &syncVScan,
			 &syncHScan, &syncSetMode});
	}
	if (ar.versionAtLeast(version, 5)) {
		ar.serialize("syncCmdEnd", syncCmdEnd);
	}

	ar.serialize("displayMode", mode); // must be deserialized before cmdEngine (because it's used to restore some derived state in cmdEngine)
	ar.serialize("vram",           vram,
	             "cmdEngine",      cmdEngine,
	             "irq",            irq,
	             "frameStartTime", frameStartTime,
	             "hScanSyncTime",  hScanSyncTime);
	ar.serialize_blob("palette", palette);
	ar.serialize("status",      status,
	             "pendingIRQs", pendingIRQs);
	ar.serialize_blob("registers", regs);
	ar.serialize("regSelect",      regSelect,
	             "palTiming",      palTiming,
	             "interlaced",     interlaced,
	             "isDisplayArea",  isDisplayArea,
	             "displayEnabled", displayEnabled,
	             "scrollAYHigh",   scrollAYHigh,
	             "scrollBYHigh",   scrollBYHigh);

	if (ar.versionBelow(version, 2)) {
		systemReset = false;
	} else {
		ar.serialize("systemReset", systemReset);
	}

	if (ar.versionBelow(version, 3)) {
		vramReadPtr = getVRAMAddr(VRAM_READ_ADDRESS_0);
		vramWritePtr = getVRAMAddr(VRAM_WRITE_ADDRESS_0);
		vramReadBuffer = vram.readVRAMCPU(vramReadPtr, getCurrentTime());
	} else {
		ar.serialize("vramReadPtr",    vramReadPtr,
		             "vramWritePtr",   vramWritePtr,
		             "vramReadBuffer", vramReadBuffer);
	}

	// No need to serialize 'externalVideoSource', it will be restored when
	// the external peripheral (e.g. Video9000) is de-serialized.
	// TODO should 'superimposing' be serialized? It can't be recalculated
	// from register values (it depends on the register values at the start
	// of this frame). But it will be correct at the start of the next
	// frame. Good enough?

	if constexpr (Archive::IS_LOADER) {
		// TODO This uses 'mode' to calculate 'horTiming' and
		//      'verTiming'. Are these always in sync? Or can for
		//      example one change at any time and the other only
		//      at start of frame (or next line)? Does this matter?
		setHorizontalTiming();
		setVerticalTiming();

		renderer->reset(getCurrentTime());
	}
}
INSTANTIATE_SERIALIZE_METHODS(V9990);
REGISTER_MSXDEVICE(V9990, "V9990");

} // namespace openmsx
