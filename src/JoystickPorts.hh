// $Id$

#ifndef __JOYSTICKPORTS_HH__
#define __JOYSTICKPORTS_HH__

#include "openmsx.hh"
#include "Connector.hh"


class JoystickPort : public Connector
{
	public:
		JoystickPort(const string &name, const EmuTime &time);
		virtual ~JoystickPort();

		virtual const string &getName() const;
		virtual const string &getClass() const;
		virtual void plug(Pluggable *device, const EmuTime &time);
		virtual void unplug(const EmuTime &time);

		byte read(const EmuTime &time);
		void write(byte value, const EmuTime &time);

	private:
		class DummyJoystick* dummy;
		string name;

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

		class Mouse* mouse;
		class JoyNet* joynet;
		class KeyJoystick* keyJoystick;
		class Joystick* joystick[10];
};

#endif
