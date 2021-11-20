#include "RomPageNN.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"

namespace openmsx {

RomPageNN::RomPageNN(const DeviceConfig& config, Rom&& rom_, RomType type)
	: Rom8kBBlocks(config, std::move(rom_))
{
	const byte pages = [&] {
		switch (type) {
			case ROM_PAGE0:    return 0b0001;
			case ROM_PAGE1:    return 0b0010;
			case ROM_PAGE01:   return 0x0011;
			case ROM_PAGE2:    return 0b0100;
			case ROM_PAGE12:   return 0b0110;
			case ROM_PAGE012:  return 0b0111;
			case ROM_PAGE3:    return 0b1000;
			case ROM_PAGE23:   return 0b1100;
			case ROM_PAGE123:  return 0b1110;
			case ROM_PAGE0123: return 0b1111;
			default: UNREACHABLE; return 0;
		}
	}();
	int bank = 0;
	for (auto page : xrange(4)) {
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
