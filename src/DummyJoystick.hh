
#ifndef __DUMMYJOYSTICK_HH__
#define __DUMMYJOYSTICK_HH__

#include "JoystickDevice.hh"

class DummyJoystick : public JoystickDevice
{
	public:
		virtual ~DummyJoystick();
		static DummyJoystick *instance();
		byte read();
		void write(byte value);

	private:
		DummyJoystick();
		static DummyJoystick *oneInstance;
};
#endif
