// $Id$

// FS-A1FM mappers
//   based on info from:
//   http://sourceforge.net/tracker/index.php?func=detail&aid=672508&group_id=38274&atid=421864
//
// Slot 3-1
//   0x4000-0x5FFF  16 x 8kB ROM, same 16 pages as mapper in slot 3-3
//   0x6000-0x7FFF  8kB RAM   (mapped ???)
//   0x7FC0-0x7FCF  I/O area
//     0x7FC0 (R/W)   8251 (UART) data register
//     0x7FC1 (R/W)   8251 (UART) command/status register
//     0x7FC4 (?)     ROM switch address, lower 4 bits -> selected bank
//                                        upper 4 bits -> ??? (always preserved)
//     0x7FC5 (W)     ???
//     0x7FC6 (R)     ???
//     0x7FC8 (W)     ???
//     0x7FCC (W)     ???
//
// Slot 3-3
//   Very similar to other Panasonic-8kB rom mappers
//
//   0x0000-0x1FFF  switch address 0x6400, read-back 0x7FF0, initial value 0xA8
//   0x2000-0x3FFF  switch address 0x6C00, read-back 0x7FF1, initial value 0xA8
//   0x4000-0x5FFF  switch address 0x6000, read-back 0x7FF2, initial value 0xA8
//   0x6000-0x7FFF  switch address 0x6800, read-back 0x7FF3, initial value 0xA8
//   0x8000-0x9FFF  switch address 0x7000, read-back 0x7FF4, initial value 0xA8
//   0xA000-0xBFFF  switch address 0x7800, read-back 0x7FF5, initial value 0xA8
//
//   0x7FF6-0x7FF7  always read 0
//   0x7FF9         control byte, bit2 activates 0x7FF0-0x7FF7
//
//   mapper pages 0x10-0x7F            mirrored at 0x90-0xFF
//                0x80-0x83 0x88-0x8B  unmapped (read 0xFF)
//                0x84-0x87 0x8C-0x8F  contain (same) 8kB RAM

#include "RomFSA1FM.hh"
#include "xmlx.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include "Rom.hh"

namespace openmsx {

// common sram //

FSA1FMRam::FSA1FMRam(const XMLElement& config)
	: sram(config.getAttribute("id") + " SRAM", 0x2000, config)
{
}

FSA1FMRam::~FSA1FMRam()
{
}

byte* FSA1FMRam::getSRAM(const XMLElement& config)
{
	static FSA1FMRam oneInstance(config);
	return &oneInstance.sram[0];
}


// Mapper for slot 3-1 //

RomFSA1FM1::RomFSA1FM1(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom)
	: MSXDevice(config, time), MSXRom(config, time, rom)
{
	sram = FSA1FMRam::getSRAM(config);
	reset(time);
}

RomFSA1FM1::~RomFSA1FM1()
{
}

void RomFSA1FM1::reset(const EmuTime& /*time*/)
{
	// initial rom bank is undefined
}

byte RomFSA1FM1::readMem(word address, const EmuTime& /*time*/)
{
	if ((0x4000 <= address) && (address < 0x6000)) {
		// read rom
		return (*rom)[(0x2000 * (sram[0x1FC4] & 0x0F)) + 
		              (address & 0x1FFF)];
	} else if ((0x7FC0 <= address) && (address < 0x7FD0)) {
		switch (address & 0x0F) {
		case 4:
			return sram[address & 0x1FFF];
		case 6:
			return frontSwitch.getStatus() ? 0xFB : 0xFF;
		default:
			return 0xFF;
		}
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		// read sram
		// TODO are there multiple sram blocks?
		return sram[address & 0x1FFF];
	} else {
		return 0xFF;
	}
}

const byte* RomFSA1FM1::getReadCacheLine(word address) const
{
	if (address == (0x7FC0 & CPU::CACHE_LINE_HIGH)) {
		// dont't cache IO area
		return NULL;
	} else if ((0x4000 <= address) && (address < 0x6000)) {
		// read rom
		return &(*rom)[(0x2000 * (sram[0x1FC4] & 0x0F)) +
		               (address & 0x1FFF)];
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		// read sram
		return &sram[address & 0x1FFF];
	} else {
		return unmappedRead;
	}
}

void RomFSA1FM1::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	// TODO 0x7FC0 - 0x7FCF is modem IO area
	
