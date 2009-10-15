// $Id$

#ifndef WAVWRITER_HH
#define WAVWRITER_HH

#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class File;
class Filename;

class WavWriter : private noncopyable
{
public:
	WavWriter(const Filename& filename,
	          unsigned channels, unsigned bits, unsigned frequency);
	~WavWriter();

	void write8mono(const unsigned char* buf, unsigned len);
	void write16stereo(const short* buffer, unsigned samples);
	void write16mono  (const int* buffer, unsigned samples, int amp = 1);
	void write16stereo(const int* buffer, unsigned samples, int amp = 1);
	void write16silence(unsigned stereo, unsigned samples);

	/** Returns false if there has been data written to the wav image.
	 */
	bool isEmpty() const;

	/** Flush data to file and update header. Try to make (possibly)
	  * incomplete file already usable for external programs.
	  */
	void flush();

private:
	const std::auto_ptr<File> file;
	unsigned bytes;
};

} // namespace openmsx

#endif
