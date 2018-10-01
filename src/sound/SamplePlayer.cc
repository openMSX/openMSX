#include "SamplePlayer.hh"
#include "DeviceConfig.hh"
#include "CliComm.hh"
#include "FileContext.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

SamplePlayer::SamplePlayer(const std::string& name_, const std::string& desc,
                           const DeviceConfig& config,
                           const std::string& samplesBaseName, unsigned numSamples,
                           const std::string& alternativeName)
	: ResampledSoundDevice(config.getMotherBoard(), name_, desc, 1)
{
	setInputRate(44100); // Initialize with dummy value

	bool alreadyWarned = false;
	samples.resize(numSamples); // initialize with empty wavs
	auto context = systemFileContext();
	for (unsigned i = 0; i < numSamples; ++i) {
		try {
			std::string filename = strCat(samplesBaseName, i, ".wav");
			samples[i] = WavData(context.resolve(filename));
		} catch (MSXException& e1) {
			try {
				if (alternativeName.empty()) throw;
				std::string filename = strCat(
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
		sampBuf = wav.getData();
		bufferSize = wav.getSize();

		unsigned bits = wav.getBits();
		assert((bits == 8) || (bits == 16));
		bits8 = (bits == 8);

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

inline int SamplePlayer::getSample(unsigned idx)
{
	return bits8
	     ? (static_cast<const uint8_t*>(sampBuf)[idx] - 0x80) * 256
	     :  static_cast<const int16_t*>(sampBuf)[idx];
}

void SamplePlayer::generateChannels(int** bufs, unsigned num)
{
	// Single channel device: replace content of bufs[0] (not add to it).
	if (!isPlaying()) {
		bufs[0] = nullptr;
		return;
	}
	for (unsigned i = 0; i < num; ++i) {
		if (index >= bufferSize) {
			if (nextSampleNum != unsigned(-1)) {
				doRepeat();
			} else {
				currentSampleNum = unsigned(-1);
				// fill remaining buffer with zeros
				do {
					bufs[0][i++] = 0;
				} while (i < num);
				break;
			}
		}
		bufs[0][i] = 3 * getSample(index++);
	}
}

void SamplePlayer::doRepeat()
{
	play(nextSampleNum);
}

template<typename Archive>
void SamplePlayer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("index", index);
	ar.serialize("currentSampleNum", currentSampleNum);
	ar.serialize("nextSampleNum", nextSampleNum);
	if (ar.isLoader()) {
		setWavParams();
	}
}
INSTANTIATE_SERIALIZE_METHODS(SamplePlayer);

} // namespace openmsx
