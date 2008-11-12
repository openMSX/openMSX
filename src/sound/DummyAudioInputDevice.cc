// $Id$

#include "DummyAudioInputDevice.hh"

namespace openmsx {

const std::string& DummyAudioInputDevice::getDescription() const
{
	static const std::string EMPTY;
	return EMPTY;
}

void DummyAudioInputDevice::plugHelper(Connector& /*connector*/,
                                       EmuTime::param /*time*/)
{
}

void DummyAudioInputDevice::unplugHelper(EmuTime::param /*time*/)
{
}

short DummyAudioInputDevice::readSample(EmuTime::param /*time*/)
{
	return 0; // silence
}

} // namespace openmsx
