// $Id$

#include "RomSynthesizer.hh"
#include "MSXConfig.hh"
#include "DACSound8U.hh"


RomSynthesizer::RomSynthesizer(Device* config, const EmuTime &time)
	: MSXDevice(config, time), Rom16kBBlocks(config, time)
{
	setBank(0, unmapped);
	setRom (1, 0);
	setRom (2, 1);
	setBank(3, unmapped);

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
