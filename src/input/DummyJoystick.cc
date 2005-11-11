// $Id$

#include "DummyJoystick.hh"

namespace openmsx {

DummyJoystick::DummyJoystick()
{
}

DummyJoystick::~DummyJoystick()
{
}


byte DummyJoystick::read(const EmuTime& /*time*/)
{
	return 0x3F;
}

void DummyJoystick::write(byte /*value*/, const EmuTime& /*time*/)
{
	// do nothing
}

const std::string& DummyJoystick::getDescription() const
{
	static const std::string EMPTY;
	return EMPTY;
}

void DummyJoystick::plugHelper(Connector& /*connector*/,
                               const EmuTime& /*time*/)
{
}

void DummyJoystick::unplugHelper(const EmuTime& /*time*/)
{
}

} // namespace openmsx
