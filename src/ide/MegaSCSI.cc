/*
 * MEGA-SCSI and ESE-RAM cartridge:
 *  The mapping does SRAM and MB89352A(MEGA-SCSI) to ASCII8 or
 *  an interchangeable bank controller.
 *
 * Specification:
 *  SRAM(MegaROM) controller: ASCII8 type
 *  SRAM capacity           : 128, 256, 512 and 1024KB
 *  SCSI Protocol Controller: Fujitsu MB89352A
 *
 * Bank changing address:
 *  bank 4(0x4000-0x5fff): 0x6000 - 0x67FF (0x6000 used)
 *  bank 6(0x6000-0x7fff): 0x6800 - 0x6FFF (0x6800 used)
 *  bank 8(0x8000-0x9fff): 0x7000 - 0x77FF (0x7000 used)
 *  bank A(0xa000-0xbfff): 0x7800 - 0x7FFF (0x7800 used)
 *
 * ESE-RAM Bank Map:
 *  BANK 0x00-0x7F (read only)
 *  BANK 0x80-0xFF (write and read. mirror of 0x00-0x7F)
 *
 * MEGA-SCSI Bank Map:
 *  BANK 0x00-0x3F (sram read only. mirror of 0x80-0xBF)
 *  BANK 0x40-0x7E (mirror of 0x7F. Use is prohibited)
 *  BANK 0x7F      (SPC)
 *  BANK 0x80-0xFF (sram write and read)
 *
 * SPC Bank:
 *  0x0000 - 0x0FFF :
 *      SPC Data register r/w (mirror of all 0x1FFA)
 *  0x1000 - 0x1FEF :
 *      mirror of 0x1FF0 - 0x1FFF
 *      Use is prohibited about the image
 *  0x1FF0 - 0x1FFE :
 *      SPC register
 *  0x1FFF :
 *      un mapped
 *
 * Note:
 *  It is possible to access it by putting it out to 0x8000 - 0xBFFF
 *  though the SPC bank is arranged in chiefly 0x4000-0x5FFF.
 */

#include "MegaSCSI.hh"
#include "MSXException.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {

static constexpr byte SPC = 0x7F;

size_t MegaSCSI::getSramSize() const
{
	size_t sramSize = getDeviceConfig().getChildDataAsInt("sramsize", 1024); // size in kb
	if (sramSize != one_of(1024u, 512u, 256u, 128u)) {
		throw MSXException(
			"SRAM size for ", getName(),
			" should be 128, 256, 512 or 1024kB and not ",
			sramSize, "kB!");
	}
	return sramSize * 1024; // in bytes
}

MegaSCSI::MegaSCSI(const DeviceConfig& config)
	: MSXDevice(config)
	, mb89352(config)
	, sram(getName() + " SRAM", getSramSize(), config)
	, romBlockDebug(*this, mapped, 0x4000, 0x8000, 13)
	, blockMask(narrow<byte>((sram.size() / 0x2000) - 1))
{
}

void MegaSCSI::reset(EmuTime::param /*time*/)
{
	for (auto i : xrange(4)) {
		setSRAM(i, 0);
	}
	mb89352.reset(true);
}

byte MegaSCSI::readMem(word address, EmuTime::param /*time*/)
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 0x2000) - 2;
		word addr = address & 0x1FFF;
		if (mapped[page] == SPC) {
			// SPC read
			if (addr < 0x1000) {
				// Data Register
				return mb89352.readDREG();
			} else {
				return mb89352.readRegister(addr & 0x0F);
			}
		} else {
			return sram[0x2000 * mapped[page] + addr];
		}
	} else {
		return 0xFF;
	}
}

byte MegaSCSI::peekMem(word address, EmuTime::param /*time*/) const
{
	if (const byte* cacheline = MegaSCSI::getReadCacheLine(address)) {
		return *cacheline;
	} else {
		address &= 0x1FFF;
		if (address < 0x1000) {
			return mb89352.peekDREG();
		} else {
			return mb89352.peekRegister(address & 0x0F);
		}
	}
}

const byte* MegaSCSI::getReadCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 0x2000) - 2;
		address &= 0x1FFF;
		if (mapped[page] == SPC) {
			return nullptr;
		} else {
			return &sram[0x2000 * mapped[page] + address];
		}
	} else {
		return unmappedRead.data();
	}
}

void MegaSCSI::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		byte region = ((address >> 11) & 3);
		setSRAM(region, value);
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 0x2000) - 2;
		address &= 0x1FFF;
		if (mapped[page] == SPC) {
			if (address < 0x1000) {
				mb89352.writeDREG(value);
			} else {
				mb89352.writeRegister(address & 0x0F, value);
			}
		} else if (isWriteable[page]) {
			sram.write(0x2000 * mapped[page] + address, value);
		}
	}
}

byte* MegaSCSI::getWriteCacheLine(word address)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		return nullptr;
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 0x2000) - 2;
		if (mapped[page] == SPC) {
			return nullptr;
		} else if (isWriteable[page]) {
			return nullptr;
		}
	}
	return unmappedWrite.data();
}

void MegaSCSI::setSRAM(unsigned region, byte block)
{
	invalidateDeviceRWCache(region * 0x2000 + 0x4000, 0x2000);
	assert(region < 4);
	isWriteable[region] = (block & 0x80) != 0;
	mapped[region] = ((block & 0xC0) == 0x40) ? 0x7F : (block & blockMask);
}

template<typename Archive>
void MegaSCSI::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("SRAM",        sram,
	             "MB89352",     mb89352,
	             "isWriteable", isWriteable,
	             "mapped",      mapped);
}
INSTANTIATE_SERIALIZE_METHODS(MegaSCSI);
REGISTER_MSXDEVICE(MegaSCSI, "MegaSCSI");

} // namespace openmsx
