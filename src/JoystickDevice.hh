// $Id$

#ifndef __JOYSTICKDEVICE_HH__
#define __JOYSTICKDEVICE_HH__

#include "openmsx.hh"

class JoystickDevice
{
	public:
		virtual byte read() = 0;
		virtual void write(byte value) = 0;
};
#endif
