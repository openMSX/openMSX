// $Id$

#ifndef __DUMMYJOYSTICK_HH__
#define __DUMMYJOYSTICK_HH__

#include "JoystickDevice.hh"


class DummyJoystick : public JoystickDevice
{
	public:
		virtual ~DummyJoystick();
		static DummyJoystick *instance();
		virtual byte read();
		virtual void write(byte value);

		virtual const std::string &getName();
		virtual const std::string &getClass();

	private:
		DummyJoystick();
		static DummyJoystick *oneInstance;
		static const std::string name;
		static const std::string className;
};
#endif
