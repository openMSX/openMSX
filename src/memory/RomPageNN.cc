// $Id$

#include "RomPageNN.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

RomPageNN::RomPageNN(const DeviceConfig& config, std::unique_ptr<Rom> rom, byte pages)
	: Rom8kBBlocks(config, std::move(rom))
{
	int bank = 0;
	for (int page = 0; page < 4; ++page) {
		if (pages & (1 << page)) {
			setRom(page * 2 + 0, bank++);
			setRom(page * 2 + 1, bank++);
		} else {
			setUnmapped(page * 2 + 0);
			setUnmapped(page * 2 + 1);
		}
	}
}

REGISTER_MSXDEVICE(RomPageNN, "RomPageNN");

} // namespace openmsx

