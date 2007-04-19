// $Id: $

#ifndef CHANNELMIXER_HH
#define CHANNELMIXER_HH

#include <memory>

namespace openmsx {

class WavWriter;

class ChannelMixer
{
public:
	static const unsigned MAX_CHANNELS = 24;

protected:
	ChannelMixer(unsigned numChannels, unsigned stereo = 1);
	virtual ~ChannelMixer();

	void mixChannels(int* dataOut, unsigned num);
	virtual void generateChannels(int** buffers, unsigned num) = 0;

private:
	const unsigned numChannels;
	const unsigned stereo;
	std::auto_ptr<WavWriter> writer[MAX_CHANNELS];
	bool muted[MAX_CHANNELS];
};

} // namespace openmsx

#endif
