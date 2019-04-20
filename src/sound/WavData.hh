#ifndef WAVDATA_HH
#define WAVDATA_HH

#include "MemBuffer.hh"
#include <cstdint>
#include <string>

namespace openmsx {

class WavData final
{
public:
	/** Construct empty wav. */
	WavData() = default;

	/** Construct from .wav file. */
	explicit WavData(const std::string& filename);

	unsigned getFreq() const { return freq; }
	unsigned getSize() const { return length; }
	const int16_t* getData() const { return buffer.data(); }
	      int16_t* getData()       { return buffer.data(); }
	int16_t getSample(unsigned pos) const;

private:
	MemBuffer<int16_t> buffer;
	unsigned freq;
	unsigned length = 0;
};

} // namespace openmsx

#endif
