// $Id$

#include "MSXRom.hh"
#include "MSXCPU.hh"
#include "Rom.hh"

using std::string;

namespace openmsx {

MSXCPU *MSXRom::cpu;

MSXRom::MSXRom(const XMLElement& config, const EmuTime& time,
               std::auto_ptr<Rom> rom_)
	: MSXDevice(config, time), rom(rom_)
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

	cpu = &MSXCPU::instance();
}


void MSXRom::writeMem(word /*address*/, byte /*value*/, const EmuTime& /*time*/)
{
	// nothing
}

byte* MSXRom::getWriteCacheLine(word /*address*/) const
{
	return unmappedWrite;
}

const string& MSXRom::getName() const
{
	return rom->getName();
}

} // namespace openmsx
