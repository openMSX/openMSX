// $Id$

#include "SoundDevice.hh"
#include "MSXMixer.hh"
#include "XMLElement.hh"
#include "WavWriter.hh"
#include <cstring>
#include <cassert>

using std::string;

namespace openmsx {

static const unsigned MAX_SAMPLES = 16384;
static int mixBuffer[SoundDevice::MAX_CHANNELS * MAX_SAMPLES * 2];
static int silence[MAX_SAMPLES * 2];

SoundDevice::SoundDevice(MSXMixer& mixer_, const string& name_,
                         const string& description_,
                         unsigned numChannels_, bool stereo_)
	: mixer(mixer_), name(name_), description(description_)
	, numChannels(numChannels_), stereo(stereo_ ? 2 : 1)
	, numRecordChannels(0)
{
	assert(numChannels <= MAX_CHANNELS);
	assert(stereo == 1 || stereo == 2);
	memset(silence, 0, sizeof(silence));

	// initially no channels are muted
	for (unsigned i = 0; i < numChannels; ++i) {
		channelMuted[i] = false;
	}
}

SoundDevice::~SoundDevice()
{
}

const std::string& SoundDevice::getName() const
{
	return name;
}

const std::string& SoundDevice::getDescription() const
{
	return description;
}

bool SoundDevice::isStereo() const
{
	return stereo == 2;
}

void SoundDevice::registerSound(const XMLElement& config)
{
	const XMLElement& soundConfig = config.getChild("sound");
	short volume = soundConfig.getChildDataAsInt("volume");
	mixer.registerSound(*this, volume, numChannels);
}

void SoundDevice::unregisterSound()
{
	mixer.unregisterSound(*this);
}

void SoundDevice::updateStream(const EmuTime& time)
{
	mixer.updateStream(time);
}

void SoundDevice::setInputRate(unsigned sampleRate)
{
	inputSampleRate = sampleRate;
}

void SoundDevice::recordChannel(unsigned channel, const string& filename)
{
	assert(channel < numChannels);
	bool wasRecording = writer[channel].get();
	if (!filename.empty()) {
		writer[channel].reset(new WavWriter(
			filename, stereo, 16, inputSampleRate));
	} else {
		writer[channel].reset();
	}
	bool recording = writer[channel].get();
	if (recording != wasRecording) {
		if (recording) {
			if (numRecordChannels == 0) {
				mixer.setSynchronousMode(true);
			}
			++numRecordChannels;
			assert(numRecordChannels <= numChannels);
		} else {
			assert(numRecordChannels > 0);
			--numRecordChannels;
			if (numRecordChannels == 0) {
				mixer.setSynchronousMode(false);
			}
		}
	}
}

void SoundDevice::muteChannel(unsigned channel, bool muted)
{
	assert(channel < numChannels);
	channelMuted[channel] = muted;
}

bool SoundDevice::mixChannels(int* dataOut, unsigned samples)
{
	assert(samples <= MAX_SAMPLES);
	int* bufs[numChannels];
	for (unsigned i = 0; i < numChannels; ++i) {
		bufs[i] = &mixBuffer[samples * stereo * i];
	}
	generateChannels(bufs, samples);
	
	// record channels
	for (unsigned i = 0; i < numChannels; ++i) {
		if (writer[i].get()) {
			if (stereo == 1) {
				writer[i]->write16mono(
					bufs[i] ? bufs[i] : silence,
					samples);
			} else {
				writer[i]->write16stereo(
					bufs[i] ? bufs[i] : silence,
					samples);
			}
		}
	}
	
	// remove muted channels (explictly by user or by device itself)
	unsigned unmuted = 0;
	for (unsigned i = 0; i < numChannels; ++i) {
		if ((bufs[i] != 0) && !channelMuted[i]) {
			bufs[unmuted] = bufs[i];
			++unmuted;
		}
	}
	if (!unmuted) {
		// all channels muted
		return false;
	}

	// actually mix channels
	for (unsigned i = 0; i < samples * stereo; ++i) {
		int out = 0;
		for (unsigned j = 0; j < unmuted; ++j) {
			out += bufs[j][i];
		}
		dataOut[i] = out;
	}
	return true;
}

bool SoundDevice::updateBuffer(
	unsigned length, int* buffer,
	const EmuTime& /*start*/, const EmuDuration& /*sampDur*/)
{
	return mixChannels(buffer, length);
}

} // namespace openmsx
