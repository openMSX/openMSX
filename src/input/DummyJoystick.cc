// $Id$

#include "DummyJoystick.hh"

namespace openmsx {

DummyJoystick::DummyJoystick()
{
}

DummyJoystick::~DummyJoystick()
{
}


byte DummyJoystick::read(const EmuTime &time)
{
	return 0x3f;
}

void DummyJoystick::write(byte value, const EmuTime &time)
{
	// do nothing
}

void DummyJoystick::plug(Connector* connector, const EmuTime& time)
	throw()
{
}

void DummyJoystick::unplug(const EmuTime& time)
{
}

} // namespace openmsx
