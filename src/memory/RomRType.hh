#ifndef ROMRTYPE_HH
#define ROMRTYPE_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomRType final : public Rom16kBBlocks
{
public:
	RomRType(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;
};

} // namespace openmsx

#endif
