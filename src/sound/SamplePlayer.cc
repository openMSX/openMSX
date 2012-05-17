// $Id$

#include "SamplePlayer.hh"
#include "MSXMotherBoard.hh"
#include "WavData.hh"
#include "MSXCliComm.hh"
#include "FileContext.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

SamplePlayer::SamplePlayer(MSXMotherBoard& motherBoard, const std::string& name,
                           const std::string& desc, const DeviceConfig& config,
                           const std::string& samplesBaseName, unsigned numSamples,
                           const std::string& alternativeName)
	: ResampledSoundDevice(motherBoard, name, desc, 1)
{
	setInputRate(44100); // Initialize with dummy value

	bool alreadyWarned = false;
	samples.resize(numSamples); // initialize with NULL ptrs
	SystemFileContext context;
	for (unsigned i = 0; i < numSamples; ++i) {
		try {
			std::string filename = StringOp::Builder() <<
				samplesBaseName << i << ".wav";
			samples[i].reset(new WavData(context.resolve(filename)));
		} catch (MSXException& e1) {
			try {
				if (alternativeName.empty()) throw;
				std::string filename = StringOp::Builder() <<
					alternativeName << i << ".wav";
				samples[i].reset(new WavData(context.resolve(filename)));
			} catch (MSXException& /*e2*/) {
				if (!alreadyWarned) {
					alreadyWarned = true;
					// print message from the 1st error
					motherBoard.getMSXCliComm().printWarning(
						"Couldn't read " + name + " sample data: " +
						e1.getMessage() +
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

void SamplePlayer::stopRepeat()
{
	nextSampleNum = unsigned(-1);
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
	if ((currentSampleNum < samples.size()) && samples[currentSampleNum].get()) {
		WavData* wav = samples[currentSampleNum].get();
		sampBuf = wav->getData();
		bufferSize = wav->getSize();

		unsigned bits = wav->getBits();
		assert((bits == 8) || (bits == 16));
		bits8 = (bits == 8);

		unsigned freq = wav->getFreq();
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

bool SamplePlayer::isPlaying() const
{
	return currentSampleNum != unsigned(-1);
}

inline int SamplePlayer::getSample(unsigned index)
{
	return bits8
	     ? (static_cast<const unsigned char*>(sampBuf)[index] - 0x80) * 256
	     :  static_cast<const short*        >(sampBuf)[index];
}

void SamplePlayer::generateChannels(int** bufs, unsigned num)
{
	if (!isPlaying()) {
		bufs[0] = 0;
		return;
	}
	for (unsigned i = 0; i < num; ++i) {
		if (index >= bufferSize) {
			if (nextSampleNum != unsigned(-1)) {
				doRepeat();
			} else {
				// no need to add 0 to the remainder of bufs[0]
				//  bufs[0][i] += 0;
				currentSampleNum = unsigned(-1);
				break;
			}
		}
		int samp = getSample(index++);
		bufs[0][i] += 3 * samp;
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
