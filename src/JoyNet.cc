// $Id$

#include <cassert>
#include "JoyNet.hh"
#include "PluggingController.hh"
#include "MSXConfig.hh"
//#include "EventDistributor.hh"


JoyNet::JoyNet(int joyNum)
{
	PRT_DEBUG("Creating a JoyNet object for joystick " << joyNum);


	//throw JoyNetException("No such joystick number");

	name = std::string("joynet")+(char)('1'+joyNum);

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

void JoyNet::setupConnection()
{
	try {
	MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById(name);
	std::string hostname = config->getParameter("connecthost");
	std::string portname = config->getParameter("connectport");
	std::string listenport = config->getParameter("listenport");
	// setup  tcp stream with second (master) msx and listerenr for third (slave) msx
	} catch (MSXException& e) {
	PRT_DEBUG("No correct connection configuration for "<<name);
	}
}
void JoyNet::destroyConnection()
{
	// destroy connection (and thread?)
}


void JoyNet::sendByte(byte value)
{
	// No transformation of bits to be directly read into openMSX later on
	// needed since it is a one-on-one mapping

	/* Joynet cable looped for Maartens test program*/
	status=value;
}
