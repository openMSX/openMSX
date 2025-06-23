#ifndef ROMMSXDOS2_HH
#define ROMMSXDOS2_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomMSXDOS2 final : public Rom16kBBlocks
{
public:
	RomMSXDOS2(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

private:
	const byte range;
};

} // namespace openmsx

#endif
