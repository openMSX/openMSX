// $Id$

#ifndef DUMMYDEVICE_HH
#define DUMMYDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class DummyDevice : public MSXDevice
{
public:
	DummyDevice(MSXMotherBoard& motherBoard, const XMLElement& config,
	            const EmuTime& time);

	virtual void reset(const EmuTime& time);
};

} // namespace openmsx

#endif
