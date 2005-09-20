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
	void write8mono(unsigned char* buf, size_t len);
	void write16stereo(short left, short right);

	/** Flush data to file and update header. Try to make (possibly)
	  * incomplete file already usable for external programs.
	  */
	void flush();

private:
	FILE* wavfp;
	unsigned bytes;
};

} // namespace openmsx

#endif
