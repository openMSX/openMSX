// $Id$

#include <cassert>
#include "JoyNet.hh"
#include "PluggingController.hh"
//#include "EventDistributor.hh"


JoyNet::JoyNet(int joyNum)
{
	PRT_DEBUG("Creating a JoyNet object for joystick " << joyNum);


	//throw JoyNetException("No such joystick number");

	name = std::string("joyNet")+(char)('1'+joyNum);

	PluggingController::instance()->registerPluggable(this);
}

JoyNet::~JoyNet()
{
	PluggingController::instance()->unregisterPluggable(this);
}

//Pluggable
const std::string &JoyNet::getName()
{
	return name;
}


//JoystickDevice
byte JoyNet::read(const EmuTime &time)
{
	return status;
}

void JoyNet::write(byte value, const EmuTime &time)
{
	//do nothing
}

