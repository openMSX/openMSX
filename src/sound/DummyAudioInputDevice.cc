#include "DummyAudioInputDevice.hh"

namespace openmsx {

std::string_view DummyAudioInputDevice::getDescription() const
{
	return {};
}

void DummyAudioInputDevice::plugHelper(Connector& /*connector*/,
                                       EmuTime::param /*time*/)
{
}

void DummyAudioInputDevice::unplugHelper(EmuTime::param /*time*/)
{
}

int16_t DummyAudioInputDevice::readSample(EmuTime::param /*time*/)
{
	return 0; // silence
}

} // namespace openmsx
