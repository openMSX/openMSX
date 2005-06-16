// $Id$

#include "RomDRAM.hh"
#include "PanasonicMemory.hh"
#include "MSXMotherBoard.hh"
#include "Rom.hh"
#include "XMLElement.hh"

namespace openmsx {

RomDRAM::RomDRAM(MSXMotherBoard& motherBoard, const XMLElement& config,
                 const EmuTime& time, std::auto_ptr<Rom> rom)
	: MSXRom(motherBoard, config, time, rom)
	, panasonicMemory(motherBoard.getPanasonicMemory())
{
	int base = config.getChild("mem").getAttributeAsInt("base");
	int first = config.getChild("rom").getChildDataAsInt("firstblock");
	baseAddr = first * 0x2000 - base;
}

RomDRAM::~RomDRAM()
{
}

byte RomDRAM::readMem(word address, const EmuTime& /*time*/)
{
	return *getReadCacheLine(address);
}

const byte* RomDRAM::getReadCacheLine(word address) const
{
	unsigned addr = address + baseAddr;
	return &panasonicMemory.getRomBlock(addr >> 13)[addr & 0x1FFF];
}

} // namespace openmsx
