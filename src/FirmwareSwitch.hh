// $Id$

#ifndef __FRONTSWITCH_HH__
#define __FRONTSWITCH_HH__

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
