#include "DummyAudioInputDevice.hh"

namespace openmsx {

std::string_view DummyAudioInputDevice::getDescription() const
{
	return {};
}

void DummyAudioInputDevice::plugHelper(Connector& /*connector*/,
                                       EmuTime /*time*/)
{
}

void DummyAudioInputDevice::unplugHelper(EmuTime /*time*/)
{
}

int16_t DummyAudioInputDevice::readSample(EmuTime /*time*/)
{
	return 0; // silence
}

} // namespace openmsx
