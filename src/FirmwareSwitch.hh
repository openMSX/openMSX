#ifndef FRONTSWITCH_HH
#define FRONTSWITCH_HH

#include "DeviceConfig.hh"
#include "BooleanSetting.hh"

namespace openmsx {

class FirmwareSwitch
{
public:
	explicit FirmwareSwitch(const DeviceConfig& config);
	~FirmwareSwitch();

	[[nodiscard]] bool getStatus() const { return setting.getBoolean(); }

private:
	const DeviceConfig config;
	BooleanSetting setting;
};

} // namespace openmsx

#endif
