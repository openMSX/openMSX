// $Id$

#include "JoystickPorts.hh"
#include "JoystickDevice.hh"
#include "DummyJoystick.hh"
#include "ConsoleSource/Console.hh"
#include "PluggingController.hh"
#include <cassert>


JoystickPort::JoystickPort(const std::string &nm, const EmuTime &time)
{
	name = nm;
	PluggingController::instance()->registerConnector(this);
	
	unplug(time);
}

JoystickPort::~JoystickPort()
{
	//unplug(time);
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

void JoystickPort::plug(Pluggable *device, const EmuTime &time)
{
	Connector::plug(device, time);
	write(lastValue, time);
}

void JoystickPort::unplug(const EmuTime &time)
{
	Connector::unplug(time);
	plug(DummyJoystick::instance(), time);
}

byte JoystickPort::read(const EmuTime &time)
{
	return ((JoystickDevice*)pluggable)->read(time);
}

void JoystickPort::write(byte value, const EmuTime &time)
{
	lastValue = value;
	((JoystickDevice*)pluggable)->write(value, time);
}

///

JoystickPorts::JoystickPorts(const EmuTime &time)
{
	selectedPort = 0;
	ports[0] = new JoystickPort("joyporta", time);
	ports[1] = new JoystickPort("joyportb", time);
}

JoystickPorts::~JoystickPorts()
{
	delete ports[0];
	delete ports[1];
}

byte JoystickPorts::read(const EmuTime &time)
{
	return ports[selectedPort]->read(time);
}

void JoystickPorts::write(byte value, const EmuTime &time)
{
	byte val0 =  (value&0x03)    |((value&0x10)>>2);
	byte val1 = ((value&0x0c)>>2)|((value&0x20)>>3);
	ports[0]->write(val0, time);
	ports[1]->write(val1, time);
	selectedPort = (value&0x40)>>6;
}

