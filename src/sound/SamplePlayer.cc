#include "SamplePlayer.hh"
#include "DeviceConfig.hh"
#include "CliComm.hh"
#include "FileContext.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {

constexpr unsigned DUMMY_INPUT_RATE = 44100; // actual rate depends on .wav files

SamplePlayer::SamplePlayer(const std::string& name_, static_string_view desc,
                           const DeviceConfig& config,
                           const std::string& samplesBaseName, unsigned numSamples,
                           const std::string& alternativeName)
	: ResampledSoundDevice(config.getMotherBoard(), name_, desc, 1, DUMMY_INPUT_RATE, false)
{
	bool alreadyWarned = false;
	samples.resize(numSamples); // initialize with empty WAVs
	auto context = systemFileContext();
	for (auto i : xrange(numSamples)) {
		try {
			auto filename = tmpStrCat(samplesBaseName, i, ".wav");
			samples[i] = WavData(context.resolve(filename));
		} catch (MSXException& e1) {
			try {
				if (alternativeName.empty()) throw;
				auto filename = tmpStrCat(
					alternativeName, i, ".wav");
				samples[i] = WavData(context.resolve(filename));
			} catch (MSXException& /*e2*/) {
				if (!alreadyWarned) {
					alreadyWarned = true;
					// print message from the 1st error
					config.getCliComm().printWarning(
						"Couldn't read ", name_, " sample data: ",
						e1.getMessage(),
						". Continuing without sample data.");
				}
			}
		}
	}

	registerSound(config);
	reset();

	// avoid UMR on serialize
	index = 0;
}

SamplePlayer::~SamplePlayer()
{
	unregisterSound();
}

void SamplePlayer::reset()
{
	currentSampleNum = unsigned(-1);
	stopRepeat();
}

void SamplePlayer::play(unsigned sampleNum)
{
	assert(sampleNum < samples.size());
	currentSampleNum = sampleNum;
	index = 0;
	setWavParams();
}

void SamplePlayer::setWavParams()
{
	if ((currentSampleNum < samples.size()) &&
	    samples[currentSampleNum].getSize()) {
		auto& wav = samples[currentSampleNum];
		bufferSize = wav.getSize();

		unsigned freq = wav.getFreq();
		if (freq != getInputRate()) {
			// this potentially switches resampler, so there might be
			// some dropped samples if this is done in the middle of
			// playing, though this shouldn't happen often (or at all)
			setInputRate(freq);
			createResampler();
		}
	} else {
		reset();
	}
}

void SamplePlayer::repeat(unsigned sampleNum)
{
	assert(sampleNum < samples.size());
	nextSampleNum = sampleNum;
	if (!isPlaying()) {
		doRepeat();
	}
}

void SamplePlayer::generateChannels(float** bufs, unsigned num)
{
	// Single channel device: replace content of bufs[0] (not add to it).
	if (!isPlaying()) {
		bufs[0] = nullptr;
		return;
	}

	auto& wav = samples[currentSampleNum];
	for (auto i : xrange(num)) {
		if (index >= bufferSize) {
			if (nextSampleNum != unsigned(-1)) {
				doRepeat();
			} else {
				currentSampleNum = unsigned(-1);
				// fill remaining buffer with zeros
				do {
					bufs[0][i++] = 0.0f;
				} while (i < num);
				break;
			}
		}
		bufs[0][i] = 3 * wav.getSample(index++);
	}
}

void SamplePlayer::doRepeat()
{
	play(nextSampleNum);
}

template<typename Archive>
void SamplePlayer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("index",            index,
	             "currentSampleNum", currentSampleNum,
	             "nextSampleNum",    nextSampleNum);
	if (ar.isLoader()) {
		setWavParams();
	}
}
INSTANTIATE_SERIALIZE_METHODS(SamplePlayer);

} // namespace openmsx
