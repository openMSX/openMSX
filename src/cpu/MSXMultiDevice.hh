#ifndef MSXMULTIDEVICE_HH
#define MSXMULTIDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXMultiDevice : public MSXDevice
{
public:
	explicit MSXMultiDevice(HardwareConfig& hwConf);

	void reset(EmuTime time) override;
	void powerUp(EmuTime time) override;
	void powerDown(EmuTime time) override;
};

} // namespace openmsx

#endif
