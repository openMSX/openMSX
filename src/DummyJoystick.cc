// $Id$

#include "DummyJoystick.hh"

DummyJoystick::DummyJoystick()
{
}

DummyJoystick::~DummyJoystick()
{
}

DummyJoystick* DummyJoystick::instance()
{
	if (oneInstance == 0 ) {
		oneInstance = new DummyJoystick();
	}
	return oneInstance;
}
DummyJoystick* DummyJoystick::oneInstance = 0;


byte DummyJoystick::read()
{
	return 0x3f;
}

void DummyJoystick::write(byte value)
{
	// do nothing
}

