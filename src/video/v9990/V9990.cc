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

	Debugger::instance().registerDebuggable("v9990-regs", v9990RegDebug);
	Debugger::instance().registerDebuggable("v9990-ports", v9990PortDebug);

	reset(time);
}

V9990::~V9990()
{
	PRT_DEBUG("[--now--] V9990::Destroy");

	Debugger::instance().unregisterDebuggable("v9990-regs", v9990RegDebug);
	Debugger::instance().unregisterDebuggable("v9990-ports", v9990PortDebug);
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

byte V9990::readIO(byte port, const EmuTime &time)
{
	byte value = 0;

	PRT_DEBUG("[" << time << "] "
		  "V9990::readIO - port=0x" << hex << int(port));

	switch(PortId(port & 0x0F)){
		case VRAM_DATA:
		case PALETTE_DATA:
		case REGISTER_DATA:
		case REGISTER_SELECT:
		case STATUS:
		case INTERRUPT_FLAG:
		case KANJI_ROM_1:
		case KANJI_ROM_2:
			value = ports[port];
			break;
		default:
			value = 0xFF;
			break;
	}
	return value;		
}

void V9990::writeIO(byte port, byte val, const EmuTime &time)
{
	PRT_DEBUG("[" << time << "] "
		  "V9990::writeIO - port=0x" << hex << int(port) << 
		                   " val=0x" << hex << int(val));

	switch(PortId(port & 0x0F)){
		case STATUS:
			/* do nothing */ ;
			break;
		default:
			ports[port] = val;
			break;
	}
}

void V9990::reset(const EmuTime &time)
{
	PRT_DEBUG("[" << time << "] V9990::reset");

	static const byte INITIAL_PORT_VALUES[16] = {
		0x00, 0x9F, 0xC0, 0xFF, 0xFF, 0x00, 0x03, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	
	memcpy(ports, INITIAL_PORT_VALUES, 16);
}

void V9990::changeRegister(byte reg, byte val, const EmuTime& time)
{
	if (reg < 54) {
		registers[reg] = val;
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
	return (address < 54)? parent.registers[address]: 0xFF;
}

void V9990::V9990RegDebug::write(unsigned address, byte value)
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	parent.changeRegister(address, value, time);
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
	return (address < 16)? parent.ports[address]: 0xFF;
}

void V9990::V9990PortDebug::write(unsigned address, byte value)
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	parent.writeIO(address, value, time);
}

} // end class V9990

