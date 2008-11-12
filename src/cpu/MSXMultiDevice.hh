// $Id$

#ifndef MSXMULTIDEVICE_HH
#define MSXMULTIDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXMultiDevice : public MSXDevice
{
public:
	explicit MSXMultiDevice(MSXMotherBoard& motherboard);

	virtual void reset(EmuTime::param time);
	virtual void powerUp(EmuTime::param time);
	virtual void powerDown(EmuTime::param time);
};

} // namespace openmsx

#endif
