#ifndef ROMSUPERLODERUNNER_HH
#define ROMSUPERLODERUNNER_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomSuperLodeRunner final : public Rom16kBBlocks
{
public:
	RomSuperLodeRunner(const DeviceConfig& config, Rom&& rom);
	~RomSuperLodeRunner();

	void reset(EmuTime::param time) override;
	void globalWrite(word address, byte value, EmuTime::param time) override;
};

} // namespace openmsx

#endif
