// $Id$

#include <cassert>
#include "JoystickPorts.hh"
#include "JoystickDevice.hh"
#include "DummyJoystick.hh"
#include "PluggingController.hh"
#include "Mouse.hh"
#include "Joystick.hh"
#include "JoyNet.hh"
#include "KeyJoystick.hh"


JoystickPort::JoystickPort(const std::string &nm, const EmuTime &time)
{
	name = nm;
	dummy = new DummyJoystick();
	lastValue = 255;	// != 0
	PluggingController::instance()->registerConnector(this);
	
	unplug(time);
}

JoystickPort::~JoystickPort()
{
	PluggingController::instance()->unregisterConnector(this);
	delete dummy;
}

const std::string &JoystickPort::getName() const
{
	return name;
}

const std::string &JoystickPort::getClass() const
{
	static const std::string className("Joystick Port");
	return className;
}

void JoystickPort::plug(Pluggable *device, const EmuTime &time)
{
	Connector::plug(device, time);
	((JoystickDevice*)pluggable)->write(lastValue, time);
}

void JoystickPort::unplug(const EmuTime &time)
{
	Connector::unplug(time);
	plug(dummy, time);
}

byte JoystickPort::read(const EmuTime &time)
{
	return ((JoystickDevice*)pluggable)->read(time);
}

void JoystickPort::write(byte value, const EmuTime &time)
{
	if (lastValue != value) {
		lastValue = value;
		((JoystickDevice*)pluggable)->write(value, time);
	}
}

///

JoystickPorts::JoystickPorts(const EmuTime &time)
{
	selectedPort = 0;
	ports[0] = new JoystickPort("joyporta", time);
	ports[1] = new JoystickPort("joyportb", time);

	mouse = new Mouse(time);
	joynet = new JoyNet();
	keyJoystick = new KeyJoystick();
	int i=0;
	try {
		for (; i<10; i++)
			joystick[i] = new Joystick(i);
	} catch(JoystickException &e) {
		for (; i<10; i++)
			joystick[i] = NULL;
	}
}

JoystickPorts::~JoystickPorts()
{
	delete ports[0];
	delete ports[1];
	
	delete keyJoystick;
	delete mouse;
	delete joynet;
	for (int i=0; i<10; i++)
		delete joystick[i];
}

void JoystickPorts::powerOff(const EmuTime &time)
{
	ports[0]->unplug(time);
	ports[1]->unplug(time);
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

