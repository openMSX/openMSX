// $Id: $

#ifndef WAVDATA_HH
#define WAVDATA_HH

#include <string>

namespace openmsx {

class WavData
{
public:
	WavData(const std::string& filename, unsigned bits = 0, unsigned freq = 0);
	~WavData();

	unsigned getFreq() const;
	unsigned getBits() const;
	unsigned getSize() const;
	const void* getData() const;

private:
	void* buffer;
	unsigned bits;
	unsigned freq;
	unsigned length;
};

} // namespace openmsx

#endif
