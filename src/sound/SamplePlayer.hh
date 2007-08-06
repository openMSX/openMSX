// $Id$

#ifndef SAMPLEPLAYER_HH
#define SAMPLEPLAYER_HH

#include "SoundDevice.hh"

namespace openmsx {

class SamplePlayer : public SoundDevice
{
public:
	SamplePlayer(MSXMixer& mixer, const std::string& name,
	             const std::string& desc, const XMLElement& config);
	~SamplePlayer();

	void reset();
	void play(const void* buffer, unsigned bufferSize,
	          unsigned bits, unsigned inFreq);
	bool isPlaying() const;

private:
	inline int getSample(unsigned index);

	// SoundDevice
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);

	const void* sampBuf;
	unsigned outFreq;
	unsigned count;
	unsigned step;
	unsigned end;
	bool playing;
	bool bits8;
};

} // namespace openmsx

#endif
