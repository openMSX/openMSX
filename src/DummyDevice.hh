// $Id$

#ifndef __DUMMYDEVICE_HH__
#define __DUMMYDEVICE_HH__

#include "MSXIODevice.hh"
#include "MSXMemDevice.hh"

namespace openmsx {

class DummyDevice : public MSXIODevice, public MSXMemDevice
{
public:
	static DummyDevice& instance();
	virtual void reset(const EmuTime& time);

private:
	DummyDevice(Config* config, const EmuTime& time);
	virtual ~DummyDevice();
};

} // namespace openmsx

#endif //__DUMMYDEVICE_HH__

