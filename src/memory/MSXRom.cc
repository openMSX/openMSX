// $Id$

#include "MSXRom.hh"
#include "MSXCPU.hh"


MSXCPU* MSXRom::cpu;


MSXRom::MSXRom(Device* config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time), rom(config, time)
{
	init();
}

MSXRom::~MSXRom()
{
}

void MSXRom::init()
{
	static bool alreadyInit = false;
	if (alreadyInit) {
		return;
	}
	alreadyInit = true;

	cpu = MSXCPU::instance();
}


void MSXRom::writeMem(word address, byte value, const EmuTime &time)
{
	// nothing
}

byte* MSXRom::getWriteCacheLine(word address) const
{
	return unmappedWrite;
}
