// $Id$

#include "RomPageNN.hh"
#include "Rom.hh"

namespace openmsx {

RomPageNN::RomPageNN(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom, byte pages)
	: Rom16kBBlocks(config, time, rom)
{
	int bank = 0;
	for (int page = 0; page < 4; page++) {
		if (pages & (1 << page)) {
			setRom(page, bank++);
		} else {
			setBank(page, unmappedRead);
		}
	}
}

RomPageNN::~RomPageNN()
{
}

} // namespace openmsx

