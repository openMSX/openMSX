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

	unsigned getFreq() const;
	unsigned getBits() const;
	unsigned getSize() const;
	unsigned getChannels() const;
	const void* getData() const;

private:
	MemBuffer<byte> buffer;
	unsigned bits;
	unsigned freq;
	unsigned length;
	unsigned channels;
};

} // namespace openmsx

#endif
