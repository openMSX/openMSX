// $Id$

#include "PanasonicRom.hh"
#include "PanasonicMemory.hh"
#include "MSXConfig.hh"


PanasonicRom::PanasonicRom(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time)
{
	PRT_INFO("PanasonicRom device is deprecated, "
	         "please upgrade your (turbor) machine configuration.");
	block = config->getParameterAsInt("block");
}

PanasonicRom::~PanasonicRom()
{
}

byte PanasonicRom::readMem(word address, const EmuTime &time)
{
	int block2 = (address & 0x2000) ? block + 1 : block;
	const byte* rom = PanasonicMemory::instance()->getRomBlock(block2);
	return rom[address & 0x1FFF];
}

const byte* PanasonicRom::getReadCacheLine(word address) const
{
	int block2 = (address & 0x2000) ? block + 1 : block;
	const byte* rom = PanasonicMemory::instance()->getRomBlock(block2);
	return &rom[address & 0x1FFF];
}

void PanasonicRom::writeMem(word address, byte value, const EmuTime &time)
{
	// nothing
}

byte* PanasonicRom::getWriteCacheLine(word address) const
{
	return unmappedWrite;
}
