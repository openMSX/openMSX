// $Id$

#include "AudioInputDevice.hh"
#include "AudioInputConnector.hh"
#include "DummyAudioInputDevice.hh"
#include "PluggingController.hh"


namespace openmsx {

AudioInputConnector::AudioInputConnector(const string &name)
	: Connector(name, new DummyAudioInputDevice())
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

const string &AudioInputConnector::getClass() const
{
	static const string className("Audio Input Port");
	return className;
}

short AudioInputConnector::readSample(const EmuTime &time)
{
	return static_cast<AudioInputDevice*>(pluggable)->readSample(time);
}

} // namespace openmsx
