// $Id$

#ifndef __FRONTSWITCH_HH__
#define __FRONTSWITCH_HH__

#include "BooleanSetting.hh"


namespace openmsx {

class FirmwareSwitch
{
public:
	FirmwareSwitch();

	bool getStatus() const;

private:
	BooleanSetting setting;
};

} // namespace openmsx

#endif
