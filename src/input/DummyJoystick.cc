#include "DummyJoystick.hh"

namespace openmsx {

uint8_t DummyJoystick::read(EmuTime::param /*time*/)
{
	return 0x3F;
}

void DummyJoystick::write(uint8_t /*value*/, EmuTime::param /*time*/)
{
	// do nothing
}

std::string_view DummyJoystick::getDescription() const
{
	return {};
}

void DummyJoystick::plugHelper(Connector& /*connector*/,
                               EmuTime::param /*time*/)
{
}

void DummyJoystick::unplugHelper(EmuTime::param /*time*/)
{
}

} // namespace openmsx
