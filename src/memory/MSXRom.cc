// $Id$

#include "MSXRom.hh"
#include "MSXCPU.hh"


byte MSXRom::unmapped[UNMAPPED_SIZE];
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

	memset(unmapped, 0xFF, UNMAPPED_SIZE);
	cpu = MSXCPU::instance();
}
