#ifndef ROMMSXWRITE_HH
#define ROMMSXWRITE_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomMSXWrite final : public Rom16kBBlocks
{
public:
	RomMSXWrite(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

REGISTER_BASE_CLASS(RomMSXWrite, "RomMSXWrite");

} // namespace openmsx

#endif
