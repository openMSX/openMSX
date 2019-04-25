#ifndef WAVDATA_HH
#define WAVDATA_HH

#include "File.hh"
#include "MemBuffer.hh"
#include <cstdint>
#include <string>

namespace openmsx {

class WavData
{
public:
	/** Construct empty wav. */
	WavData() = default;

	/** Construct from .wav file. */
	explicit WavData(const std::string& filename)
		: WavData(File(filename)) {}
	explicit WavData(File file);

	unsigned getFreq() const { return freq; }
	unsigned getSize() const { return length; }
	const int16_t* getData() const { return buffer.data(); }
	      int16_t* getData()       { return buffer.data(); }
	int16_t getSample(unsigned pos) const {
		return (pos < length) ? buffer[pos] : 0;
	}

private:
	MemBuffer<int16_t> buffer;
	unsigned freq = 0;
	unsigned length = 0;
};

} // namespace openmsx

#endif
