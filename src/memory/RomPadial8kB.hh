// $Id$

#ifndef ROMPADIAL8KB_HH
#define ROMPADIAL8KB_HH

#include "RomAscii8kB.hh"

namespace openmsx {

class RomPadial8kB : public RomAscii8kB
{
public:
	RomPadial8kB(MSXMotherBoard& motherBoard, const XMLElement& config,
	             std::auto_ptr<Rom> rom);

	virtual void reset(EmuTime::param time);
};

} // namespace openmsx

#endif
