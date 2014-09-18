#ifndef ROMPADIAL8KB_HH
#define ROMPADIAL8KB_HH

#include "RomAscii8kB.hh"

namespace openmsx {

class RomPadial8kB final : public RomAscii8kB
{
public:
	RomPadial8kB(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	virtual void reset(EmuTime::param time);
};

} // namespace openmsx

#endif
