// $Id$

#include "RomMSXAudio.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include "Rom.hh"

namespace openmsx {

RomMSXAudio::RomMSXAudio(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom)
	: MSXRom(config, time, rom)
	, ram(getName() + " RAM", "MSX-AUDIO mapped RAM", 0x1000)
{
	reset(time);
}

RomMSXAudio::~RomMSXAudio()
{
}

void RomMSXAudio::reset(const EmuTime& /*time*/)
{
	ram.clear();	// TODO check
	bankSelect = 0;
	cpu->invalidateCache(0x0000, 0x10000 / CPU::CACHE_LINE_SIZE);
}

byte RomMSXAudio::readMem(word address, const EmuTime& /*time*/)
{
	if ((bankSelect == 0) && ((address & 0x3FFF) >= 0x3000)) {
		return ram[(address & 0x3FFF) - 0x3000];
	} else {
		return (*rom)[0x8000 * bankSelect + (address & 0x7FFF)];
	}
}

const byte* RomMSXAudio::getReadCacheLine(word address) const
{
	if ((bankSelect == 0) && ((address & 0x3FFF) >= 0x3000)) {
		return &ram[(address & 0x3FFF) - 0x3000];
	} else {
		return &(*rom)[0x8000 * bankSelect + (address & 0x7FFF)];
	}
}

void RomMSXAudio::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	address &= 0x7FFF;
	if (address == 0x7FFE) {
		bankSelect = value & 3;
		cpu->invalidateCache(0x0000, 0x10000 / CPU::CACHE_LINE_SIZE);
	}
	address &= 0x3FFF;
	if ((bankSelect == 0) && (address >= 0x3000)) {
		ram[address - 0x3000] = value;
	}
}

byte* RomMSXAudio::getWriteCacheLine(word address) const
{
	address &= 0x7FFF;
	if (address == (0x7FFE & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	}
	address &= 0x3FFF;
	if ((bankSelect == 0) && (address >= 0x3000)) {
		return const_cast<byte*>(&ram[address - 0x3000]);
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx
