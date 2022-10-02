// GAME MASTER 2
//
// This is a 1 megabit ROM cartridge with 8 Kb SRAM. Because of the SRAM,
// the mappers have special features.
//
// Since the size of the mapper is 8Kb, the memory banks are:
//   Bank 1: 4000h - 5FFFh
//   Bank 2: 6000h - 7FFFh
//   Bank 3: 8000h - 9FFFh
//   Bank 4: A000h - BFFFh
//
// And the addresses to change banks:
//   Bank 1: <none>
//   Bank 2: 6000h - 6FFFh (6000h used)
//   Bank 3: 8000h - 8FFFh (8000h used)
//   Bank 4: A000h - AFFFh (A000h used)
//   SRAM write: B000h - BFFFh
//
// If SRAM is selected in bank 4, you can write to it in the memory area
// B000h - BFFFh.
//
// The value you write to change banks also determines whether you select ROM
// or SRAM. SRAM can be in any memory bank (except bank 1 which can't be
// modified) but it can only be written too in bank 4.
//
//   bit      |  0 |  1 |  2 |  3 |      4       |  5 | 6 | 7 |
//   ----------------------------------------------------------
//   function | R0 | R1 | R2 | R3 | 1=SRAM/0=ROM | S0 | X | X |
//
// If bit 4 is reset, bits 0 - 3 select the ROM page as you would expect them
// to do. Bits 5 - 7 are ignored now. If bit 4 is set, bit 5 selects the SRAM
// page (first or second 4Kb of the 8Kb). Bits 6 - 7 and bits 0 - 3 are
// ignored now.
//
// Since you can only select 4Kb of the SRAM at once in a memory bank and a
// memory bank is 8Kb in size, the first and second 4Kb of the memory bank
// read the same 4Kb of SRAM if SRAM is selected.

#include "RomGameMaster2.hh"
#include "SRAM.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <memory>

namespace openmsx {

RomGameMaster2::RomGameMaster2(const DeviceConfig& config, Rom&& rom_)
	: Rom4kBBlocks(config, std::move(rom_), 1)
{
	sram = std::make_unique<SRAM>(getName() + " SRAM", 0x2000, config);
	reset(EmuTime::dummy());
}

void RomGameMaster2::reset(EmuTime::param /*time*/)
{
	for (auto i : xrange(4)) {
		setUnmapped(i);
	}
	for (auto i : xrange(4, 12)) {
		setRom(i, i - 4);
	}
	for (auto i : xrange(12, 16)) {
		setUnmapped(i);
	}
	sramOffset = 0;
	sramEnabled = false;
}

void RomGameMaster2::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((0x6000 <= address) && (address < 0xB000)) {
		if (!(address & 0x1000)) {
			byte region = address >> 12; // 0x6, 0x8 or 0xA
			if (region == 0x0A) {
				sramEnabled = (value & 0x10) != 0;
				invalidateDeviceWCache(0xB000, 0x1000); // 'R' is handled below
			}
			if (value & 0x10) {
				// switch SRAM
				sramOffset = (value & 0x20) ? 0x1000: 0x0000;
				setBank(region,     &(*sram)[sramOffset], value);
				setBank(region + 1, &(*sram)[sramOffset], value);
			} else {
				// switch ROM
				setRom(region,     2 * (value & 0x0F));
				setRom(region + 1, 2 * (value & 0x0F) + 1);
			}
		}
	} else if ((0xB000 <= address) && (address < 0xC000)) {
		// write SRAM
		if (sramEnabled) {
			sram->write(sramOffset | (address & 0x0FFF), value);
		}
	}
}

byte* RomGameMaster2::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0xB000)) {
		if (!(address & 0x1000)) {
			return nullptr;
		} else {
			return unmappedWrite.data();
		}
	} else if ((0xB000 <= address) && (address < 0xC000) && sramEnabled) {
		// write SRAM
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void RomGameMaster2::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom4kBBlocks>(*this);
	ar.serialize("sramOffset",  sramOffset,
	             "sramEnabled", sramEnabled);
}
INSTANTIATE_SERIALIZE_METHODS(RomGameMaster2);
REGISTER_MSXDEVICE(RomGameMaster2, "RomGameMaster2");

} // namespace openmsx
