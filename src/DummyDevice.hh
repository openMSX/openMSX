// $Id$

#ifndef DUMMYDEVICE_HH
#define DUMMYDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class DummyDevice : public MSXDevice
{
public:
	static DummyDevice& instance();
	virtual void reset(const EmuTime& time);

private:
	DummyDevice(const XMLElement& config, const EmuTime& time);
	virtual ~DummyDevice();
};

} // namespace openmsx

#endif
