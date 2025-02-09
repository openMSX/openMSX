#ifndef MSXMULTIDEVICE_HH
#define MSXMULTIDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXMultiDevice : public MSXDevice
{
public:
	explicit MSXMultiDevice(HardwareConfig& hwConf);

	void reset(EmuTime::param time) override;
	void powerUp(EmuTime::param time) override;
	void powerDown(EmuTime::param time) override;
};

} // namespace openmsx

#endif
