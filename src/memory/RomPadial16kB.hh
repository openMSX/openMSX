#ifndef ROMPADIAL16KB_HH
#define ROMPADIAL16KB_HH

#include "RomAscii16kB.hh"

namespace openmsx {

class RomPadial16kB final : public RomAscii16kB
{
public:
	RomPadial16kB(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
};

} // namespace openmsx

#endif
