// $Id$

#include "MSXRom.hh"
#include "MSXCPU.hh"
#include "File.hh"


MSXCPU* MSXRom::cpu;


MSXRom::MSXRom(Device* config, const EmuTime &time, Rom *rom)
	: MSXMemDevice(config, time), MSXDevice(config, time)
{
	init();
	this->rom = rom;

	std::ostringstream tmpname;
	if (rom->getInfo().getId().length()==0)
	{
		tmpname << "Unknown ROM: " << rom->getFile()->getURL();
	}
	else
	{
		tmpname << rom->getInfo().getId() << ", " << rom->getInfo().getCompany() << " " << rom->getInfo().getYear();
	}

	romName = tmpname.str(); 
}

MSXRom::~MSXRom()
{
	delete rom;
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

const std::string &MSXRom::getName() const
{
	return romName;
}
