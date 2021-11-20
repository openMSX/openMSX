#ifndef ROMMSXWRITE_HH
#define ROMMSXWRITE_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomMSXWrite final : public Rom16kBBlocks
{
public:
	RomMSXWrite(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;
};

REGISTER_BASE_CLASS(RomMSXWrite, "RomMSXWrite");

} // namespace openmsx

#endif
