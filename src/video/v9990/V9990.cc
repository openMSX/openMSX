// $Id$ //

/*
 * The Graphics9000 class
 * 
 * TODO:
 * - Close to everything :-)
 */

#include <iomanip>
#include <cassert>
#include "Scheduler.hh"
#include "Debugger.hh"
#include "V9990.hh"

namespace openmsx {

const unsigned VRAM_SIZE = 0x80000; // 512kB

V9990::V9990(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time),
	  v9990RegDebug(*this),
	  v9990VRAMDebug(*this),
	  pendingIRQs(0)
{
	PRT_DEBUG("[" << time << "] V9990::Create");

	vram = new byte[VRAM_SIZE];

	// initialize palette
	for (int i = 0; i < 64; ++i) {
		palette[4 * i + 0] = 0x9F;
		palette[4 * i + 1] = 0x1F;
		palette[4 * i + 2] = 0x1F;
		palette[4 * i + 3] = 0x00;
	}
	
	Debugger::instance().registerDebuggable("V9990 regs", v9990RegDebug);
	Debugger::instance().registerDebuggable("V9990 VRAM", v9990VRAMDebug);

	reset(time);
}

V9990::~V9990()
{
	PRT_DEBUG("[--now--] V9990::Destroy");

	Debugger::instance().unregisterDebuggable("V9990 VRAM", v9990VRAMDebug);
	Debugger::instance().unregisterDebuggable("V9990 regs", v9990RegDebug);
	
	delete[] vram;
}

void V9990::reset(const EmuTime& time)
{
	PRT_DEBUG("[" << time << "] V9990::reset");

	// Reset IRQs
	writeIO(INTERRUPT_FLAG, 0xFF, time);
	
	// Clear registers / ports
	memset(regs, 0, sizeof(regs));
	regSelect = 0xFF; // TODO check value for power-on and reset
	
	// Palette remains unchanged after reset
}

void V9990::executeUntil(const EmuTime &time, int userData)
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
		case COMMAND_DATA:
			// TODO
			result = 0xC0;
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
		    
		case STATUS:
			// TODO
			result = 0x00;
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
		case COMMAND_DATA:
			// TODO
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
			// TODO
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

	switch (reg) {
		case INT_ENABLE:
			if (pendingIRQs & val) {
				irq.set();
			} else {
				irq.reset();
			}
			break;
	}
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


void V9990::raiseIRQ(IRQType irqType)
{
	pendingIRQs |= irqType;
	if (pendingIRQs & regs[INT_ENABLE]) {
		irq.set();
	}
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

// V9990VRAMDebug

V9990::V9990VRAMDebug::V9990VRAMDebug(V9990& parent_)
	: parent(parent_)
{
}

unsigned V9990::V9990VRAMDebug::getSize() const
{
	return VRAM_SIZE;
}

const string& V9990::V9990VRAMDebug::getDescription() const
{
	static const string desc = "V9990 VRAM.";
	return desc;
}

byte V9990::V9990VRAMDebug::read(unsigned address)
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	return parent.readVRAM(address, time);
}

void V9990::V9990VRAMDebug::write(unsigned address, byte value)
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	parent.writeVRAM(address, value, time);
}

} // namespace openmsx

