// $Id$

#include "AudioInputConnector.hh"
#include "DummyAudioInputDevice.hh"
#include "AudioInputDevice.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "memory.hh"

namespace openmsx {

AudioInputConnector::AudioInputConnector(PluggingController& pluggingController,
                                         string_ref name)
	: Connector(pluggingController, name,
	            make_unique<DummyAudioInputDevice>())
{
}

AudioInputConnector::~AudioInputConnector()
{
}

const std::string AudioInputConnector::getDescription() const
{
	return "Audio input connector";
}

string_ref AudioInputConnector::getClass() const
{
	return "Audio Input Port";
}

short AudioInputConnector::readSample(EmuTime::param time) const
{
	return getPluggedAudioDev().readSample(time);
}

AudioInputDevice& AudioInputConnector::getPluggedAudioDev() const
{
	return *checked_cast<AudioInputDevice*>(&getPlugged());
}

template<typename Archive>
void AudioInputConnector::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Connector>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(AudioInputConnector);

} // namespace openmsx
