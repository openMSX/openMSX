// $Id$

#include "DummyJoystick.hh"

namespace openmsx {

byte DummyJoystick::read(EmuTime::param /*time*/)
{
	return 0x3F;
}

void DummyJoystick::write(byte /*value*/, EmuTime::param /*time*/)
{
	// do nothing
}

const std::string& DummyJoystick::getDescription() const
{
	static const std::string EMPTY;
	return EMPTY;
}

void DummyJoystick::plugHelper(Connector& /*connector*/,
                               EmuTime::param /*time*/)
{
}

void DummyJoystick::unplugHelper(EmuTime::param /*time*/)
{
}

} // namespace openmsx
