// ASCII 8kB based cartridges with SRAM
//   - ASCII8-8  /  KOEI-8  /  KOEI-32  /  WIZARDRY / ASCII8-2
//
// The address to change banks:
//  bank 1: 0x6000 - 0x67ff (0x6000 used)
//  bank 2: 0x6800 - 0x6fff (0x6800 used)
//  bank 3: 0x7000 - 0x77ff (0x7000 used)
//  bank 4: 0x7800 - 0x7fff (0x7800 used)
//
//  To select SRAM set bit 7 (for WIZARDRY) or the bit just above the
//  rom selection bits (bit 5/6/7 depending on ROM size). For KOEI-32
//  the lowest bits indicate which SRAM page is selected. SRAM is
//  readable at 0x8000-0xBFFF. For the KOEI-x types SRAM is also
//  readable at 0x4000-0x5FFF

#include "RomAscii8_8.hh"
#include "SRAM.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <memory>

namespace openmsx {

RomAscii8_8::RomAscii8_8(const DeviceConfig& config,
                         Rom&& rom_, SubType subType)
	: Rom8kBBlocks(config, std::move(rom_))
	, sramEnableBit((subType == SubType::WIZARDRY) ? 0x80
	                                      : narrow_cast<byte>(rom.size() / BANK_SIZE))
	, sramPages((subType == one_of(SubType::KOEI_8, SubType::KOEI_32)) ? 0x34 : 0x30)
{
	unsigned size = (subType == one_of(SubType::KOEI_32, SubType::ASCII8_32)) ? 0x8000  // 32kB
	              : (subType == SubType::ASCII8_2) ? 0x0800  //  2kB
	                                      : 0x2000; //  8kB
	sram = std::make_unique<SRAM>(getName() + " SRAM", size, config);
	reset(EmuTime::dummy());
}

void RomAscii8_8::reset(EmuTime::param /*time*/)
{
	setUnmapped(0);
	setUnmapped(1);
	for (auto i : xrange(2, 6)) {
		setRom(i, 0);
	}
	setUnmapped(6);
	setUnmapped(7);

	sramEnabled = 0;
}

byte RomAscii8_8::readMem(word address, EmuTime::param time)
{
	auto bank = narrow<byte>(address / BANK_SIZE);
	if ((1 << bank) & sramEnabled) {
		// read from SRAM (possibly mirror)
		auto addr = (sramBlock[bank] * BANK_SIZE)
		          + (address & (sram->size() - 1) & BANK_MASK);
		return (*sram)[addr];
	} else {
		return Rom8kBBlocks::readMem(address, time);
	}
}

const byte* RomAscii8_8::getReadCacheLine(word address) const
{
	auto bank = narrow<byte>(address / BANK_SIZE);
	if ((1 << bank) & sramEnabled) {
		// read from SRAM (possibly mirror)
		auto addr = (sramBlock[bank] * BANK_SIZE)
		          + (address & (sram->size() - 1) & BANK_MASK);
		return &(*sram)[addr];
	} else {
		return Rom8kBBlocks::getReadCacheLine(address);
	}
}

void RomAscii8_8::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		// bank switching
		byte region = ((address >> 11) & 3) + 2;
		if (value & sramEnableBit) {
			auto mask = narrow_cast<byte>((sram->size() + BANK_MASK) / BANK_SIZE - 1); // round up;
			sramEnabled |= (1 << region) & sramPages;
			sramBlock[region] = value & mask;
			setBank(region, &(*sram)[sramBlock[region] * BANK_SIZE], value);
			invalidateDeviceRCache(0x2000 * region, 0x2000); // do not cache
		} else {
			sramEnabled &= byte(~(1 << region));
			setRom(region, value);
		}
		// 'R' is already handled
		invalidateDeviceWCache(0x2000 * region, 0x2000);
	} else {
		auto bank = narrow<byte>(address / BANK_SIZE);
		if ((1 << bank) & sramEnabled) {
			// write to SRAM (possibly mirror)
			auto addr = (sramBlock[bank] * BANK_SIZE)
			          + (address & (sram->size() - 1) & BANK_MASK);
			sram->write(addr, value);
		}
	}
}

byte* RomAscii8_8::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		// bank switching
		return nullptr;
	} else if ((1 << (address / BANK_SIZE)) & sramEnabled) {
		// write to SRAM
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void RomAscii8_8::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
	ar.serialize("sramEnabled", sramEnabled,
	             "sramBlock",   sramBlock);
}
INSTANTIATE_SERIALIZE_METHODS(RomAscii8_8);
REGISTER_MSXDEVICE(RomAscii8_8, "RomAscii8_8");

} // namespace openmsx
