#ifndef ROMGENERIC8KB_HH
#define ROMGENERIC8KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomGeneric8kB final : public Rom8kBBlocks
{
public:
	RomGeneric8kB(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;
};

} // namespace openmsx

#endif
