
#include "JoystickPorts.hh"
#include "DummyJoystick.hh"
#include <assert.h>


JoystickPorts::JoystickPorts()
{
	selectedPort = 0;
	unplug(0);
	unplug(1);
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

