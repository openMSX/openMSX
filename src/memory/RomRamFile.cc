/**
 * Tecall MSX RAMFile TM220
 *
 * This is a cartridge with 16kB ROM and 16kB RAM (powered by a battery). It
 * offers BASIC commands to load/save files to this RAM. See manual for more
 * details:
 *     http://www.msxarchive.nl/pub/msx/graphics/jpg/hardware/manuals/
 *
 * The ROM is visible in region 0x4000-0x8000.
 * The RAM is visible in region 0x8000-0xC000.
 * Reading ROM/RAM works as expected, but writing to RAM is special.
 *
 * The /WE signal of the RAM chip (actually implemented as 2x8kB RAM chips) is
 * calculated from the /WE signal on the MSX cartridge slot plus 3 output bits
 * from a 74LS164 shift-register. The input of this shift-register is 'D2 & D6'
 * of the cartridge slot. The clock input of the shift-register is a
 * combination of the /CS1 and /M1 cartridge signals. This means that on each
 * (first byte of an) instruction fetch from the ROM, 2 bits of the fetched
 * opcode are combined and shifted in an 8-bit value. 3 of those shifted bits
 * need to have a specific value to allow write. See implementation below for
 * more details.
 *
 * So in summary, the TM220 monitors which instructions are fetched from ROM
 * and only allows to write to RAM after certain sequences of instructions.
 */

#include "RomRamFile.hh"
#include "MSXCPU.hh"
#include "narrow.hh"
#include "serialize.hh"
#include <memory>

namespace openmsx {

RomRamFile::RomRamFile(const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, cpu(getCPU())
{
	sram = std::make_unique<SRAM>(getName() + " SRAM", 0x4000, config);
	reset(EmuTime::dummy());
}

void RomRamFile::reset(EmuTime /*time*/)
{
	shiftValue = 0;
}

byte RomRamFile::readMem(uint16_t address, EmuTime /*time*/)
{
	if ((0x4000 <= address) && (address < 0x8000)) {
		byte result = rom[address & 0x1fff];
		if (cpu.isM1Cycle(address)) {
			bool tmp = (result & 0x44) == 0x44;
			shiftValue = narrow_cast<byte>((shiftValue << 1) | byte(tmp));
		}
		return result;
	} else if ((0x8000 <= address) && (address < 0xC000)) {
		return (*sram)[address & 0x3fff];
	} else {
		return 0xff;
	}
}

byte RomRamFile::peekMem(uint16_t address, EmuTime /*time*/) const
{
	if ((0x4000 <= address) && (address < 0x8000)) {
		return rom[address & 0x1fff];
	} else if ((0x8000 <= address) && (address < 0xC000)) {
		return (*sram)[address & 0x3fff];
	} else {
		return 0xff;
	}
}

const byte* RomRamFile::getReadCacheLine(uint16_t address) const
{
	if ((0x4000 <= address) && (address < 0x8000)) {
		// reads from ROM are not cacheable because of the M1 stuff
		return nullptr;
	} else if ((0x8000 <= address) && (address < 0xC000)) {
		return &(*sram)[address & 0x3fff];
	} else {
		return unmappedRead.data();
	}
}

void RomRamFile::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	if ((0x8000 <= address) && (address < 0xC000) &&
	    ((shiftValue & 0x31) == 0x11)) {
		sram->write(address & 0x3fff, value);
	}
}

byte* RomRamFile::getWriteCacheLine(uint16_t address)
{
	if ((0x8000 <= address) && (address < 0xC000)) {
		// writes to SRAM are not cacheable because of sync-to-disk
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void RomRamFile::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("shiftValue", shiftValue);
}
INSTANTIATE_SERIALIZE_METHODS(RomRamFile);
REGISTER_MSXDEVICE(RomRamFile, "RomRamFile");

} // namespace openmsx
