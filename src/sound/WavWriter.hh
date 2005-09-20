// $Id$

#ifndef WAVWRITER_HH
#define WAVWRITER_HH

#include <string>
#include <cstdio>

namespace openmsx {

class WavWriter
{
public:
	WavWriter(const std::string& filename,
	          unsigned channels, unsigned bits, unsigned frequency);
	~WavWriter();

	void write8mono(unsigned char val);
	void write8mono(unsigned char *buf, size_t len);
	void write16stereo(short left, short right);

private:
	FILE* wavfp;
	unsigned bytes;
};

} // namespace openmsx

#endif
