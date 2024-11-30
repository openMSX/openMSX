#include "SamplePlayer.hh"
#include "DeviceConfig.hh"
#include "MSXCliComm.hh"
#include "FileContext.hh"
#include "MSXException.hh"
#include "narrow.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <cassert>
#include <memory>

namespace openmsx {

static constexpr unsigned DUMMY_INPUT_RATE = 44100; // actual rate depends on .wav files

[[nodiscard]] static auto loadSamples(
	std::string_view name, const DeviceConfig& config,
	std::string_view baseName, std::string_view alternativeName,
	unsigned numSamples)
{
	dynarray<WavData> result(numSamples); // initialize with empty WAVs

	bool alreadyWarned = false;
	const auto& context = systemFileContext();
	for (auto i : xrange(numSamples)) {
		try {
			auto filename = tmpStrCat(baseName, i, ".wav");
			result[i] = WavData(context.resolve(filename));
		} catch (MSXException& e1) {
			try {
				if (alternativeName.empty()) throw;
				auto filename = tmpStrCat(
					alternativeName, i, ".wav");
				result[i] = WavData(context.resolve(filename));
			} catch (MSXException& /*e2*/) {
				if (!alreadyWarned) {
					alreadyWarned = true;
					// print message from the 1st error
					config.getCliComm().printWarning(
						"Couldn't read ", name, " sample data: ",
						e1.getMessage(),
						". Continuing without sample data.");
				}
			}
		}
	}
	return result;
}

SamplePlayer::SamplePlayer(const std::string& name_, static_string_view desc,
                           const DeviceConfig& config,
                           std::string_view samplesBaseName, unsigned numSamples,
                           std::string_view alternativeName)
	: ResampledSoundDevice(config.getMotherBoard(), name_, desc, 1, DUMMY_INPUT_RATE, false)
	, samples(loadSamples(name_, config, samplesBaseName, alternativeName, numSamples))
{
	registerSound(config);
	reset();
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
		const auto& wav = samples[currentSampleNum];
		bufferSize = narrow<unsigned>(wav.getSize());

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

void SamplePlayer::generateChannels(std::span<float*> bufs, unsigned num)
{
	// Single channel device: replace content of bufs[0] (not add to it).
	assert(bufs.size() == 1);
	if (!isPlaying()) {
		bufs[0] = nullptr;
		return;
	}

	const auto& wav = samples[currentSampleNum];
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
		bufs[0][i] = narrow<float>(3 * wav.getSample(index++));
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
	if constexpr (Archive::IS_LOADER) {
		setWavParams();
	}
}
INSTANTIATE_SERIALIZE_METHODS(SamplePlayer);

} // namespace openmsx
