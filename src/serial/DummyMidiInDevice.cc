// $Id$

#include "DummyMidiInDevice.hh"

namespace openmsx {

void DummyMidiInDevice::signal(EmuTime::param /*time*/)
{
	// ignore
}

const std::string DummyMidiInDevice::getDescription() const
{
	const std::string EMPTY;
	return EMPTY;
}

void DummyMidiInDevice::plugHelper(Connector& /*connector*/,
                                   EmuTime::param /*time*/)
{
}

void DummyMidiInDevice::unplugHelper(EmuTime::param /*time*/)
{
}

} // namespace openmsx
