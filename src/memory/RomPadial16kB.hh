// $Id$

#ifndef ROMPADIAL16KB_HH
#define ROMPADIAL16KB_HH

#include "RomAscii16kB.hh"

namespace openmsx {

class RomPadial16kB : public RomAscii16kB
{
public:
	RomPadial16kB(MSXMotherBoard& motherBoard, const XMLElement& config,
	              const EmuTime& time, std::auto_ptr<Rom> rom);

	virtual void reset(const EmuTime& time);
};

} // namespace openmsx

#endif
