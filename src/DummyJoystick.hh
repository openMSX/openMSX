// $Id$

#ifndef __DUMMYJOYSTICK_HH__
#define __DUMMYJOYSTICK_HH__

#include "JoystickDevice.hh"


class DummyJoystick : public JoystickDevice
{
	public:
		DummyJoystick();
		virtual ~DummyJoystick();
		virtual byte read(const EmuTime &time);
		virtual void write(byte value, const EmuTime &time);
		virtual void plug(Connector* connector, const EmuTime& time);
		virtual void unplug(const EmuTime& time);
};

#endif
