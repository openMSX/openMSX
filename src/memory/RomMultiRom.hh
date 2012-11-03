//

#ifndef ROMMULTIROM_HH
#define ROMMULTIROM_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomMultiRom : public Rom16kBBlocks
{
public:
	RomMultiRom(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	virtual ~RomMultiRom();

	virtual void reset(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte counter;
};

} // namespace openmsx

#endif
