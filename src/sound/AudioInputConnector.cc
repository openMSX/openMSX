// $Id$

#include "AudioInputDevice.hh"
#include "AudioInputConnector.hh"
#include "DummyAudioInputDevice.hh"
#include "WavAudioInput.hh"
#include "PluggingController.hh"


namespace openmsx {

AudioInputConnector::AudioInputConnector(const string &name)
	: Connector(name, new DummyAudioInputDevice())
{
	wavInput = new WavAudioInput();
	PluggingController::instance()->registerConnector(this);
}

AudioInputConnector::~AudioInputConnector()
{
	PluggingController::instance()->unregisterConnector(this);
	delete wavInput;
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
