#include "RomPageNN.hh"
#include "serialize.hh"

namespace openmsx {

RomPageNN::RomPageNN(const DeviceConfig& config, Rom&& rom_, byte pages)
	: Rom8kBBlocks(config, std::move(rom_))
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
