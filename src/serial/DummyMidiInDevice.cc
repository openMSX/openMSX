// $Id$

#include "DummyMidiInDevice.hh"

using std::string;

namespace openmsx {

void DummyMidiInDevice::signal(const EmuTime& /*time*/)
{
	// ignore
}

const string& DummyMidiInDevice::getDescription() const
{
	static const string EMPTY;
	return EMPTY;
}

void DummyMidiInDevice::plugHelper(Connector* /*connector*/,
                                   const EmuTime& /*time*/)
{
}

void DummyMidiInDevice::unplugHelper(const EmuTime& /*time*/)
{
}

} // namespace openmsx
