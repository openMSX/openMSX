#include "RomPageNN.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"

namespace openmsx {

RomPageNN::RomPageNN(const DeviceConfig& config, Rom&& rom_, RomType type)
	: Rom8kBBlocks(config, std::move(rom_))
{
	const auto pages = [&] {
		switch (type) {
			using enum RomType;
			case PAGE0:    return 0b0001;
			case PAGE1:    return 0b0010;
			case PAGE01:   return 0x0011;
			case PAGE2:    return 0b0100;
			case PAGE12:   return 0b0110;
			case PAGE012:  return 0b0111;
			case PAGE3:    return 0b1000;
			case PAGE23:   return 0b1100;
			case PAGE123:  return 0b1110;
			case PAGE0123: return 0b1111;
			default: UNREACHABLE;
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
