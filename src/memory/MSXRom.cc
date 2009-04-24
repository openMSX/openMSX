// $Id$

#include "MSXRom.hh"
#include "Rom.hh"
#include "RomInfo.hh"
#include "TclObject.hh"

namespace openmsx {

MSXRom::MSXRom(MSXMotherBoard& motherBoard, const XMLElement& config,
               std::auto_ptr<Rom> rom_)
	: MSXDevice(motherBoard, config, rom_->getName())
	, rom(rom_)
{
}

MSXRom::~MSXRom()
{
}

void MSXRom::writeMem(word /*address*/, byte /*value*/, EmuTime::param /*time*/)
{
	// nothing
}

byte* MSXRom::getWriteCacheLine(word /*address*/) const
{
	return unmappedWrite;
}

void MSXRom::setRomType(RomType type_)
{
	type = type_;
}

void MSXRom::getExtraDeviceInfo(TclObject& result) const
{
	result.addListElement(RomInfo::romTypeToName(type));
	// add sha1sum, to be able to get a unique key for this ROM device,
	// so that it can be used to look up things in databases
	result.addListElement(rom->getOriginalSHA1());
}

} // namespace openmsx
