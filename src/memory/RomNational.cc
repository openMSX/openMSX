// $Id$

#include "RomNational.hh"
#include "Device.hh"
#include "CPU.hh"


namespace openmsx {

RomNational::RomNational(Device* config, const EmuTime& time, Rom* rom)
	: MSXDevice(config, time), Rom16kBBlocks(config, time, rom),
	  sram(0x1000, config)
{
	reset(time);
}

RomNational::~RomNational()
{
}

void RomNational::reset(const EmuTime& time)
{
	control = 0;
	for (int region = 0; region < 4; region++) {
		setRom(region, 0);
		bankSelect[region] = 0;
	}
	sramAddr = 0;	// TODO check this
}

byte RomNational::readMem(word address, const EmuTime& time)
{
	if ((control & 0x04) && ((address & 0x7FF9) == 0x7FF0)) {
		// TODO check mirrored
		// 7FF0 7FF2 7FF4 7FF6   bank select read back
		int bank = (address & 6) / 2;
		return bankSelect[bank];
	}
	if ((control & 0x02) && ((address & 0x3FFF) == 0x3FFD)) {
		// SRAM read
		return sram.read(sramAddr++ & 0x0FFF);
	}
	return Rom16kBBlocks::readMem(address, time);
}

const byte* RomNational::getReadCacheLine(word address) const
{
	if ((0x3FF0 & CPU::CACHE_LINE_HIGH) == (address & 0x3FFF)) {
		return NULL;
	} else {
		return Rom16kBBlocks::getReadCacheLine(address);
	}
}

void RomNational::writeMem(word address, byte value, const EmuTime& time)
{
	// TODO bank switch address mirrored?
	if (address == 0x6000) {
		bankSelect[1] = value;
		setRom(1, value);	// !!
	} else if (address == 0x6400) {
		bankSelect[0] = value;
		setRom(0, value);	// !!
	} else if (address == 0x7000) {
		bankSelect[2] = value;
		setRom(2, value);
	} else if (address == 0x7400) {
		bankSelect[3] = value;
		setRom(3, value);
	} else if (address == 0x7FF9) {
		// write control byte
		control = value;
	} else if (control & 0x02) {
		address &= 0x3FFF;
		if (address == 0x3FFA) {
			// SRAM address bits 23-16
			sramAddr = (sramAddr & 0x00FFFF) | value << 16;
		} else if (address == 0x3FFB) {
			// SRAM address bits 15-8
			sramAddr = (sramAddr & 0xFF00FF) | value << 8;
		} else if (address == 0x3FFC) {
			// SRAM address bits 7-0
			sramAddr = (sramAddr & 0xFFFF00) | value;
		} else if (address == 0x3FFD) {
			sram.write(sramAddr++ & 0x0FFF, value);
		}
	}
}

byte* RomNational::getWriteCacheLine(word address) const
{
	if ((address == (0x6000 & CPU::CACHE_LINE_HIGH)) ||
	    (address == (0x6400 & CPU::CACHE_LINE_HIGH)) ||
	    (address == (0x7000 & CPU::CACHE_LINE_HIGH)) ||
	    (address == (0x7400 & CPU::CACHE_LINE_HIGH)) ||
	    (address == (0x7FF9 & CPU::CACHE_LINE_HIGH))) {
		return NULL;
	} else if ((address & 0x3FFF) == (0x3FFA & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx
