// $Id$

#include "JoystickPorts.hh"
#include "JoystickPort.hh"
//#include "JoystickDevice.hh"
//#include "DummyJoystick.hh"
//#include "PluggingController.hh"
#include "Mouse.hh"
#include "Joystick.hh"
#include "JoyNet.hh"
#include "KeyJoystick.hh"


namespace openmsx {

JoystickPorts::JoystickPorts(const EmuTime &time)
{
	selectedPort = 0;
	ports[0] = new JoystickPort("joyporta", time);
	ports[1] = new JoystickPort("joyportb", time);

	mouse = new Mouse(time);
#ifndef	NO_SOCKET
	joynet = new JoyNet();
#endif
	keyJoystick = new KeyJoystick();
	int i = 0;
	try {
		for (; i < 10; i++) {
			joystick[i] = new Joystick(i);
		}
	} catch (JoystickException &e) {
		for (; i < 10; i++) {
			joystick[i] = NULL;
		}
	}
}

JoystickPorts::~JoystickPorts()
{
	delete ports[0];
	delete ports[1];

	delete keyJoystick;
	delete mouse;
#ifndef	NO_SOCKET
	delete joynet;
#endif
	for (int i = 0; i < 10; i++) {
		delete joystick[i];
	}
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
	byte val0 =  (value & 0x03)       | ((value & 0x10) >> 2);
	byte val1 = ((value & 0x0C) >> 2) | ((value & 0x20) >> 3);
	ports[0]->write(val0, time);
	ports[1]->write(val1, time);
	selectedPort = (value & 0x40) >> 6;
}

} // namespace openmsx

