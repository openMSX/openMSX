// $Id$

#ifndef __JOYSTICKPORTS_HH__
#define __JOYSTICKPORTS_HH__

#include "openmsx.hh"
#include "EmuTime.hh"


namespace openmsx {

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
		class JoystickPort *ports[2];

		class Mouse *mouse;
		class JoyNet *joynet;
		class KeyJoystick *keyJoystick;
		class Joystick *joystick[10];
};

} // namespace openmsx

#endif
