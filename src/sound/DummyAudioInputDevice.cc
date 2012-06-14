// $Id$

#include "DummyAudioInputDevice.hh"

namespace openmsx {

string_ref DummyAudioInputDevice::getDescription() const
{
	return "";
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
