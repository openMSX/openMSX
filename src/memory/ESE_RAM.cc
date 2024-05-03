/*
 * MEGA-SCSI and ESE-RAM cartridge:
 *  The mapping does SRAM and MB89352A(MEGA-SCSI) to ASCII8 or
 *  an interchangeable bank controller.
 *  The rest of this documentation is only about ESE-RAM specifically.
 *
 * Specification:
 *  SRAM(MegaROM) controller: ASCII8 type
 *  SRAM capacity           : 128, 256, 512 and 1024KB
 *
 * Bank changing address:
 *  bank 4(0x4000-0x5fff): 0x6000 - 0x67FF (0x6000 used)
 *  bank 6(0x6000-0x7fff): 0x6800 - 0x6FFF (0x6800 used)
 *  bank 8(0x8000-0x9fff): 0x7000 - 0x77FF (0x7000 used)
 *  bank A(0xa000-0xbfff): 0x7800 - 0x7FFF (0x7800 used)
 *
 * ESE-RAM Bank Map:
 *  BANK 00H-7FH (read only)
 *  BANK 80H-FFH (write and read. mirror of 00H-7FH)
 */

#include "ESE_RAM.hh"
#include "MSXException.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {

size_t ESE_RAM::getSramSize() const
{
	size_t sramSize = getDeviceConfig().getChildDataAsInt("sramsize", 256); // size in kb
	if (sramSize != one_of(1024u, 512u, 256u, 128u)) {
		throw MSXException(
			"SRAM size for ", getName(),
			" should be 128, 256, 512 or 1024kB and not ",
			sramSize, "kB!");
	}
	return sramSize * 1024; // in bytes
}

ESE_RAM::ESE_RAM(const DeviceConfig& config)
	: MSXDevice(config)
	, sram(getName() + " SRAM", getSramSize(), config)
	, romBlockDebug(*this, mapped, 0x4000, 0x8000, 13)
	, blockMask(narrow<byte>((sram.size() / 0x2000) - 1))
{
	reset(EmuTime::dummy());
}

void ESE_RAM::reset(EmuTime::param /*time*/)
{
	for (auto i : xrange(4)) {
		setSRAM(i, 0);
	}
}

byte ESE_RAM::readMem(word address, EmuTime::param /*time*/)
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 8192) - 2;
		word addr = address & 0x1FFF;
		return sram[8192 * mapped[page] + addr];
	} else {
		return 0xFF;
	}
}

const byte* ESE_RAM::getReadCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 8192) - 2;
		address &= 0x1FFF;
		return &sram[8192 * mapped[page] + address];
	} else {
		return unmappedRead.data();
	}
}

void ESE_RAM::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		byte region = ((address >> 11) & 3);
		setSRAM(region, value);
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 8192) - 2;
		address &= 0x1FFF;
		if (isWriteable[page]) {
			sram.write(8192 * mapped[page] + address, value);
		}
	}
}

byte* ESE_RAM::getWriteCacheLine(word address)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		return nullptr;
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 8192) - 2;
		if (isWriteable[page]) {
			return nullptr;
		}
	}
	return unmappedWrite.data();
}

void ESE_RAM::setSRAM(unsigned region, byte block)
{
	invalidateDeviceRWCache(region * 0x2000 + 0x4000, 0x2000);
	assert(region < 4);
	isWriteable[region] = (block & 0x80) != 0;
	mapped[region] = block & blockMask;
}

template<typename Archive>
void ESE_RAM::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("SRAM",        sram,
	             "isWriteable", isWriteable,
	             "mapped",      mapped);
}
INSTANTIATE_SERIALIZE_METHODS(ESE_RAM);
REGISTER_MSXDEVICE(ESE_RAM, "ESE_RAM");

} // namespace openmsx
