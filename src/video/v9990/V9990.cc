// $Id$ //

/*
 * The Graphics9000 class
 * 
 * TODO:
 * - Everything :-)
 */

#include <iomanip>
#include <cassert>

#include "V9990.hh"

#include "Scheduler.hh"
#include "MSXConfig.hh"
#include "Config.hh"

namespace openmsx {
 
V9990::V9990(Config *config, const EmuTime &time)
	: MSXDevice(config, time),
	  MSXIODevice(config, time)
{
	PRT_DEBUG("[" << time << "] V9990::V9990");
}

V9990::~V9990()
{
	PRT_DEBUG("[--now-- ] V9990::~V9990");
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
}

} // end class V9990

