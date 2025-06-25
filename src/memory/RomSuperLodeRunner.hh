#ifndef ROMSUPERLODERUNNER_HH
#define ROMSUPERLODERUNNER_HH

#include "RomBlocks.hh"
#include "MSXCPUInterface.hh"

namespace openmsx {

class RomSuperLodeRunner final
	: public Rom16kBBlocks
	, public GlobalWriteClient<RomSuperLodeRunner, CT_Interval<0x0000>>
{
public:
	RomSuperLodeRunner(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void globalWrite(uint16_t address, byte value, EmuTime time) override;
};

} // namespace openmsx

#endif
