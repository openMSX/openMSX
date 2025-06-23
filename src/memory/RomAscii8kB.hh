#ifndef ROMASCII8KB_HH
#define ROMASCII8KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomAscii8kB : public Rom8kBBlocks
{
public:
	RomAscii8kB(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
