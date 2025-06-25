#ifndef ROMCOLECOMEGACART_HH
#define ROMCOLECOMEGACART_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomColecoMegaCart final : public Rom16kBBlocks
{
public:
	RomColecoMegaCart(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
};

} // namespace openmsx

#endif
