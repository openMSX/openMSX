// $Id$

#include "SETetrisDongle.hh"

namespace openmsx {

SETetrisDongle::SETetrisDongle()
{
	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;
}

SETetrisDongle::~SETetrisDongle()
{
}

//Pluggable
const string& SETetrisDongle::getName() const
{
	static const string NAME = "tetris2-protection";
	return NAME;
}

const string& SETetrisDongle::getDescription() const
{
	static const string DESC = "SETetrisDongledongle";
	return DESC;
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
	status = (value & 2) ? JOY_RIGHT : 0;
}

} // namespace openmsx
