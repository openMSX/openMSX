// $Id$

#include "RomPageNN.hh"
#include "Rom.hh"

namespace openmsx {

RomPageNN::RomPageNN(const XMLElement& config, const EmuTime& time,
                     std::auto_ptr<Rom> rom, byte pages)
	: Rom8kBBlocks(config, time, rom)
{
	int bank = 0;
	for (int page = 0; page < 4; page++) {
		if (pages & (1 << page)) {
			setRom(page * 2, bank++);
			setRom(page * 2 + 1, bank++);
		} else {
			setBank(page * 2, unmappedRead);
			setBank(page * 2 + 1, unmappedRead);
		}
	}
}

RomPageNN::~RomPageNN()
{
}

} // namespace openmsx

