
#ifndef __JOYSTICKINTERFACE_HH__
#define __JOYSTICKINTERFACE_HH__

#include "openmsx.hh"

class JoystickDevice
{
	public:
		virtual byte read() = 0;
		virtual void write(byte value) = 0;
};
#endif
