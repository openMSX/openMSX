// $Id$

#include "JoystickPorts.hh"
#include "JoystickDevice.hh"
#include "DummyJoystick.hh"
#include "Console.hh"
#include "Mouse.hh"
#include "Joystick.hh"
#include <cassert>


JoystickPorts::JoystickPorts()
{
	selectedPort = 0;
	unplug(0);
	unplug(1);

	Console::instance()->registerCommand(joyPortCmd, "joyporta");
	Console::instance()->registerCommand(joyPortCmd, "joyportb");
}

JoystickPorts::~JoystickPorts()
{
}

JoystickPorts* JoystickPorts::instance()
{
	if (oneInstance == 0 ) {
		oneInstance = new JoystickPorts();
	}
	return oneInstance;
}
JoystickPorts* JoystickPorts::oneInstance = 0;
 

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

void JoystickPorts::plug(int port, JoystickDevice *device)
{
	assert ((port==0)||(port==1));
	ports[port] = device;
}

void JoystickPorts::unplug(int port)
{
	plug (port, DummyJoystick::instance());
}

JoystickPorts::JoyPortCmd::JoyPortCmd()
{
	mouse = new Mouse();
	joystick1 = new Joystick(1);
	joystick2 = new Joystick(2);
}
JoystickPorts::JoyPortCmd::~JoyPortCmd()
{
	delete mouse;
	delete joystick1;
	delete joystick2;
}
void JoystickPorts::JoyPortCmd::execute(char* commandLine)
{
	// TODO stricter syntax checking
	int port = (commandLine[7] == 'b') ? 1 : 0;
	switch (commandLine[9]) {
	case 'u':
		JoystickPorts::instance()->unplug(port);
		break;
	case 'm':
		JoystickPorts::instance()->plug(port, mouse);
		break;
	case 'j':
		if (commandLine[17] == '2') {
			JoystickPorts::instance()->plug(port, joystick1);
		} else {
			JoystickPorts::instance()->plug(port, joystick2);
		}
		break;
	default:
		Console::instance()->printOnConsole("syntax error");
	}
}
void JoystickPorts::JoyPortCmd::help(char *commandLine)
{
	Console::instance()->printOnConsole("joyport[a|b] unplug        unplugs device from port");
	Console::instance()->printOnConsole("joyport[a|b] mouse         plugs mouse in port");
	Console::instance()->printOnConsole("joyport[a|b] joystick[1|2] plugs joystick in port");
}
