#ifndef ROMCOLECOMEGACART_HH
#define ROMCOLECOMEGACART_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomColecoMegaCart final : public Rom16kBBlocks
{
public:
	RomColecoMegaCart(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
};

} // namespace openmsx

#endif
