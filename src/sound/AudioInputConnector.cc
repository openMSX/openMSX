// $Id$

#include "AudioInputDevice.hh"
#include "AudioInputConnector.hh"
#include "DummyAudioInputDevice.hh"
#include "WavAudioInput.hh"
#include "PluggingController.hh"


namespace openmsx {

AudioInputConnector::AudioInputConnector()
{
	dummy = new DummyAudioInputDevice();
	wavInput = new WavAudioInput();
	pluggable = dummy;

	PluggingController::instance()->registerConnector(this);
}

AudioInputConnector::~AudioInputConnector()
{
	PluggingController::instance()->unregisterConnector(this);

	delete wavInput;
	delete dummy;
}


const string &AudioInputConnector::getClass() const
{
	static const string className("Audio Input Port");
	return className;
}

void AudioInputConnector::plug(Pluggable *dev, const EmuTime &time)
{
	Connector::plug(dev, time);
}

void AudioInputConnector::unplug(const EmuTime &time)
{
	Connector::unplug(time);
	plug(dummy, time);
}


short AudioInputConnector::readSample(const EmuTime &time)
{
	return static_cast<AudioInputDevice*>(pluggable)->readSample(time);
}

} // namespace openmsx
