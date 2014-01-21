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

	// Move constructor/assignment
	//  Normally these are auto-generated, but vs2013 isn't fully c++11
	//  compliant yet.
	WavData(WavData&& other)
		: buffer(std::move(other.buffer))
		, bits(other.bits)
		, freq(other.freq)
		, length(other.length)
		, channels(other.channels) {}
	WavData& operator=(WavData&& other) {
		buffer = std::move(other.buffer);
		bits = other.bits;
		freq = other.freq;
		length = other.length;
		channels = other.channels;
		return *this;
	}

	unsigned getFreq() const;
	unsigned getBits() const;
	unsigned getSize() const;
	unsigned getChannels() const;
	const void* getData() const;

private:
#if defined(_MSC_VER)
	// Make non-copyable/assignable
	//  work around limitation in vs2013, see comment in MemBuffer.
	WavData(const WavData&);
	WavData& operator=(const WavData&);
#endif

	MemBuffer<byte> buffer;
	unsigned bits;
	unsigned freq;
	unsigned length;
	unsigned channels;
};

} // namespace openmsx

#endif
