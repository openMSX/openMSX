// $Id$

#include "DummyMidiInDevice.hh"

namespace openmsx {

void DummyMidiInDevice::signal(const EmuTime& /*time*/)
{
	// ignore
}

const std::string& DummyMidiInDevice::getDescription() const
{
	static const std::string EMPTY;
	return EMPTY;
}

void DummyMidiInDevice::plugHelper(Connector& /*connector*/,
                                   const EmuTime& /*time*/)
{
}

void DummyMidiInDevice::unplugHelper(const EmuTime& /*time*/)
{
}

} // namespace openmsx
