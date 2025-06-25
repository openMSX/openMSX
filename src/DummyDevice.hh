#ifndef DUMMYDEVICE_HH
#define DUMMYDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class DummyDevice final : public MSXDevice
{
public:
	explicit DummyDevice(const DeviceConfig& config);
	void reset(EmuTime time) override;
	void getNameList(TclObject& result) const override;
};

} // namespace openmsx

#endif
