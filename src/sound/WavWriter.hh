// $Id$

#ifndef WAVWRITER_HH
#define WAVWRITER_HH

#include "noncopyable.hh"
#include <cassert>
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

	void write8(const unsigned char* buffer, unsigned stereo,
	            unsigned samples) {
		assert(stereo == 1 || stereo == 2);
		write8(buffer, stereo * samples);
	}
	void write16(const short* buffer, unsigned stereo, unsigned samples) {
		assert(stereo == 1 || stereo == 2);
		write16(buffer, stereo * samples);
	}
	void write16(const int* buffer, unsigned stereo, unsigned samples,
	             int amp = 1) {
		assert(stereo == 1 || stereo == 2);
		write16(buffer, stereo * samples, amp);
	}
	void write16silence(unsigned stereo, unsigned samples) {
		assert(stereo == 1 || stereo == 2);
		write16silence(stereo * samples);
	}

	/** Returns false if there has been data written to the wav image.
	 */
	bool isEmpty() const;

	/** Flush data to file and update header. Try to make (possibly)
	  * incomplete file already usable for external programs.
	  */
	void flush();

private:
	void write8(const unsigned char* buffer, unsigned samples);
	void write16(const short* buffer, unsigned samples);
	void write16(const int* buffer, unsigned samples, int amp);
	void write16silence(unsigned samples);

	const std::auto_ptr<File> file;
	unsigned bytes;
};

} // namespace openmsx

#endif
