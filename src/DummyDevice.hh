#ifndef DUMMYDEVICE_HH
#define DUMMYDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class DummyDevice final : public MSXDevice
{
public:
	explicit DummyDevice(const DeviceConfig& config);
	virtual void reset(EmuTime::param time);
	virtual void getNameList(TclObject& result) const;
};

} // namespace openmsx

#endif
