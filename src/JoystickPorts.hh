// $Id$

#ifndef __JOYSTICKPORTS_HH__
#define __JOYSTICKPORTS_HH__

#include "openmsx.hh"
#include "Connector.hh"

// forward declaration
class DummyJoystick;
class Mouse;
class JoyNet;
class Joystick;


class JoystickPort : public Connector
{
	public:
		JoystickPort(const std::string &name, const EmuTime &time);
		virtual ~JoystickPort();

		virtual const std::string &getName() const;
		virtual const std::string &getClass() const;
		virtual void plug(Pluggable *device, const EmuTime &time);
		virtual void unplug(const EmuTime &time);

		byte read(const EmuTime &time);
		void write(byte value, const EmuTime &time);

	private:
		DummyJoystick* dummy;
		std::string name;

		byte lastValue;
};

class JoystickPorts
{
	public:
		JoystickPorts(const EmuTime &time);
		~JoystickPorts();

		void powerOff(const EmuTime &time);
		byte read(const EmuTime &time);
		void write(byte value, const EmuTime &time);

	private:
		int selectedPort;
		JoystickPort* ports[2];

		Mouse* mouse;
		JoyNet* joynet;
		Joystick* joystick[10];
};
#endif
