#include "RomDRAM.hh"
#include "PanasonicMemory.hh"
#include "MSXMotherBoard.hh"
#include "XMLElement.hh"
#include "serialize.hh"

namespace openmsx {

[[nodiscard]] static unsigned calcBaseAddr(const DeviceConfig& config)
{
	int base = config.getChild("mem").getAttributeValueAsInt("base", 0);
	int first = config.getChild("rom").getChildDataAsInt("firstblock", 0);
	return first * 0x2000 - base;
}

RomDRAM::RomDRAM(const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, panasonicMemory(getMotherBoard().getPanasonicMemory())
	, baseAddr(calcBaseAddr(config))
{
	// ignore result, only called to trigger 'missing rom' error early
	(void)panasonicMemory.getRomBlock(baseAddr);
}

byte RomDRAM::readMem(uint16_t address, EmuTime /*time*/)
{
	return *RomDRAM::getReadCacheLine(address);
}

const byte* RomDRAM::getReadCacheLine(uint16_t address) const
{
	unsigned addr = address + baseAddr;
	return &panasonicMemory.getRomBlock(addr >> 13)[addr & 0x1FFF];
}

REGISTER_MSXDEVICE(RomDRAM, "RomDRAM");

} // namespace openmsx
