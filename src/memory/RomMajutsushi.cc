// $Id$

// Mapper for "Hai no Majutsushi" from Konami.
// It's a Konami4 mapper with a DAC.

#include "RomMajutsushi.hh"
#include "MSXMotherBoard.hh"
#include "DACSound8U.hh"
#include "Rom.hh"

namespace openmsx {

RomMajutsushi::RomMajutsushi(
	MSXMotherBoard& motherBoard, const XMLElement& config,
	const EmuTime& time, std::auto_ptr<Rom> rom)
	: RomKonami4(motherBoard, config, time, rom)
	, dac(new DACSound8U(motherBoard.getMixer(), getName(),
		"Majutsushi DAC", config, time))
{
}

RomMajutsushi::~RomMajutsushi()
{
}

void RomMajutsushi::reset(const EmuTime& time)
{
	RomKonami4::reset(time);
	dac->reset(time);
}

void RomMajutsushi::writeMem(word address, byte value, const EmuTime& time)
{
	if (0x5000 <= address && address < 0x6000) {
		dac->writeDAC(value, time);
	} else {
		RomKonami4::writeMem(address, value, time);
	}
}

byte* RomMajutsushi::getWriteCacheLine(word address) const
{
	return (0x5000 <= address && address < 0x6000)
		? NULL : RomKonami4::getWriteCacheLine(address);
}

} // namespace openmsx
