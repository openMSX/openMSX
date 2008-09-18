// $Id:$

#ifndef ROMSUPERLODERUNNER_HH
#define ROMSUPERLODERUNNER_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomSuperLodeRunner : public Rom16kBBlocks
{
public:
	RomSuperLodeRunner(MSXMotherBoard& motherBoard, const XMLElement& config,
	                   std::auto_ptr<Rom> rom);
	virtual ~RomSuperLodeRunner();

	virtual void reset(const EmuTime& time);
	virtual void globalWrite(word address, byte value, const EmuTime& time);
};

} // namespace openmsx

#endif
