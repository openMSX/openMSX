// $Id$

#include "AudioInputConnector.hh"
#include "DummyAudioInputDevice.hh"
#include "PluggingController.hh"

namespace openmsx {

AudioInputConnector::AudioInputConnector(const string& name)
	: Connector(name, auto_ptr<Pluggable>(new DummyAudioInputDevice()))
{
	PluggingController::instance().registerConnector(this);
}

AudioInputConnector::~AudioInputConnector()
{
	PluggingController::instance().unregisterConnector(this);
}

const string& AudioInputConnector::getDescription() const
{
	static const string desc("Auddio input connector.");
	return desc;
}

const string& AudioInputConnector::getClass() const
{
	static const string className("Audio Input Port");
	return className;
}

AudioInputDevice& AudioInputConnector::getPlugged() const
{
	return static_cast<AudioInputDevice&>(*plugged);
}

short AudioInputConnector::readSample(const EmuTime& time) const
{
	return getPlugged().readSample(time);
}

} // namespace openmsx
