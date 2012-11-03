// $Id$

#ifndef ROMPADIAL16KB_HH
#define ROMPADIAL16KB_HH

#include "RomAscii16kB.hh"

namespace openmsx {

class RomPadial16kB : public RomAscii16kB
{
public:
	RomPadial16kB(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	virtual void reset(EmuTime::param time);
};

} // namespace openmsx

#endif
