// $Id$

#include "SamplePlayer.hh"
#include "MSXMotherBoard.hh"
#include "GlobalSettings.hh"
#include "Reactor.hh"
#include "WavData.hh"
#include "MSXCliComm.hh"
#include "FileContext.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

SamplePlayer::SamplePlayer(MSXMotherBoard& motherBoard, const std::string& name,
                           const std::string& desc, const XMLElement& config,
                           const std::string& samplesBaseName, unsigned numSamples)
	: SoundDevice(motherBoard.getMSXMixer(), name, desc, 1)
	, Resample(motherBoard.getReactor().getGlobalSettings().getResampleSetting(), 1)
	, inFreq(44100)
{
	bool alreadyWarned = false;
	samples.resize(numSamples); // initialize with NULL ptrs
	for (unsigned i = 0; i < numSamples; ++i) {
		try {
			SystemFileContext context;
			std::string filename = StringOp::Builder() <<
				samplesBaseName << i << ".wav";
			samples[i].reset(new WavData(context.resolve(
				motherBoard.getCommandController(), filename)));
		} catch (MSXException& e) {
			if (!alreadyWarned) {
				alreadyWarned = true;
				motherBoard.getMSXCliComm().printWarning(
					"Couldn't read playball sample data: " +
					e.getMessage() +
					". Continuing without sample data.");
			}
		}
	}

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

void SamplePlayer::stopRepeat()
{
	nextSampleNum = unsigned(-1);
}

void SamplePlayer::setOutputRate(unsigned outFreq_)
{
	outFreq = outFreq_;
	setInputRate(inFreq);
	setResampleRatio(inFreq, outFreq);
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
		if (freq != inFreq) {
			// this potentially switches resampler, so there might be
			// some dropped samples if this is done in the middle of
			// playing, though this shouldn't happen often (or at all)
			inFreq = freq;
			setOutputRate(outFreq);
		}
	} else {
		reset();
	}
}

void SamplePlayer::repeat(unsigned sampleNum)
{
	assert(sampleNum < samples.size());
	nextSampleNum = sampleNum;
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

bool SamplePlayer::generateInput(int* buffer, unsigned num)
{
	return mixChannels(buffer, num);
}

bool SamplePlayer::updateBuffer(unsigned length, int* buffer,
     EmuTime::param /*time*/, EmuDuration::param /*sampDur*/)
{
	return generateOutput(buffer, length);
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
