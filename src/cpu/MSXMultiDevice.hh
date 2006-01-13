// $Id$

#ifndef MSXMULTIDEVICE_HH
#define MSXMULTIDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXMultiDevice : public MSXDevice
{
public:
	explicit MSXMultiDevice(MSXMotherBoard& motherboard);

	virtual void reset(const EmuTime& time);
	virtual void powerUp(const EmuTime& time);
	virtual void powerDown(const EmuTime& time);
};

} // namespace openmsx

#endif
