// $Id$

#ifndef __FRONTSWITCH_HH__
#define __FRONTSWITCH_HH__

#include "BooleanSetting.hh"


namespace openmsx {

class FrontSwitch
{
public:
	FrontSwitch();

	bool getStatus() const;

private:
	BooleanSetting setting;
};

} // namespace openmsx

#endif