	//PRT_DEBUG("FSA1FM1: write "<<hex<<(int)address<<" "<<(int)value<<dec);
	if ((0x6000 <= address) && (address < 0x8000)) {
		if (address == 0x7FC4) {
			// switch rom bank
			cpu->invalidateCache(0x4000, 0x2000 / CPU::CACHE_LINE_SIZE);
		}
		sram[address & 0x1FFF] = value;
	}
}

byte* RomFSA1FM1::getWriteCacheLine(word address) const
{
	if (address == (0x7FC0 & CPU::CACHE_LINE_HIGH)) {
		// dont't cache IO area
		return NULL;
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		return &sram[address & 0x1FFF];
	} else {
		return unmappedWrite;
	}
}


// Mapper for slot 3-3 //

RomFSA1FM2::RomFSA1FM2(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom)
	: MSXDevice(config, time), Rom8kBBlocks(config, time, rom)
{
	sram = FSA1FMRam::getSRAM(config);
	reset(time);
}

RomFSA1FM2::~RomFSA1FM2()
{
}

void RomFSA1FM2::reset(const EmuTime& /*time*/)
{
	control = 0;
	for (int region = 0; region < 6; ++region) {
		changeBank(region, 0xA8);
	}
	changeBank(6, 0);
	changeBank(7, 0);
}

byte RomFSA1FM2::readMem(word address, const EmuTime& time)
{
	byte result;
	if (0xC000 <= address) {
		result = 0xFF;
	} else if ((control & 0x04) && (0x7FF0 <= address) && (address < 0x7FF8)) {
		// read mapper state
		result = bankSelect[address & 7];
	} else if (isRam[address >> 13]) {
		result = sram[address & 0x1FFF];
	} else if (isEmpty[address >> 13]) {
		result = 0xFF;
	} else {
		result = Rom8kBBlocks::readMem(address, time);
	}
	//PRT_DEBUG("FSA1FM2 read "<<hex<<(int)address<<" "<<(int)result<<dec);
	return result;
}

const byte* RomFSA1FM2::getReadCacheLine(word address) const
{
	if (0xC000 <= address) {
		return unmappedRead;
	} else if ((0x7FF0 & CPU::CACHE_LINE_HIGH) == address) {
		return NULL;
	} else if (isRam[address >> 13]) {
		return &sram[address & 0x1FFF];
	} else if (isEmpty[address >> 13]) {
		return unmappedRead;
	} else {
		return Rom8kBBlocks::getReadCacheLine(address);
	}
}

void RomFSA1FM2::writeMem(word address, byte value,
                          const EmuTime& /*time*/)
{
	//PRT_DEBUG("FSA1FM2: write "<<hex<<(int)address<<" "<<(int)value<<dec);

	if ((0x6000 <= address) && (address < 0x7FF0)) {
		// set mapper state
		switch (address & 0x1C00) {
			case 0x0000:
				changeBank(2, value);
				break;
			case 0x0400:
				changeBank(0, value);
				break;
			case 0x0800:
				changeBank(3, value);
				break;
			case 0x0C00:
				changeBank(1, value);
				break;
			case 0x1000:
				changeBank(4, value);
				break;
			case 0x1400:
				// nothing
				break;
			case 0x1800:
				changeBank(5, value);
				break;
			case 0x1C00:
				// nothing
				break;
		}
	} else if (address == 0x7FF9) {
		// write control byte
		control = value;
	} else if (isRam[address >> 13]) {
		sram[address & 0x1FFF] = value;
	}
}

byte* RomFSA1FM2::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		return NULL;
	} else if (isRam[address >> 13]) {
		return &sram[address & 0x1FFF];
	} else {
		return unmappedWrite;
	}
}

void RomFSA1FM2::changeBank(byte region, byte bank)
{
	bankSelect[region] = bank;
	if ((0x80 <= bank) && (bank < 0x90)) {
		if (bank & 0x04) {
			isRam[region]   = true;
			isEmpty[region] = false;
		} else {
			isRam[region]   = false;
			isEmpty[region] = true;
		}
		cpu->invalidateCache(0x2000 * region, 0x2000 / CPU::CACHE_LINE_SIZE);
	} else {
		isRam[region]   = false;
		isEmpty[region] = false;
		setRom(region, bank & 0x7F);
	}
}

} // namespace openmsx
