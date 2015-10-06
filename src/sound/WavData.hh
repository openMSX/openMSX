#ifndef WAVDATA_HH
#define WAVDATA_HH

#include "MemBuffer.hh"
#include "openmsx.hh"
#include <string>

namespace openmsx {

class WavData
{
public:
	/** Construct empty wav. */
	WavData() : length(0) {}

	/** Construct from .wav file, optionally convert to a specific
	 * bit-depth and sample rate. */
	WavData(const std::string& filename, unsigned bits = 0, unsigned freq = 0);

	unsigned getFreq() const { return freq; }
	unsigned getBits() const { return bits; }
	unsigned getSize() const { return length; }
	unsigned getChannels() const { return channels; }
	const void* getData() const { return buffer.data(); }

private:
	MemBuffer<byte> buffer;
	unsigned bits;
	unsigned freq;
	unsigned length;
	unsigned channels;
};

} // namespace openmsx

#endif
