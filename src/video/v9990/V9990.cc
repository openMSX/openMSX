// $Id$ 

#include "V9990.hh"
#include "Scheduler.hh"
#include "EventDistributor.hh"
#include "Debugger.hh"
#include "CommandController.hh"
#include "RendererFactory.hh"
#include "V9990VRAM.hh"
#include "V9990CmdEngine.hh"
#include "V9990Renderer.hh"
#include <cassert>
#include <iomanip>

using std::setw;
using std::string;
using std::vector;

namespace openmsx {

enum RegisterAccess { NO_ACCESS, RD_ONLY, WR_ONLY, RD_WR };
static const RegisterAccess regAccess[64] = {
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
	RD_WR,                              // Sprite Pattern Table Adress
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

V9990::V9990(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time),
	  v9990RegDebug(*this),
	  v9990RegsCmd(*this),
	  pendingIRQs(0)
{
	PRT_DEBUG("[" << time << "] V9990::Create");

        // clear regs TODO find realistic init values
        memset(regs, 0, sizeof(regs));
	calcDisplayMode();

	// create VRAM
	vram.reset(new V9990VRAM(this, time));

	// create Command Engine
	cmdEngine.reset(new V9990CmdEngine(this));

	// Start with NTSC timing
	palTiming = false;
	
	// Register debuggable
	Debugger::instance().registerDebuggable("V9990 regs", v9990RegDebug);

	// Register console commands
	CommandController::instance().registerCommand(&v9990RegsCmd, "v9990regs");

	// Initialise rendering system
	createRenderer(time);

	// initialize palette
	for (int i = 0; i < 64; ++i) {
		palette[4 * i + 0] = 0x9F;
		palette[4 * i + 1] = 0x1F;
		palette[4 * i + 2] = 0x1F;
		renderer->setPalette(i, 0x9F, 0x1F, 0x1F, time);
		palette[4 * i + 3] = 0x00;
	}

	reset(time);
	EventDistributor::instance().registerEventListener(
		RENDERER_SWITCH2_EVENT, *this, EventDistributor::DETACHED );
}


V9990::~V9990()
{
	PRT_DEBUG("[--now--] V9990::Destroy");

	// Unregister everything that needs to be unregistered
	CommandController::instance().unregisterCommand(&v9990RegsCmd, "v9990regs");
	Debugger::instance().unregisterDebuggable("V9990 regs", v9990RegDebug);
	EventDistributor::instance().unregisterEventListener(
		RENDERER_SWITCH2_EVENT, *this, EventDistributor::DETACHED );
}

// -------------------------------------------------------------------------
// MSXDevice
// -------------------------------------------------------------------------

void V9990::reset(const EmuTime& time)
{
	PRT_DEBUG("[" << time << "] V9990::reset");

	Scheduler::instance().removeSyncPoint(this, V9990_VSYNC);

	// Reset IRQs
	writeIO(INTERRUPT_FLAG, 0xFF, time);
	
	// Clear registers / ports
	memset(regs, 0, sizeof(regs));
	status = 0;
	regSelect = 0xFF; // TODO check value for power-on and reset
	calcDisplayMode();

	palTiming = false;
	// Reset sub-systems
	renderer->reset(time);
	cmdEngine->reset(time);

	// Init scheduling
	frameStart(time);
}

byte V9990::readIO(byte port, const EmuTime& time)
{
	port &= 0x0F;
	
	byte result = 0;
	switch (port) {
		case VRAM_DATA: {
			// read from VRAM
			unsigned addr = getVRAMAddr(VRAM_READ_ADDRESS_0);
			result = vram->readVRAM(addr);
			if (!(regs[VRAM_READ_ADDRESS_2] & 0x80)) {
				setVRAMAddr(VRAM_READ_ADDRESS_0, addr + 1);
			}
			break;
		}
		case PALETTE_DATA: {
			byte palPtr = regs[PALETTE_POINTER];
			switch (palPtr & 3) {
				case 0: // red
				case 1: // green
					result = palette[palPtr];
					regs[PALETTE_POINTER] = palPtr + 1;
					break;
				case 2: // blue
					result = palette[palPtr];
					regs[PALETTE_POINTER] = palPtr + 2;
					break;
				default: // invalid, checked on real V9990
					result = 0;
					regs[PALETTE_POINTER] = palPtr - 3;
					break;
			}
			break;
			palPtr &= ~3;
			renderer->setPalette(palPtr >> 2,
			                     regs[palPtr], regs[palPtr+1], regs[palPtr+2],
			                     time);
		}
		case COMMAND_DATA:
			// TODO
			//assert(cmdEngine != NULL);
			result = cmdEngine->getCmdData(time);
			break;

		case REGISTER_DATA: {
			// read register
			result = readRegister(regSelect & 0x3F, time);
			if (!(regSelect & 0x40)) {
				regSelect =   regSelect      & 0xC0 |
				            ((regSelect + 1) & 0x3F);
			}
			break;
		}
		case INTERRUPT_FLAG:
			result = pendingIRQs;
			break;
		    
		case STATUS: {
			int ticks = getUCTicksThisFrame(time);
			int x = UCtoX(ticks, getDisplayMode());
			int y = ticks / V9990DisplayTiming::UC_TICKS_PER_LINE;
			bool hr = (x < 64) || (576 <= x); // TODO not correct
			bool vr = (y < 14) || (226 <= y); // TODO not correct
			result = (cmdEngine->getTransfer(time) ? 0x80 : 0x00) |
				 (vr ? 0x40 : 0x00) |
				 (hr ? 0x20 : 0x00) |
				 (status & 0x1F);
			break;
		}
		case KANJI_ROM_1:
		case KANJI_ROM_3:
			// not used in Gfx9000
			result = 0xFF;	// TODO check
			break;

		case REGISTER_SELECT:
		case SYSTEM_CONTROL:
		case KANJI_ROM_0:
		case KANJI_ROM_2:
		default:
			// write-only
			result = 0xFF;
			break;
	}
	
	PRT_DEBUG("[" << time << "] "
		  "V9990::readIO - port=0x" << std::hex << (int)port <<
		                  " val=0x" << std::hex << (int)result);

	return result;
}

byte V9990::peekIO(byte port, const EmuTime& time) const
{
	// TODO not implemented
	return 0xFF;
}

void V9990::writeIO(byte port, byte val, const EmuTime& time)
{
	port &= 0x0F;
	
	PRT_DEBUG("[" << time << "] "
		  "V9990::writeIO - port=0x" << std::hex << int(port) << 
		                   " val=0x" << std::hex << int(val));

	switch (port) {
		case VRAM_DATA: {
			// write VRAM
			unsigned addr = getVRAMAddr(VRAM_WRITE_ADDRESS_0);
			vram->writeVRAM(addr, val);
			if (!(regs[VRAM_WRITE_ADDRESS_2] & 0x80)) {
				setVRAMAddr(VRAM_WRITE_ADDRESS_0, addr + 1);
			}
			break;
		}
		case PALETTE_DATA: {
			byte palPtr = regs[PALETTE_POINTER];
			switch (palPtr & 3) {
				case 0: // red
					palette[palPtr] = val & 0x9F;
					regs[PALETTE_POINTER] = palPtr + 1;
					break;
				case 1: // green
					palette[palPtr] = val & 0x1F;
					regs[PALETTE_POINTER] = palPtr + 1;
					break;
				case 2: // blue
					palette[palPtr] = val & 0x1F;
					regs[PALETTE_POINTER] = palPtr + 2;
					break;
				default: // invalid, checked on real V9990
					regs[PALETTE_POINTER] = palPtr - 3;
					break;
			}
			palPtr &= ~3;
			renderer->setPalette(palPtr >> 2,
			                     palette[palPtr + 0],
			                     palette[palPtr + 1],
			                     palette[palPtr + 2],
			                     time);
			break;
		}
		case COMMAND_DATA:
			//assert(cmdEngine != NULL);
			cmdEngine->setCmdData(val, time);
			break;

		case REGISTER_DATA: {
			// write register
			writeRegister(regSelect & 0x3F, val, time);
			if (!(regSelect & 0x80)) {
				regSelect =   regSelect      & 0xC0 |
				            ((regSelect + 1) & 0x3F);
			}
			break;
		}
		case REGISTER_SELECT:
			regSelect = val;
			break;

		case STATUS:
			// read-only, ignore writes
			break;
		
		case INTERRUPT_FLAG:
			pendingIRQs &= ~val;
			if (!pendingIRQs) {
				irq.reset();
			}
			break;
		
		case SYSTEM_CONTROL:
			status = (status & 0xFB) | ((val & 1) << 2); 
			calcDisplayMode();
			renderer->setDisplayMode(getDisplayMode(), time);
			renderer->setImageWidth(getImageWidth());
		break;
		
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

void V9990::executeUntil(const EmuTime& time, int userData)
{
	PRT_DEBUG("[" << time << "] "
	          "V9990::executeUntil - data=0x" << std::hex << userData);
	switch (userData)  {
		case V9990_VSYNC:
			renderer->frameEnd(time);
			frameStart(time);
			break;
		default:
			/* ignore */
			break;
	}
}

const string& V9990::schedName() const
{
	static const string name("V9990");
	PRT_DEBUG("[--now---] V9990::SchedName - \"" << name << "\"");
	return name;
}

// -------------------------------------------------------------------------
// EventListener
// -------------------------------------------------------------------------

bool V9990::signalEvent(const Event& event)
{
	PRT_DEBUG("[--now---] V9990::signalEvent");
	
	assert(event.getType() == RENDERER_SWITCH2_EVENT);
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	createRenderer(time);
	renderer->frameStart(time);
	return true;
}

// -------------------------------------------------------------------------
// V9990RegDebug
// -------------------------------------------------------------------------

V9990::V9990RegDebug::V9990RegDebug(V9990& parent_)
	: parent(parent_)
{
}

unsigned V9990::V9990RegDebug::getSize() const
{
	return 0x40;
}

const string& V9990::V9990RegDebug::getDescription() const
{
	static const string desc = "V9990 registers.";
	return desc;
}

byte V9990::V9990RegDebug::read(unsigned address)
{
	return parent.regs[address];
}

void V9990::V9990RegDebug::write(unsigned address, byte value)
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	parent.writeRegister(address, value, time);
}

// -------------------------------------------------------------------------
// V9990RegsCmd
// -------------------------------------------------------------------------

V9990::V9990RegsCmd::V9990RegsCmd(V9990& v9990_)
	: v9990(v9990_)
{
}

string V9990::V9990RegsCmd::execute(const vector<string>& /*tokens*/)
{
	// Print 55 registers in 4 colums
	
	static const int NCOLS = 4;
	static const int NROWS = (55 + (NCOLS-1))/NCOLS;
	
	std::ostringstream out;
	for(int row = 0; row < NROWS; row++) {
		for(int col = 0; col < NCOLS; col++) {
			int reg   = col * NROWS + row;
			if(reg < 55) {
				int value = v9990.regs[reg];
				out << std::dec << std::setw(2) << reg << " : "
			    	<< std::hex << std::setw(2) << value << "   ";
			}
		}
		out << "\n";
	}
	return out.str();
}

string V9990::V9990RegsCmd::help(const vector<string>& /*tokens*/) const
{
	return "Prints the current state of the V9990 registers.\n";
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
	regs[base + 2] = ((addr & 0x070000) >> 16) | regs[base + 2] & 0x80;
	// TODO check
}

byte V9990::readRegister(byte reg, const EmuTime& time)
{
	// TODO sync(time) (if needed at all)
	//
	byte result;
	if(regAccess[reg] != NO_ACCESS && regAccess[reg] != WR_ONLY) {
		result = regs[reg];
	} else {
		result = 0xFF;
	}

	PRT_DEBUG("[" << time << "] "
		  "V9990::readRegister - reg=0x" << std::hex << (int)reg <<
		                       " val=0x" << std::hex << (int)result);
	return result;
}

void V9990::writeRegister(byte reg, byte val, const EmuTime& time)
{
	PRT_DEBUG("[" << time << "] "
		  "V9990::writeRegister - reg=0x" << std::hex << int(reg) << 
		                        " val=0x" << std::hex << int(val));

	// TODO sync(time)
	
	if(regAccess[reg] != NO_ACCESS && regAccess[reg] != RD_ONLY) {
		regs[reg] = val;

		switch (reg) {
			case INTERRUPT_0:
				if (pendingIRQs & val) {
					irq.set();
				} else {
					irq.reset();
				}
				break;
			case SCREEN_MODE_0:
			case SCREEN_MODE_1:
				calcDisplayMode();
				renderer->setDisplayMode(getDisplayMode(), time);
				renderer->setImageWidth(getImageWidth());
				// fall through!
			case PALETTE_CONTROL:
				renderer->setColorMode(getColorMode(), time);
				break;
			case BACK_DROP_COLOR:
				renderer->setBackgroundColor(val & 31);
				break;
			default: break;
		}
		if((reg >= CMD_PARAM_SRC_ADDRESS_0) && (reg <= CMD_PARAM_OPCODE)) {
			//assert(cmdEngine != NULL);
			cmdEngine->setCmdReg(reg, val, time);
		}
	} else {
		PRT_DEBUG("[" << time << "] "
		"V9990::writeRegister: Register not writable");
	}
}

void V9990::createRenderer(const EmuTime& time)
{
	renderer.reset(); // delete old renderer before creating new one
	renderer.reset(RendererFactory::createV9990Renderer(this));
	renderer->reset(time);
}

void V9990::frameStart(const EmuTime& time)
{
	// Update setings that are fixed at the start of a frame
	palTiming = regs[SCREEN_MODE_1] & 0x08;
	status ^= 0x02; // flip EO bit

	renderer->frameStart(time);
	// Schedule next VSYNC
	frameStartTime.advance(time);
			
	Scheduler::instance().setSyncPoint(
		frameStartTime + V9990DisplayTiming::getUCTicksPerFrame(palTiming),
		this, V9990_VSYNC);
}

void V9990::raiseIRQ(IRQType irqType)
{
	pendingIRQs |= irqType;
	if (pendingIRQs & regs[INTERRUPT_0]) {
		irq.set();
	}
}

V9990ColorMode V9990::getColorMode()
{
	V9990ColorMode mode = INVALID_COLOR_MODE;

	if(!regs[SCREEN_MODE_0] & 0x80) {
		mode = BP4;
	} else {
		switch(regs[SCREEN_MODE_0] & 0x03) {
			case 0x00: mode = BP2; break;
			case 0x01: mode = BP4; break;
			case 0x02: 
				switch(regs[PALETTE_CONTROL] & 0xC0) {
					case 0x00: mode = BP6; break;
					case 0x40: mode = BD8; break;
					case 0x80: mode = BYJK; break;
					case 0xC0: mode = BYUV; break;
					default: assert(false); break;
				}
				break;
			case 0x03: mode = BD16; break;
			default: assert(false); break;
		}		
	}
	
	PRT_DEBUG("[---------] V9990::getColorMode: mode = " <<
	        ((mode == BP2)?  "BP2":
	         (mode == BP4)?  "BP4":
	         (mode == BP6)?  "BP6":
	         (mode == BD8)?  "BD8":
	         (mode == BD16)? "BD16":
	         (mode == BYJK)? "BYJK":
	         (mode == BYUV)? "BYUV":
			 "Dunno"));

	// TODO Check
	if(mode == INVALID_COLOR_MODE) mode = BP4;
	return mode;
}

void V9990::calcDisplayMode()
{
	mode = INVALID_DISPLAY_MODE;
	switch (regs[SCREEN_MODE_0] & 0xC0) {
		case 0x00:
			mode = P1;
			break;
		case 0x40:
			mode = P2;
			break;
		case 0x80:
			if (status & 0x04) {
				// MCLK timing
				switch (regs[SCREEN_MODE_0] & 0x30) {
				case 0x00: mode = B0; break;
				case 0x10: mode = B2; break;
				case 0x20: mode = B4; break;
				case 0x30: mode = INVALID_DISPLAY_MODE; break;
				}
			} else {
				// XTAL1 timing
				switch(regs[SCREEN_MODE_0] & 0x30) {
				case 0x00: mode = B1; break;
				case 0x10: mode = B3; break;
				case 0x20: mode = B7; break;
				case 0x30: mode = INVALID_DISPLAY_MODE; break;
				}
			}
			break;
		case 0xC0:
			mode = INVALID_DISPLAY_MODE;
			break;
	}

	// TODO Check
	if (mode == INVALID_DISPLAY_MODE) mode = P1;
}

} // namespace openmsx

