// $Id$

#include "JoystickPorts.hh"
#include "JoystickDevice.hh"
#include "DummyJoystick.hh"
#include "ConsoleSource/Console.hh"
#include "PluggingController.hh"
#include <cassert>


JoystickPort::JoystickPort(const std::string &nm)
{
	name = nm;
	PluggingController::instance()->registerConnector(this);
	
	unplug();
}

JoystickPort::~JoystickPort()
{
	unplug();
	PluggingController::instance()->unregisterConnector(this);
}

std::string JoystickPort::getName()
{
	return name;
}

std::string JoystickPort::getClass()
{
	return className;
}
std::string JoystickPort::className("Joystick Port");

void JoystickPort::plug(Pluggable *device)
{
	Connector::plug(device);
}

void JoystickPort::unplug()
{
	Connector::unplug();
	plug(DummyJoystick::instance());
}

byte JoystickPort::read()
{
	return ((JoystickDevice*)pluggable)->read();
}

void JoystickPort::write(byte value)
{
	((JoystickDevice*)pluggable)->write(value);
}

///

JoystickPorts::JoystickPorts()
{
	selectedPort = 0;
	ports[0] = new JoystickPort("joyporta");
	ports[1] = new JoystickPort("joyportb");
}

JoystickPorts::~JoystickPorts()
{
	delete ports[0];
	delete ports[1];
}

byte JoystickPorts::read()
{
	return ports[selectedPort]->read();
}

void JoystickPorts::write(byte value)
{
	byte val0 =  (value&0x03)    |((value&0x10)>>2);
	byte val1 = ((value&0x0c)>>2)|((value&0x20)>>3);
	ports[0]->write(val0);
	ports[1]->write(val1);
	selectedPort = (value&0x40)>>6;
}

