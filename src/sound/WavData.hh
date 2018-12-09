#ifndef WAVDATA_HH
#define WAVDATA_HH

#include "MemBuffer.hh"
#include <cstdint>
#include <string>

namespace openmsx {

class WavData
{
public:
	/** Construct empty wav. */
	WavData() = default;

	/** Construct from .wav file, optionally convert to a specific
	 * bit-depth and sample rate. */
	explicit WavData(const std::string& filename, unsigned bits = 0, unsigned freq = 0);

	unsigned getFreq() const { return freq; }
	unsigned getBits() const { return bits; }
	unsigned getSize() const { return length; }
	const void* getData() const { return buffer.data(); }
	      void* getData()       { return buffer.data(); }

private:
	MemBuffer<uint8_t> buffer;
	unsigned bits;
	unsigned freq;
	unsigned length = 0;
};

} // namespace openmsx

#endif
