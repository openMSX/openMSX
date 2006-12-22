// $Id$

#include "SamplePlayer.hh"
#include <cassert>

namespace openmsx {

SamplePlayer::SamplePlayer(MSXMixer& mixer, const std::string& name,
                           const std::string& desc, const XMLElement& config)
	: SoundDevice(mixer, name, desc)
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
	setMute(true);
	playing = false;
}

void SamplePlayer::setVolume(int newVolume)
{
	volume = newVolume * 3;
}

void SamplePlayer::setSampleRate(int sampleRate)
{
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

	setMute(false);
	playing = true;
}

bool SamplePlayer::isPlaying() const
{
	return playing;
}

inline int SamplePlayer::getSample(unsigned index)
{
	return bits8
	     ? (((unsigned char*)sampBuf)[index] - 0x80) * 256
	     : ((short*)sampBuf)[index];
}

void SamplePlayer::updateBuffer(unsigned length, int* output,
        const EmuTime& /*start*/, const EmuDuration& /*sampDur*/)
{
	for (unsigned i = 0; i < length; ++i) {
		if (count < end) {
			unsigned index = count >> 8;
			int frac  = count & 0xFF;
			int samp0 = getSample(index);
			int samp1 = getSample(index + 1);
			int samp  = ((0x100 - frac) * samp0 + frac * samp1) >> 8;
			output[i] = (samp * volume) >> 15;
			count += step;
		} else {
			setMute(true);
			playing = false;
			output[i] = 0;
		}
	}
}

} // namespace openmsx
