// $Id$

// HYDLIDE2 cartridges
// 
// this type is is almost completely a ASCII16 cartrdige
// However, it has 2kB of SRAM (and 128 kB ROM)
// Use value 0x10 to select the SRAM.
// SRAM in page 1 => read-only
// SRAM in page 2 => read-write
// The 2Kb SRAM (0x800 bytes) are mirrored in the 16 kB block
//
// The address to change banks (from ASCII16):
//  first  16kb: 0x6000 - 0x67FF (0x6000 used)
//  second 16kb: 0x7000 - 0x77FF (0x7000 and 0x77FF used)

#include "RomHydlide2.hh"
#include "MSXCPU.hh"
#include "Rom.hh"
#include "SRAM.hh"

namespace openmsx {

RomHydlide2::RomHydlide2(const XMLElement& config, const EmuTime& time,
                         std::auto_ptr<Rom> rom)
	: RomAscii16kB(config, time, rom)
	, sram(new SRAM(getName() + " SRAM", 0x0800, config))
{
	reset(time);
}

RomHydlide2::~RomHydlide2()
{
}

void RomHydlide2::reset(const EmuTime& time)
{
	sramEnabled = 0;
	RomAscii16kB::reset(time);
}

byte RomHydlide2::readMem(word address, const EmuTime& time)
{
	if ((1 << (address >> 14)) & sramEnabled) {
		return (*sram)[address & 0x07FF];
	} else {
		return RomAscii16kB::readMem(address, time);
	}
}

const byte* RomHydlide2::getReadCacheLine(word address) const
{
	if ((1 << (address >> 14)) & sramEnabled) {
		return &(*sram)[address & 0x07FF];
	} else {
		return RomAscii16kB::getReadCacheLine(address);
	}
}

void RomHydlide2::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	if ((0x6000 <= address) && (address < 0x7800) && !(address & 0x0800)) {
		// bank switch
		byte region = ((address >> 12) & 1) + 1;
		if (value == 0x10) {
			// SRAM block
			sramEnabled |= (1 << region);
			cpu->invalidateMemCache(0x4000 * region, 0x4000);
		} else {
			// ROM block
			setRom(region, value);
			sramEnabled &= ~(1 << region);
		}
	} else {
		// write sram
		if ((1 << (address >> 14)) & sramEnabled & 0x04) {
			(*sram)[address & 0x07FF] = value;
		}
	}
}

byte* RomHydlide2::getWriteCacheLine(word address) const
{
	if ((1 << (address >> 14)) & sramEnabled & 0x04) {
		return const_cast<byte*>(&(*sram)[address & 0x07FF]);
	} else {
		return RomAscii16kB::getWriteCacheLine(address);
	}
}

} // namespace openmsx
