// $Id$

#ifndef __JOYSTICKPORTS_HH__
#define __JOYSTICKPORTS_HH__

#include "openmsx.hh"
#include "Connector.hh"

// forward declaration
class JoystickDevice;
class Mouse;
class Joystick;


class JoystickPort : public Connector
{
public:
	JoystickPort(const std::string &name);
	virtual ~JoystickPort();

	virtual std::string getName();
	virtual std::string getClass();
	virtual void plug(Pluggable *device);
	virtual void unplug();

	byte read();
	void write(byte value);

private:
	std::string name;
	static std::string className;
};

class JoystickPorts
{
	public:
		JoystickPorts();
		~JoystickPorts();

		byte read();
		void write(byte value);

	private:
		int selectedPort;
		JoystickPort* ports[2];
};
#endif
