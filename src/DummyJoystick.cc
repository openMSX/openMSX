// $Id$

#include "DummyJoystick.hh"

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
{
}

void DummyJoystick::unplug(const EmuTime& time)
{
}
