// $Id$

#ifndef DUMMYJOYSTICK_HH
#define DUMMYJOYSTICK_HH

#include "JoystickDevice.hh"

namespace openmsx {

class DummyJoystick : public JoystickDevice
{
public:
	DummyJoystick();
	virtual ~DummyJoystick();
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
};

} // namespace openmsx

#endif
