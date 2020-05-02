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
 *  BANK 00H-7FH (read only)
 *  BANK 80H-FFH (write and read. mirror of 00H-7FH)
 *
 * MEGA-SCSI Bank Map:
 *  BANK 00H-3FH (sram read only. mirror of 80H-BFH)
 *  BANK 40H-7EH (mirror of 7FH. Use is prohibited)
 *  BANK 7FH     (SPC)
 *  BANK 80H-FFH (sram write and read)
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
 *  It is possible to access it by putting it out to 8000H - BFFFH
 *  though the SPC bank is arranged in chiefly 4000H-5FFF.
 */

#include "MegaSCSI.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

constexpr byte SPC = 0x7F;

unsigned MegaSCSI::getSramSize() const
{
	unsigned sramSize = getDeviceConfig().getChildDataAsInt("sramsize", 1024); // size in kb
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
	, blockMask((sram.getSize() / 0x2000) - 1)
{
}

void MegaSCSI::reset(EmuTime::param /*time*/)
{
	for (int i = 0; i < 4; ++i) {
		setSRAM(i, 0);
	}
	mb89352.reset(true);
}

byte MegaSCSI::readMem(word address, EmuTime::param /*time*/)
{
	byte result;
	if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 0x2000) - 2;
		word addr = address & 0x1FFF;
		if (mapped[page] == SPC) {
			// SPC read
			if (addr < 0x1000) {
				// Data Register
				result = mb89352.readDREG();
			} else {
				result = mb89352.readRegister(addr & 0x0F);
			}
		} else {
			result = sram[0x2000 * mapped[page] + addr];
		}
	} else {
		result = 0xFF;
	}
	return result;
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
		return unmappedRead;
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

byte* MegaSCSI::getWriteCacheLine(word address) const
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
	return unmappedWrite;
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
