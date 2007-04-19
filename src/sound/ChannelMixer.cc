// $Id: $

#include "ChannelMixer.hh"
#include "WavWriter.hh"
#include <cstring>
#include <cassert>

namespace openmsx {

static const unsigned MAX_SAMPLES = 16384;
static int mixBuffer[ChannelMixer::MAX_CHANNELS * MAX_SAMPLES * 2];
static int silence[MAX_SAMPLES * 2];

ChannelMixer::ChannelMixer(unsigned numChannels_, unsigned stereo_)
	: numChannels(numChannels_)
	, stereo(stereo_)
{
	assert(numChannels <= MAX_CHANNELS);
	assert(stereo == 1 || stereo == 2);
	memset(silence, 0, sizeof(silence));

	// initially no channels are muted
	for (unsigned i = 0; i < numChannels; ++i) {
		muted[i] = false;
	}

	// test: record first channel
	//writer[0].reset(new WavWriter("channel0.wav", 1, 16, 49716));
}

ChannelMixer::~ChannelMixer()
{
}

void ChannelMixer::mixChannels(int* dataOut, unsigned samples)
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
		if ((bufs[i] != 0) && !muted[i]) {
			bufs[unmuted] = bufs[i];
			++unmuted;
		}
	}

	// actually mix channels
	for (unsigned i = 0; i < samples * stereo; ++i) {
		int out = 0;
		for (unsigned j = 0; j < unmuted; ++j) {
			out += bufs[j][i];
		}
		dataOut[i] = out;
	}
}

} // namespace openmsx
