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

const std::string &DummyJoystick::getName()
{
	static const std::string name("dummy");
	return name;
}
