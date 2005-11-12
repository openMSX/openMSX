// $Id$

#include "RomPageNN.hh"
#include "Rom.hh"

namespace openmsx {

RomPageNN::RomPageNN(MSXMotherBoard& motherBoard, const XMLElement& config,
                     const EmuTime& time, std::auto_ptr<Rom> rom, byte pages)
	: Rom8kBBlocks(motherBoard, config, time, rom)
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

} // namespace openmsx

