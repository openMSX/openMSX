// $Id$

#include "JoystickPorts.hh"
#include "JoystickDevice.hh"
#include "DummyJoystick.hh"
#include "ConsoleSource/Console.hh"
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
	mouse = NULL;
	for (int i=0; i<NUM_JOYSTICKS; i++) {
		joystick[i] = NULL;
	}
}
JoystickPorts::JoyPortCmd::~JoyPortCmd()
{
	delete mouse;
	for (int i=0; i<NUM_JOYSTICKS; i++) {
		delete joystick[i];
	}
}
void JoystickPorts::JoyPortCmd::execute(char* commandLine)
{
	bool error = false;
	int port = (commandLine[7] == 'b') ? 1 : 0;	// is either 'a' or 'b'
	if (0 == strncmp(&commandLine[9], "unplug", 6)) {
		JoystickPorts::instance()->unplug(port);
	} else if (0 == strncmp(&commandLine[9], "mouse", 5)) {
		if (mouse==NULL)
			mouse = new Mouse();
		JoystickPorts::instance()->plug(port, mouse);
	} else if (0 == strncmp(&commandLine[9], "joystick", 8)) {
		int num = commandLine[17]-'1';
		if (num<0 || num>NUM_JOYSTICKS) {
			error = true;
		} else {
			try {
				if (joystick[num]==NULL)
					joystick[num] = new Joystick(num);
				JoystickPorts::instance()->plug(port, joystick[num]);
			} catch (JoystickException &e) {
				Console::instance()->print(e.desc);
			}
		}
	} else {
		error = true;
	}
	if (error)
		Console::instance()->print("syntax error");
}
void JoystickPorts::JoyPortCmd::help(char *commandLine)
{
	Console::instance()->print("joyport[a|b] unplug        unplugs device from port");
	Console::instance()->print("joyport[a|b] mouse         plugs mouse in port");
	Console::instance()->print("joyport[a|b] joystick[1|2] plugs joystick in port");
}
