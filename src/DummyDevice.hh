// $Id$

#ifndef DUMMYDEVICE_HH
#define DUMMYDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class DummyDevice : public MSXDevice
{
public:
	DummyDevice(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual void reset(EmuTime::param time);
};

} // namespace openmsx

#endif
