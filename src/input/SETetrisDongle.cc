// $Id$

#include "SETetrisDongle.hh"

namespace openmsx {

SETetrisDongle::SETetrisDongle()
{
	PRT_DEBUG("Creating a SETetrisDongle object for joystick " );
	name=string("tetris2-protection");
	desc=string("SETetrisDongledongle");
}

SETetrisDongle::~SETetrisDongle()
{
}

//Pluggable
const string& SETetrisDongle::getName() const
{
	return name;
}

const string& SETetrisDongle::getDescription() const
{
	return desc;
}

void SETetrisDongle::plugHelper(Connector* /*connector*/, const EmuTime& /*time*/)
{
}

void SETetrisDongle::unplugHelper(const EmuTime& /*time*/)
{
}


//JoystickDevice
byte SETetrisDongle::read(const EmuTime& /*time*/)
{
	return status;
}

void SETetrisDongle::write(byte value, const EmuTime& /*time*/)
{
	//Original device used 4 NOR ports
	//pin4 will be value of pin7
	status=(value&2)*JOY_RIGHT;
}


} // namespace openmsx
