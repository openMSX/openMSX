// $Id$

#include "MSXRom.hh"
#include "RomTypes.hh"
#include "MSXCPU.hh"
#include "File.hh"
#include <sstream>

using std::ostringstream;


namespace openmsx {

MSXCPU *MSXRom::cpu;

MSXRom::MSXRom(Config* config, const EmuTime& time, auto_ptr<Rom> rom_)
	: MSXDevice(config, time), MSXMemDevice(config, time),
	  rom(rom_)
{
	init();

	ostringstream tmpname;
	if (rom->getSize() == 0) {
		tmpname << "Empty ROM (with SCC)"; // valid assumption??
	} else if (rom->getInfo().getTitle().empty()) {
		tmpname << "Unknown ROM";
		if (rom->getFile()) {
			tmpname << ": " << rom->getFile()->getURL();
		}
	} else {
		tmpname << rom->getInfo().getTitle();
		if (!rom->getInfo().getCompany().empty()) {
			tmpname << ", " << rom->getInfo().getCompany()
				<< " " << rom->getInfo().getYear();
		}
	}
	romName = tmpname.str(); 
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

	cpu = &MSXCPU::instance();
}


void MSXRom::writeMem(word address, byte value, const EmuTime& time)
{
	// nothing
}

byte* MSXRom::getWriteCacheLine(word address) const
{
	return unmappedWrite;
}

const string& MSXRom::getName() const
{
	return romName;
}

} // namespace openmsx
