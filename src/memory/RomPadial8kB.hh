// $Id$

#ifndef ROMPADIAL8KB_HH
#define ROMPADIAL8KB_HH

#include "RomAscii8kB.hh"

namespace openmsx {

class RomPadial8kB : public RomAscii8kB
{
public:
	RomPadial8kB(MSXMotherBoard& motherBoard, const XMLElement& config,
	             const EmuTime& time, std::auto_ptr<Rom> rom);

	virtual void reset(const EmuTime& time);
};

} // namespace openmsx

#endif
