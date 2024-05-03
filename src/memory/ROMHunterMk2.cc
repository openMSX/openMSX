#include "ROMHunterMk2.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "serialize.hh"
#include <cassert>

/*

As reverse engineered by BiFi:

At 0x3FFF is a configuration register.
bit 1: page 1 control:
       0: ROM hunter ROM in page 1 for READs, writes ignored (default)
       1: writes go to RAM in page 1, mapper switching disabled
bits 2 and 0 is a mapper selector:
  00 = ASCII16 (default),
  01 = ASCII8,
  11 = Konami,
  10 = unknown (seems to be some Konami 16K variant)
bit 3: RAM write protect:
       0: write enable (default)
       1: write protect; also sets reads in page 1 to RAM (and not ROM)

When the ROM loads a MegaROM, bit 1 is left on 0. But for plain ROMs, it is set
to 1.
*/

namespace openmsx {

ROMHunterMk2::ROMHunterMk2(const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, ram(config, getName() + " RAM", "ROM Hunter Mk 2 RAM", 0x40000)
{
	reset(getCurrentTime());
}

void ROMHunterMk2::reset(EmuTime::param /*time*/)
{
	configReg = 0;
	ranges::fill(bankRegs, 0);
	invalidateDeviceRCache(); // flush all to be sure
}

unsigned ROMHunterMk2::getRamAddr(unsigned addr) const
{
	unsigned page = (addr >> 13) - 2;
	assert(page < 4);
	unsigned bank = bankRegs[page];
	return (bank * 0x2000) + (addr & 0x1FFF);
}

const byte* ROMHunterMk2::getReadCacheLine(word addr) const
{
	// reads outside [0x4000, 0xC000) return 0xFF
	if ((addr < 0x4000) || (0xC000 <= addr)) {
		return unmappedRead.data();
	}

	// if bit 3 is 0, reads from page 1 come from the ROM, else from the RAM
	if (((configReg & 0b1000) == 0) && (addr < 0x8000)) {
		return &rom[addr & 0x1FFF];
	} else {
		return &ram[getRamAddr(addr)];
	}
}

byte ROMHunterMk2::peekMem(word addr, EmuTime::param /*time*/) const
{
	return *getReadCacheLine(addr);
}

byte ROMHunterMk2::readMem(word addr, EmuTime::param time)
{
	return peekMem(addr, time); // reads have no side effects
}

void ROMHunterMk2::writeMem(word addr, byte value, EmuTime::param /*time*/)
{
	// config register at address 0x3FFF
	if (addr == 0x3FFF) {
		configReg = value;
		invalidateDeviceRCache(0x4000, 0x8000);
		return;
	}

	// ignore (other) writes outside [0x4000, 0xC000)
	if ((addr < 0x4000) || (0xC000 <= addr)) {
		return;
	}

	// address is calculated before writes to other regions take effect
	unsigned ramAddr = getRamAddr(addr);

	// only write mapper registers if bit 1 is not set
	if ((configReg & 0b10) == 0) {
		// (possibly) write to bank registers
		switch (configReg & 0b101) {
		case 0b000: {
			// ASCII-16
			// Implemented similarly as the (much later) MFR SCC+,
			// TODO did we verify that ROMHunterMk2 behaves the same?
			//
			// ASCII-16 uses all 4 bank registers and one bank
			// switch changes 2 registers at once. This matters
			// when switching mapper mode, because the content of
			// the bank registers is unchanged after a switch.
			const byte maskedValue = value & 0xF;
			if ((0x6000 <= addr) && (addr < 0x6800)) {
				bankRegs[0] = narrow_cast<byte>(2 * maskedValue + 0);
				bankRegs[1] = narrow_cast<byte>(2 * maskedValue + 1);
				invalidateDeviceRCache(0x4000, 0x4000);
			}
			if ((0x7000 <= addr) && (addr < 0x7800)) {
				bankRegs[2] = narrow_cast<byte>(2 * maskedValue + 0);
				bankRegs[3] = narrow_cast<byte>(2 * maskedValue + 1);
				invalidateDeviceRCache(0x8000, 0x4000);
			}
			break;
		}
		case 0b001:
			// ASCII-8
			if ((0x6000 <= addr) && (addr < 0x8000)) {
				byte bank = (addr >> 11) & 0x03;
				bankRegs[bank] = value & 0x1F;
				invalidateDeviceRCache(0x4000 + 0x2000 * bank, 0x2000);
			}
			break;
		case 0b101:
			// Konami
			if ((0x6000 <= addr) && (addr < 0xC000)) {
				unsigned bank = (addr >> 13) - 2;
				bankRegs[bank] = value & 0x1F;
				invalidateDeviceRCache(0x4000 + 0x2000 * bank, 0x2000);
			}
			break;
		case 0b100:
			// TODO how does this configuration behave?
			break;
		}
	}

	// write to RAM, if not write-protected
	if ((configReg & 0b1000) == 0) {
		// if write to [0x8000, 0xC000), just do it
		// if write to [0x4000, 0x8000), only do it if bit 1 is set
		if ((addr >= 0x8000) || ((configReg & 0b10) == 0b10)) {
			ram[ramAddr] = value;
		}
	}
}

byte* ROMHunterMk2::getWriteCacheLine(word /*addr*/)
{
	return nullptr;
}

template<typename Archive>
void ROMHunterMk2::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ram",       ram,
	             "configReg", configReg,
	             "bankRegs",  bankRegs);
}
INSTANTIATE_SERIALIZE_METHODS(ROMHunterMk2);
REGISTER_MSXDEVICE(ROMHunterMk2, "ROMHunterMk2");

} // namespace openmsx
