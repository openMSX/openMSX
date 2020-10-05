#include "DummyMidiInDevice.hh"

namespace openmsx {

void DummyMidiInDevice::signal(EmuTime::param /*time*/)
{
	// ignore
}

std::string_view DummyMidiInDevice::getDescription() const
{
	return {};
}

void DummyMidiInDevice::plugHelper(Connector& /*connector*/,
                                   EmuTime::param /*time*/)
{
}

void DummyMidiInDevice::unplugHelper(EmuTime::param /*time*/)
{
}

} // namespace openmsx
