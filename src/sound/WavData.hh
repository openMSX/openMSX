// $Id: $

#ifndef WAVDATA_HH
#define WAVDATA_HH

#include "openmsx.hh"
#include <string>
#include <vector>

namespace openmsx {

class WavData
{
public:
	WavData(const std::string& filename, unsigned bits = 0, unsigned freq = 0);

	unsigned getFreq() const;
	unsigned getBits() const;
	unsigned getSize() const;
	unsigned getChannels() const;
	const void* getData() const;

private:
	std::vector<byte> buffer;
	unsigned bits;
	unsigned freq;
	unsigned length;
	unsigned channels;
};

} // namespace openmsx

#endif
