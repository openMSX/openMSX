// $Id$

#ifndef __DUMMYDEVICE_HH__
#define __DUMMYDEVICE_HH__

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

#endif //__DUMMYDEVICE_HH__

