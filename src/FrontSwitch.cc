// $Id$

#include "FrontSwitch.hh"


namespace openmsx {

FrontSwitch::FrontSwitch()
	: BooleanSetting("frontswitch",
	                 "This setting controls the front switch",
	                 false)
{
}

} // namespace openmsx
