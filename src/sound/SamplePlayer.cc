// $Id$

#include "SamplePlayer.hh"
#include <cassert>

namespace openmsx {

SamplePlayer::SamplePlayer(MSXMixer& mixer, const std::string& name,
                           const std::string& desc, const XMLElement& config)
	: SoundDevice(mixer, name, desc, 1)
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

void SamplePlayer::setOutputRate(unsigned sampleRate)
{
	setInputRate(sampleRate);
	outFreq = sampleRate;
}

void SamplePlayer::play(const void* buffer, unsigned bufferSize,
                        unsigned bits, unsigned inFreq)
{
	sampBuf = buffer;
	count = 0;
	end = (bufferSize - 1) << 8;

	assert((bits == 8) || (bits == 16));
	bits8 = (bits == 8);

	step = (inFreq << 8) / outFreq;

	playing = true;
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
	if (!playing) {
		bufs[0] = 0;
		return;
	}
	for (unsigned i = 0; i < num; ++i) {
		if (count < end) {
			unsigned index = count >> 8;
			int frac  = count & 0xFF;
			int samp0 = getSample(index);
			int samp1 = getSample(index + 1);
			int samp  = ((0x100 - frac) * samp0 + frac * samp1) >> 8;
			bufs[0][i] = 3 * samp;
			count += step;
		} else {
			playing = false;
			bufs[0][i] = 0;
		}
	}
}

} // namespace openmsx
