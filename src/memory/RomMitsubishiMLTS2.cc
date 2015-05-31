#include "RomMitsubishiMLTS2.hh"
#include "Rom.hh"
#include "Ram.hh"
#include "CacheLine.hh"
#include "serialize.hh"
#include "memory.hh"

#include <iostream>

namespace openmsx {

RomMitsubishiMLTS2::RomMitsubishiMLTS2(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
	, ram(make_unique<Ram>(config, getName() + " RAM", "ML-TS2 RAM", 0x2000))
{
	reset(EmuTime::dummy());
}

RomMitsubishiMLTS2::~RomMitsubishiMLTS2()
{
}

void RomMitsubishiMLTS2::reset(EmuTime::param /*time*/)
{
	setRom(2, 0);
}

void RomMitsubishiMLTS2::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if (address == 0x7FC0) {
		byte bank = (value & 1) + (((value >> 2) & 1) << 1) + (((value >> 4) & 1) << 2);
		std::cerr << "Setting MLTS2 mapper page 1 to bank " << int(bank) << std::endl;
		setRom(2, bank);
	} else if ((0x6000 <= address) && (address < 0x8000)) {
                (*ram)[address & 0x1FFF] = value;
        }
}

byte RomMitsubishiMLTS2::readMem(word address, EmuTime::param time)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
                return (*ram)[address & 0x1FFF];
	} else {
		return Rom8kBBlocks::readMem(address, time);
	}
}

byte RomMitsubishiMLTS2::peekMem(word address, EmuTime::param time) const
{
	if ((0x6000 <= address) && (address < 0x8000)) {
                return (*ram)[address & 0x1FFF];
	} else {
		return Rom8kBBlocks::peekMem(address, time);
	}
}

const byte* RomMitsubishiMLTS2::getReadCacheLine(word address) const
{
	if (address == (0x7FC0 & CacheLine::HIGH)) return nullptr;
	if ((0x6000 <= address) && (address < 0x8000)) {
                return &(*ram)[address & 0x1FFF];
	}
	if ((0x4000 <= address) && (address < 0x6000)) {
                return &(rom[address & 0x1FFF]);
	}
	return unmappedRead;
}

byte* RomMitsubishiMLTS2::getWriteCacheLine(word address) const
{
	if (address == (0x7FC0 & CacheLine::HIGH)) return nullptr;
	if ((0x6000 <= address) && (address < 0x8000)) {
                return &(*ram)[address & 0x1FFF];
	}
	return unmappedWrite;
}

REGISTER_MSXDEVICE(RomMitsubishiMLTS2, "RomMitsubishiMLTS2");

} // namespace openmsx
