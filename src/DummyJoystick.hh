// $Id$

#ifndef __DUMMYJOYSTICK_HH__
#define __DUMMYJOYSTICK_HH__

#include "JoystickDevice.hh"


class DummyJoystick : public JoystickDevice
{
	public:
		virtual ~DummyJoystick();
		static DummyJoystick *instance();
		virtual byte read(const EmuTime &time);
		virtual void write(byte value, const EmuTime &time);

		virtual const std::string &getName();

	private:
		DummyJoystick();
		static DummyJoystick *oneInstance;
		static const std::string name;
};
#endif
