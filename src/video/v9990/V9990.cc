// $Id$ //

/*
 * The Graphics9000 class
 * 
 * TODO:
 * - Close to everything :-)
 */

#include <iomanip>
#include <cassert>

#include "V9990.hh"

#include "Scheduler.hh"
#include "MSXConfig.hh"
#include "Config.hh"
#include "Debugger.hh"

namespace openmsx {

V9990::V9990(Config* config, const EmuTime& time)
	: MSXDevice(config, time),
	  MSXIODevice(config, time),
	  v9990RegDebug(*this),
	  v9990PortDebug(*this)
{
	PRT_DEBUG("[" << time << "] V9990::Create");

	vram = new byte[0x080000]; // 512kb

	// initialize palette
	for (int i = 0; i < 64; ++i) {
		palette[4 * i + 0] = 0x9F;
		palette[4 * i + 1] = 0x1F;
		palette[4 * i + 2] = 0x1F;
		palette[4 * i + 3] = 0x00;
	}
	
	Debugger::instance().registerDebuggable("v9990-regs", v9990RegDebug);
	Debugger::instance().registerDebuggable("v9990-ports", v9990PortDebug);

	reset(time);
}

V9990::~V9990()
{
	PRT_DEBUG("[--now--] V9990::Destroy");

	Debugger::instance().unregisterDebuggable("v9990-ports", v9990PortDebug);
	Debugger::instance().unregisterDebuggable("v9990-regs", v9990RegDebug);
	
	delete[] vram;
}

void V9990::reset(const EmuTime& time)
{
	PRT_DEBUG("[" << time << "] V9990::reset");

	static const byte INITIAL_PORT_VALUES[16] = {
		0x00, 0x9F, 0xC0, 0xFF, 0xFF, 0x00, 0x03, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	
	memcpy(ports, INITIAL_PORT_VALUES, 16);

	// Palette remains unchanged after reset
}

void V9990::executeUntil(const EmuTime &time, int userData) throw()
{
	PRT_DEBUG("[" << time << "] "
	          "V9990::executeUntil - data=0x" << hex << userData);
}

const string& V9990::schedName() const
{
	static const string name("V9990");
	PRT_DEBUG("[--now-- ] V9990::SchedName - \"" << name << "\"");
	return name;
}


inline unsigned V9990::getVRAMAddr(RegId base) const
{
	return   regs[base + 0] +
	        (regs[base + 1] << 8) +
	       ((regs[base + 2] & 0x07) << 16);
}

inline void V9990::setVRAMAddr(RegId base, unsigned addr)
{
	regs[base + 0] =   addr &     0xFF;
	regs[base + 1] =  (addr &   0xFF00) >> 8;
	regs[base + 2] = ((addr & 0x070000) >> 16) | regs[base + 2] & 0x80; // TODO check
}

byte V9990::readIO(byte port, const EmuTime& time)
{
	port &= 0x0F;
	
	byte result;
	switch (port) {
		case VRAM_DATA: {
			// read from VRAM
			unsigned addr = getVRAMAddr(VRAM_RD_ADR0);
			result = readVRAM(addr, time);
			if (!(regs[VRAM_RD_ADR2] & 0x80)) {
				setVRAMAddr(VRAM_RD_ADR0, addr + 1);
			}
			break;
		}
		case PALETTE_DATA: {
			byte palPtr = regs[PALETTE_PTR];
			switch (palPtr & 3) {
				case 0: // red
				case 1: // green
					result = palette[palPtr];
					regs[PALETTE_PTR] = palPtr + 1;
					break;
				case 2: // blue
					result = palette[palPtr];
					regs[PALETTE_PTR] = palPtr + 2;
					break;
				default: // invalid, checked on real V9990
					result = 0;
					regs[PALETTE_PTR] = palPtr - 3;
					break;
			}
			break;
		}
		case REGISTER_DATA: {
			// read register
			byte regSelect = ports[REGISTER_SELECT];
			result = readRegister(regSelect & 0x3F, time);
			if (!(regSelect & 0x40)) {
				ports[REGISTER_SELECT] =
				    regSelect & 0xC0 | ((regSelect + 1) & 0x3F);
			}
			break;
		}
		case COMMAND_DATA:
		case STATUS:
		case INTERRUPT_FLAG:
			// TODO
			result = ports[port];
			break;
		
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
		  "V9990::readIO - port=0x" << hex << (int)port <<
		                  " val=0x" << hex << (int)result);

	return result;
}


void V9990::writeIO(byte port, byte val, const EmuTime &time)
{
	port &= 0x0F;
	
	PRT_DEBUG("[" << time << "] "
		  "V9990::writeIO - port=0x" << hex << int(port) << 
		                   " val=0x" << hex << int(val));

	switch (port) {
		case VRAM_DATA: {
			// write vram
			unsigned addr = getVRAMAddr(VRAM_WR_ADR0);
			writeVRAM(addr, val, time);
			if (!(regs[VRAM_WR_ADR2] & 0x80)) {
				setVRAMAddr(VRAM_WR_ADR0, addr + 1);
			}
			break;
		}
		case PALETTE_DATA: {
			byte palPtr = regs[PALETTE_PTR];
			switch (palPtr & 3) {
				case 0: // red
					palette[palPtr] = val & 0x9F;
					regs[PALETTE_PTR] = palPtr + 1;
					break;
				case 1: // green
					palette[palPtr] = val & 0x1F;
					regs[PALETTE_PTR] = palPtr + 1;
					break;
				case 2: // blue
					palette[palPtr] = val & 0x1F;
					regs[PALETTE_PTR] = palPtr + 2;
					break;
				default: // invalid, checked on real V9990
					regs[PALETTE_PTR] = palPtr - 3;
					break;
			}
			break;
		}
		case REGISTER_DATA: {
			// write register
			byte regSelect = ports[REGISTER_SELECT];
			writeRegister(regSelect & 0x3F, val, time);
			if (!(regSelect & 0x80)) {
				ports[REGISTER_SELECT] =
				    regSelect & 0xC0 | ((regSelect + 1) & 0x3F);
			}
			break;
		}
		case REGISTER_SELECT:
			ports[port] = val;
			break;

		case STATUS:
			// read-only, ignore writes
			break;
		
		case KANJI_ROM_0:
		case KANJI_ROM_1:
		case KANJI_ROM_2:
		case KANJI_ROM_3:
			// not used in Gfx9000
			ports[port] = val;
			break;
			
		case COMMAND_DATA:
		case INTERRUPT_FLAG:
		case SYSTEM_CONTROL:
		default:
			// TODO
			ports[port] = val;
			break;
	}
}

byte V9990::readRegister(byte reg, const EmuTime& time)
{
	// TODO sync(time) (if needed at all)
	byte result;
	switch (reg) {
		case VRAM_WR_ADR0:
		case VRAM_WR_ADR1:
		case VRAM_WR_ADR2:
		case VRAM_RD_ADR0:
		case VRAM_RD_ADR1:
		case VRAM_RD_ADR2:
			// write-only registers
			result = 0xFF;
			break;

		default:
			result = regs[reg];
	}
	PRT_DEBUG("[" << time << "] "
		  "V9990::readRegister - reg=0x" << hex << (int)reg <<
		                       " val=0x" << hex << (int)result);
	return result;
}

void V9990::writeRegister(byte reg, byte val, const EmuTime& time)
{
	PRT_DEBUG("[" << time << "] "
		  "V9990::writeRegister - reg=0x" << hex << int(reg) << 
		                        " val=0x" << hex << int(val));
	// TODO sync(time)
	regs[reg] = val;
}

byte V9990::readVRAM(unsigned addr, const EmuTime& time)
{
	// TODO sync(time)
	byte result = vram[addr];
	PRT_DEBUG("[" << time << "] "
		  "V9990::readVRAM - adr=0x" << hex << addr <<
		                   " val=0x" << hex << (int)result);
	return result;
}

void V9990::writeVRAM(unsigned addr, byte value, const EmuTime& time)
{
	PRT_DEBUG("[" << time << "] "
		  "V9990::writeVRAM - adr=0x" << hex << addr <<
		                    " val=0x" << hex << (int)value);
	// TODO sync(time)
	vram[addr] = value;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Private classes
 */

// V9990RegDebug

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

// V9990PortDebug

V9990::V9990PortDebug::V9990PortDebug(V9990& parent_)
	: parent(parent_)
{
}

unsigned V9990::V9990PortDebug::getSize() const
{
	return 16;
}

const string& V9990::V9990PortDebug::getDescription() const
{
	static const string desc = "V9990 I/O ports.";
	return desc;
}

byte V9990::V9990PortDebug::read(unsigned address)
{
	return parent.ports[address];
}

void V9990::V9990PortDebug::write(unsigned address, byte value)
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	parent.writeIO(address, value, time);
}

} // end class V9990

