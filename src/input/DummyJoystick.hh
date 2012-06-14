// $Id$

#ifndef DUMMYJOYSTICK_HH
#define DUMMYJOYSTICK_HH

#include "JoystickDevice.hh"

namespace openmsx {

class DummyJoystick : public JoystickDevice
{
public:
	virtual byte read(EmuTime::param time);
	virtual void write(byte value, EmuTime::param time);
	virtual string_ref getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
};

} // namespace openmsx

#endif
