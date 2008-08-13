// $Id$

#ifndef WAVWRITER_HH
#define WAVWRITER_HH

#include "noncopyable.hh"
#include <cstdio>

namespace openmsx {

class Filename;

class WavWriter : private noncopyable
{
public:
	WavWriter(const Filename& filename,
	          unsigned channels, unsigned bits, unsigned frequency);
	~WavWriter();

	void write8mono(unsigned char val);
	void write8mono(unsigned char* buf, unsigned len);
	void write16stereo(short* buffer, unsigned samples);
	void write16mono  (int* buffer, unsigned samples, int amp = 1);
	void write16stereo(int* buffer, unsigned samples, int amp = 1);

	/** Returns false if there has been data written to the wav image.
	 */
	bool isEmpty() const;

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
