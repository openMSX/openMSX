#include "ROMHunterMk2.hh"
#include "CacheLine.hh"
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

ROMHunterMk2::ROMHunterMk2(
		const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, ram(config, getName() + " RAM", "ROM Hunter Mk 2 RAM", 0x40000)
{
	reset(getCurrentTime());
}

ROMHunterMk2::~ROMHunterMk2()
{
}

void ROMHunterMk2::reset(EmuTime::param /*time*/)
{
	configReg = 0;
	for (int i = 0; i < 4; ++i) {
		bankRegs[i] = 0;
	}
	invalidateMemCache(0x0000, 0x10000); // flush all to be sure
}

unsigned ROMHunterMk2::getRamAddr(unsigned addr) const
{
	unsigned page = (addr >> 13) - 2;
	if (page >= 4) {
		// Bank: -2, -1, 4, 5. So not mapped in this region,
		// returned value should not be used. But querying it
		// anyway is easier, see start of writeMem().
		return unsigned(-1);
	}
	unsigned bank = bankRegs[page];
	return (bank * 0x2000) + (addr & 0x1FFF);
}

byte ROMHunterMk2::peekMem(word addr, EmuTime::param /*time*/) const
{
	if ((0x4000 <= addr) && (addr < 0xC000)) {
		// if bit 3 is 0, reads from page 1 go from the ROM, else from the RAM
		if (((configReg & 0b1000) == 0) && (addr < 0x8000)) {
			// read ROM
			return rom[addr & 0x1FFF];
		} else {
			// read RAM content
			unsigned ramAddr = getRamAddr(addr);
			assert(ramAddr != unsigned(-1));
			return ram[ramAddr];
		}
	} else {		
		// unmapped read
		return 0xFF;
	}
}

byte ROMHunterMk2::readMem(word addr, EmuTime::param time)
{
	// peek is good enough, no side effects expected
	return peekMem(addr, time);
}

const byte* ROMHunterMk2::getReadCacheLine(word addr) const
{
	if ((0x4000 <= addr) && (addr < 0xC000)) {
		// if bit 3 is 0, reads from page 1 go from the ROM, else from the RAM
		if (((configReg & 0b1000) == 0) && (addr < 0x8000)) {
			return &rom[addr & 0x1FFF];
		} else {
			// read RAM content
			unsigned ramAddr = getRamAddr(addr);
			assert(ramAddr != unsigned(-1));
			return &ram[ramAddr];
		}
	} else {		
		return unmappedRead;
	}
}

void ROMHunterMk2::writeMem(word addr, byte value, EmuTime::param /*time*/)
{
	if (addr == 0x3FFF)
	{
		configReg = value;
		invalidateMemCache(0x4000, 0x8000);
	} else {
		// address is calculated before writes to other regions take effect
		unsigned ramAddr = getRamAddr(addr);

		unsigned page8kB = (addr >> 13) - 2;
		// only write mapper registers if bit 1 is not set
		if (((configReg & 0b10) == 0) && (page8kB < 4)) {
			// (possibly) write to bank registers
			switch (configReg & 0b101) {
			case 0b000: {
				// ASCII-16
				// Doing something similar as the much later MFR SCC+:
				// This behaviour is confirmed by Manuel Pazos (creator
				// of the cartridge): ASCII-16 uses all 4 bank registers
				// and one bank switch changes 2 registers at once.
				// This matters when switching mapper mode, because
				// the content of the bank registers is unchanged after
				// a switch.
				const byte maskedValue = value & 0xF;
				if ((0x6000 <= addr) && (addr < 0x6800)) {
					bankRegs[0] = 2 * maskedValue + 0;
					bankRegs[1] = 2 * maskedValue + 1;
					invalidateMemCache(0x4000, 0x4000);
				}
				if ((0x7000 <= addr) && (addr < 0x7800)) {
					bankRegs[2] = 2 * maskedValue + 0;
					bankRegs[3] = 2 * maskedValue + 1;
					invalidateMemCache(0x8000, 0x4000);
				}
				break;
			}
			case 0b001:
				// ASCII-8
				if ((0x6000 <= addr) && (addr < 0x8000)) {
					byte bank = (addr >> 11) & 0x03;
					bankRegs[bank] = value & 0x1F;
					invalidateMemCache(0x4000 + 0x2000 * bank, 0x2000);
				}
				break;
			case 0b101: {
				// Konami
				if ((0x6000 <= addr) && (addr < 0xC000)) {
					bankRegs[page8kB] = value & 0x1F;
					invalidateMemCache(0x4000 + 0x2000 * page8kB, 0x2000);
				}
				break;
			}
			default:
				// NOT IMPLEMENTED
				break;
			}
		}

		// write to RAM
		if (((configReg & 0b1000) == 0) && (0x4000 <= addr) && (addr < 0xC000)) {
			// write protect is off, so:
			// if write to page 2, just do it
			// if write to page 1, only do it if bit 1 is set
			if ((addr >= 0x8000) || ((addr >= 0x4000) && ((configReg & 0b10) == 0b10))) {
				assert(ramAddr != unsigned(-1));
				ram[ramAddr] = value;
			}
		}
	}
}

byte* ROMHunterMk2::getWriteCacheLine(word /*addr*/) const
{
	return nullptr;
}

template<typename Archive>
void ROMHunterMk2::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("ram", ram);

	ar.serialize("configReg", configReg);
	ar.serialize("bankRegs", bankRegs);
}
INSTANTIATE_SERIALIZE_METHODS(ROMHunterMk2);
REGISTER_MSXDEVICE(ROMHunterMk2, "ROMHunterMk2");

} // namespace openmsx
