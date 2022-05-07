//

#ifndef ROMMULTIROM_HH
#define ROMMULTIROM_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomMultiRom final : public Rom16kBBlocks
{
public:
	RomMultiRom(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;

	void serialize(Archive auto& ar, unsigned version);

private:
	byte counter;
};

} // namespace openmsx

#endif
