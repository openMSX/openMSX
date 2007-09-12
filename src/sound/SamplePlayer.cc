// $Id$

#include "SamplePlayer.hh"
#include "MSXMotherBoard.hh"
#include <cassert>

namespace openmsx {

SamplePlayer::SamplePlayer(MSXMotherBoard& motherBoard, const std::string& name,
                           const std::string& desc, const XMLElement& config)
	: SoundDevice(motherBoard.getMSXMixer(), name, desc, 1)
	, Resample(motherBoard.getGlobalSettings(), 1)
	, inFreq(44100)
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
	playing = false;
}

void SamplePlayer::setOutputRate(unsigned outFreq_)
{
	outFreq = outFreq_;
	setInputRate(inFreq);
	setResampleRatio(inFreq, outFreq);
}

void SamplePlayer::play(const void* buffer, unsigned bufferSize_,
                        unsigned bits, unsigned freq)
{
	sampBuf = buffer;
	index = 0;
	bufferSize = bufferSize_;

	assert((bits == 8) || (bits == 16));
	bits8 = (bits == 8);

	playing = true;

	if (freq != inFreq) {
		// this potentially switches resampler, so there might be
		// some dropped samples if this is done in the middle of
		// playing, though this shouldn't happen often (or at all)
		inFreq = freq;
		setOutputRate(outFreq);
	}
}

bool SamplePlayer::isPlaying() const
{
	return playing;
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
		if (index < bufferSize) {
			int samp = getSample(index++);
			bufs[0][i] = 3 * samp;
		} else {
			playing = false;
			bufs[0][i] = 0;
		}
	}
}

bool SamplePlayer::generateInput(int* buffer, unsigned num)
{
	return mixChannels(buffer, num);
}

bool SamplePlayer::updateBuffer(unsigned length, int* buffer,
     const EmuTime& /*time*/, const EmuDuration& /*sampDur*/)
{
	return generateOutput(buffer, length);
}

} // namespace openmsx
