// $Id$

#include "FrontSwitch.hh"

namespace openmsx {

FrontSwitch::FrontSwitch()
	: setting("frontswitch",
	          "This setting controls the front switch",
	          false)
{
}

bool FrontSwitch::getStatus() const
{
	return setting.getValue();
}

} // namespace openmsx
