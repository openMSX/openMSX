// $Id: $

#ifndef WAVDATA_HH
#define WAVDATA_HH

#include <string>

namespace openmsx {

class WavData
{
public:
	WavData(const std::string& filename, int bits = 0, int freq = 0);
	~WavData();

	unsigned getFreq() const;
	unsigned getBits() const;
	unsigned getSize() const;
	const void* getData() const;

private:
	int bits;
	int freq;
	int length;
	void* buffer;
};

} // namespace openmsx

#endif
