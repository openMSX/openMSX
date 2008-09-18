// $Id$

#include "RomDRAM.hh"
#include "PanasonicMemory.hh"
#include "MSXMotherBoard.hh"
#include "Rom.hh"
#include "XMLElement.hh"
#include "serialize.hh"

namespace openmsx {

static unsigned calcBaseAddr(const XMLElement& config)
{
	int base = config.getChild("mem").getAttributeAsInt("base");
	int first = config.getChild("rom").getChildDataAsInt("firstblock");
	return first * 0x2000 - base;
}

RomDRAM::RomDRAM(MSXMotherBoard& motherBoard, const XMLElement& config,
                 std::auto_ptr<Rom> rom)
	: MSXRom(motherBoard, config, rom)
	, panasonicMemory(motherBoard.getPanasonicMemory())
	, baseAddr(calcBaseAddr(config))
{
	// ignore result, only called to trigger 'missing rom' error early
	panasonicMemory.getRomBlock(baseAddr);
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

REGISTER_MSXDEVICE(RomDRAM, "RomDRAM");

} // namespace openmsx
