// $Id$

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
 *
 */

#include "ESE_RAM.hh"
#include "SRAM.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include "XMLElement.hh"
#include <cassert>

namespace openmsx {

ESE_RAM::ESE_RAM(MSXMotherBoard& motherBoard, const XMLElement& config,
                   const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, cpu(motherBoard.getCPU())
{
	unsigned sramSize = config.getChildDataAsInt("sramsize", 256); // size in kb
	if (sramSize != 1024 && sramSize != 512 && sramSize != 256 && sramSize != 128) {
		throw MSXException("SRAM size for " + getName() + 
			" should be 128, 256, 512 or 1024kB and not " + 
			StringOp::toString(sramSize) + "kB!");
	}
	sramSize *= 1024; // in bytes
	sram.reset(new SRAM(motherBoard, getName() + " SRAM", sramSize, config));
	blockMask = (sramSize / 8192) - 1;
}

ESE_RAM::~ESE_RAM()
{
}

void ESE_RAM::reset(const EmuTime& /*time*/)
{
	for (int i = 0; i < 4; ++i) {
		setSRAM(i, 0);
	}
}

byte ESE_RAM::readMem(word address, const EmuTime& /*time*/)
{
	byte result;
	if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 8192) - 2;
		word addr = address & 0x1FFF;
		result = (*sram)[8192 * mapped[page] + addr];
	} else {
		result = 0xFF;
	}
	return result;
}

const byte* ESE_RAM::getReadCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 8192) - 2;
		address &= 0x1FFF;
		return &(*sram)[8192 * mapped[page] + address];
	} else {
		return unmappedRead;
	}
}

void ESE_RAM::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		byte region = ((address >> 11) & 3);
		setSRAM(region, value);
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 8192) - 2;
		address &= 0x1FFF;
		if (isWriteable[page]) {
			sram->write(8192 * mapped[page] + address, value);
		}
	}
}

byte* ESE_RAM::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		return NULL;
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		unsigned page = (address / 8192) - 2;
		if (isWriteable[page]) {
			return NULL;
		}
	}
	return unmappedWrite;
}

void ESE_RAM::setSRAM(unsigned region, byte block)
{
	cpu.invalidateMemCache(region * 0x2000 + 0x4000, 0x2000);
	assert(region < 4);
	isWriteable[region] = block & 0x80;
	mapped[region] = block & blockMask;
}

} // namespace openmsx
