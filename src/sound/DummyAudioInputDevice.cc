// $Id$

#include "DummyAudioInputDevice.hh"

namespace openmsx {

const std::string& DummyAudioInputDevice::getDescription() const
{
	static const std::string EMPTY;
	return EMPTY;
}

void DummyAudioInputDevice::plugHelper(Connector& /*connector*/,
                                       const EmuTime& /*time*/)
{
}

void DummyAudioInputDevice::unplugHelper(const EmuTime& /*time*/)
{
}

short DummyAudioInputDevice::readSample(const EmuTime& /*time*/)
{
	return 0; // silence
}

} // namespace openmsx
