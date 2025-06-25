// ASCII 16kB based cartridges with SRAM
//
// Examples 2kB SRAM: Daisenryaku, Harry Fox MSX Special, Hydlide 2, Jyansei
// Examples 8kB SRAM: A-Train
//
// this type is is almost completely a ASCII16 cartridge
// However, it has 2 or 8kB of SRAM (and 128 kB ROM)
// Use value 0x10 to select the SRAM.
// SRAM in page 1 => read-only
// SRAM in page 2 => read-write
// The SRAM is mirrored in the 16 kB block
//
// The address to change banks (from ASCII16):
//  first  16kb: 0x6000 - 0x67FF (0x6000 used)
//  second 16kb: 0x7000 - 0x77FF (0x7000 and 0x77FF used)

#include "RomAscii16_2.hh"
#include "SRAM.hh"
#include "serialize.hh"
#include <memory>

namespace openmsx {

RomAscii16_2::RomAscii16_2(const DeviceConfig& config, Rom&& rom_, SubType subType)
	: RomAscii16kB(config, std::move(rom_))
{
	unsigned size = (subType == SubType::ASCII16_8) ? 0x2000 // 8kB
	                                                : 0x0800; // 2kB
	sram = std::make_unique<SRAM>(getName() + " SRAM", size, config);
	reset(EmuTime::dummy());
}

void RomAscii16_2::reset(EmuTime time)
{
	sramEnabled = 0;
	RomAscii16kB::reset(time);
}

byte RomAscii16_2::readMem(uint16_t address, EmuTime time)
{
	if ((1 << (address >> 14)) & sramEnabled) {
		return (*sram)[address & (sram->size() - 1)];
	} else {
		return RomAscii16kB::readMem(address, time);
	}
}

const byte* RomAscii16_2::getReadCacheLine(uint16_t address) const
{
	if ((1 << (address >> 14)) & sramEnabled) {
		return &(*sram)[address & (sram->size() - 1)];
	} else {
		return RomAscii16kB::getReadCacheLine(address);
	}
}

void RomAscii16_2::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	if ((0x6000 <= address) && (address < 0x7800) && !(address & 0x0800)) {
		// bank switch
		byte region = ((address >> 12) & 1) + 1;
		if (value == 0x10) {
			// SRAM block
			sramEnabled |= (1 << region);
		} else {
			// ROM block
			setRom(region, value);
			sramEnabled &= byte(~(1 << region));
		}
		invalidateDeviceRWCache(0x4000 * region, 0x4000);
	} else {
		// write sram
		if ((1 << (address >> 14)) & sramEnabled & 0x04) {
			sram->write(address & (sram->size() - 1), value);
		}
	}
}

byte* RomAscii16_2::getWriteCacheLine(uint16_t address)
{
	if ((1 << (address >> 14)) & sramEnabled & 0x04) {
		// write sram
		return nullptr;
	} else {
		return RomAscii16kB::getWriteCacheLine(address);
	}
}

template<typename Archive>
void RomAscii16_2::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<RomAscii16kB>(*this);
	ar.serialize("sramEnabled", sramEnabled);
}
INSTANTIATE_SERIALIZE_METHODS(RomAscii16_2);
REGISTER_MSXDEVICE(RomAscii16_2, "RomAscii16_2");

} // namespace openmsx
