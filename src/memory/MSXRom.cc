// $Id$

#include "MSXRom.hh"
#include "MSXMotherBoard.hh"
#include "Rom.hh"

using std::string;

namespace openmsx {

MSXRom::MSXRom(MSXMotherBoard& motherBoard, const XMLElement& config,
               const EmuTime& time, std::auto_ptr<Rom> rom_)
	: MSXDevice(motherBoard, config, time), rom(rom_)
	, cpu(motherBoard.getCPU())
{
}

MSXRom::~MSXRom()
{
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
