#include "AudioInputConnector.hh"

#include "AudioInputDevice.hh"
#include "DummyAudioInputDevice.hh"

#include "serialize.hh"

#include "checked_cast.hh"

#include <memory>

namespace openmsx {

AudioInputConnector::AudioInputConnector(PluggingController& pluggingController_,
                                         std::string name_)
	: Connector(pluggingController_, std::move(name_),
	            std::make_unique<DummyAudioInputDevice>())
{
}

zstring_view AudioInputConnector::getDescription() const
{
	return "Audio input connector";
}

zstring_view AudioInputConnector::getClass() const
{
	return "Audio Input Port";
}

int16_t AudioInputConnector::readSample(EmuTime time) const
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
