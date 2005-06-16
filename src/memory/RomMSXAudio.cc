// $Id$

#include "RomMSXAudio.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include "Rom.hh"
#include "Ram.hh"

namespace openmsx {

RomMSXAudio::RomMSXAudio(MSXMotherBoard& motherBoard, const XMLElement& config,
                         const EmuTime& time, std::auto_ptr<Rom> rom)
	: MSXRom(motherBoard, config, time, rom)
	, ram(new Ram(getName() + " RAM", "MSX-AUDIO mapped RAM", 0x1000))
{
	reset(time);
}

RomMSXAudio::~RomMSXAudio()
{
}

void RomMSXAudio::reset(const EmuTime& /*time*/)
{
	ram->clear();	// TODO check
	bankSelect = 0;
	cpu.invalidateMemCache(0x0000, 0x10000);
}

byte RomMSXAudio::readMem(word address, const EmuTime& /*time*/)
{
	if ((bankSelect == 0) && ((address & 0x3FFF) >= 0x3000)) {
		return (*ram)[(address & 0x3FFF) - 0x3000];
	} else {
		return (*rom)[0x8000 * bankSelect + (address & 0x7FFF)];
	}
}

const byte* RomMSXAudio::getReadCacheLine(word address) const
{
	if ((bankSelect == 0) && ((address & 0x3FFF) >= 0x3000)) {
		return &(*ram)[(address & 0x3FFF) - 0x3000];
	} else {
		return &(*rom)[0x8000 * bankSelect + (address & 0x7FFF)];
	}
}

void RomMSXAudio::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	address &= 0x7FFF;
	if (address == 0x7FFE) {
		bankSelect = value & 3;
		cpu.invalidateMemCache(0x0000, 0x10000);
	}
	address &= 0x3FFF;
	if ((bankSelect == 0) && (address >= 0x3000)) {
		(*ram)[address - 0x3000] = value;
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
		return const_cast<byte*>(&(*ram)[address - 0x3000]);
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx
