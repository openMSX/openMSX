// $Id$

#ifndef ROMSUPERLODERUNNER_HH
#define ROMSUPERLODERUNNER_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomSuperLodeRunner : public Rom16kBBlocks
{
public:
	RomSuperLodeRunner(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	virtual ~RomSuperLodeRunner();

	virtual void reset(EmuTime::param time);
	virtual void globalWrite(word address, byte value, EmuTime::param time);
};

} // namespace openmsx

#endif
