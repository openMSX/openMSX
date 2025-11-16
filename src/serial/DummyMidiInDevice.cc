#include "DummyMidiInDevice.hh"

namespace openmsx {

void DummyMidiInDevice::signal(EmuTime /*time*/)
{
	// ignore
}

zstring_view DummyMidiInDevice::getDescription() const
{
	return {};
}

void DummyMidiInDevice::plugHelper(Connector& /*connector*/,
                                   EmuTime /*time*/)
{
}

void DummyMidiInDevice::unplugHelper(EmuTime /*time*/)
{
}

} // namespace openmsx
