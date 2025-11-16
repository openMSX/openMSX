#include "DummyJoystick.hh"

namespace openmsx {

uint8_t DummyJoystick::read(EmuTime /*time*/)
{
	return 0x3F;
}

void DummyJoystick::write(uint8_t /*value*/, EmuTime /*time*/)
{
	// do nothing
}

zstring_view DummyJoystick::getDescription() const
{
	return {};
}

void DummyJoystick::plugHelper(Connector& /*connector*/,
                               EmuTime /*time*/)
{
}

void DummyJoystick::unplugHelper(EmuTime /*time*/)
{
}

} // namespace openmsx
