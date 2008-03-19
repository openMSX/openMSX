// $Id$

#include "AudioInputConnector.hh"
#include "DummyAudioInputDevice.hh"
#include "PluggingController.hh"
#include "AudioInputDevice.hh"
#include "checked_cast.hh"

namespace openmsx {

AudioInputConnector::AudioInputConnector(PluggingController& pluggingController_,
                                         const std::string& name)
	: Connector(name, std::auto_ptr<Pluggable>(new DummyAudioInputDevice()))
	, pluggingController(pluggingController_)
{
	pluggingController.registerConnector(*this);
}

AudioInputConnector::~AudioInputConnector()
{
	pluggingController.unregisterConnector(*this);
}

const std::string& AudioInputConnector::getDescription() const
{
	static const std::string desc("Audio input connector.");
	return desc;
}

const std::string& AudioInputConnector::getClass() const
{
	static const std::string className("Audio Input Port");
	return className;
}

short AudioInputConnector::readSample(const EmuTime& time) const
{
	return getPluggedAudioDev().readSample(time);
}

AudioInputDevice& AudioInputConnector::getPluggedAudioDev() const
{
	return *checked_cast<AudioInputDevice*>(&getPlugged());
}

} // namespace openmsx
