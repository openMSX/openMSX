// $Id$

#include "RomSynthesizer.hh"
#include "MSXConfig.hh"
#include "DACSound8U.hh"
#include "CPU.hh"


RomSynthesizer::RomSynthesizer(Device* config, const EmuTime &time, Rom *rom)
	: MSXDevice(config, time), Rom16kBBlocks(config, time, rom)
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setRom (2, 1);
	setBank(3, unmappedRead);

	short volume = (short)config->getParameterAsInt("volume");
	dac = new DACSound8U(volume, time);

	reset(time);
}

RomSynthesizer::~RomSynthesizer()
{
	delete dac;
}

void RomSynthesizer::reset(const EmuTime &time)
{
	dac->reset(time);
}

void RomSynthesizer::writeMem(word address, byte value, const EmuTime &time)
{
	if (address == 0x4000) {
		dac->writeDAC(value, time);
	}
}

byte* RomSynthesizer::getWriteCacheLine(word address) const
{
	if (address == (0x4000 & CPU::CACHE_LINE_SIZE)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}
