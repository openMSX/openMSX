// $Id$

#ifndef FRONTSWITCH_HH
#define FRONTSWITCH_HH

#include <memory>

namespace openmsx {

class BooleanSetting;

class FirmwareSwitch
{
public:
	FirmwareSwitch();
	bool getStatus() const;

private:
	const std::auto_ptr<BooleanSetting> setting;
};

} // namespace openmsx

#endif
