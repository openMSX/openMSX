#ifndef WAVWRITER_HH
#define WAVWRITER_HH

#include "noncopyable.hh"
#include <cassert>
#include <memory>

namespace openmsx {

class File;
class Filename;

/** Base class for writing WAV files.
  */
class WavWriter : private noncopyable
{
public:
	/** Returns false if there has been data written to the wav image.
	 */
	bool isEmpty() const;

	/** Flush data to file and update header. Try to make (possibly)
	  * incomplete file already usable for external programs.
	  */
	void flush();

protected:
	WavWriter(const Filename& filename,
	          unsigned channels, unsigned bits, unsigned frequency);
	~WavWriter();

	const std::unique_ptr<File> file;
	unsigned bytes;
};

/** Writes 8-bit WAV files.
  */
class Wav8Writer : public WavWriter
{
public:
	Wav8Writer(const Filename& filename, unsigned channels, unsigned frequency)
		: WavWriter(filename, channels, 8, frequency)
	{}

	void write(const unsigned char* buffer, unsigned stereo,
	           unsigned samples) {
		assert(stereo == 1 || stereo == 2);
		write(buffer, stereo * samples);
	}

private:
	void write(const unsigned char* buffer, unsigned samples);
};

/** Writes 16-bit WAV files.
  */
class Wav16Writer : public WavWriter
{
public:
	Wav16Writer(const Filename& filename, unsigned channels, unsigned frequency)
		: WavWriter(filename, channels, 16, frequency)
	{}

	void write(const short* buffer, unsigned stereo, unsigned samples) {
		assert(stereo == 1 || stereo == 2);
		write(buffer, stereo * samples);
	}
	void write(const int* buffer, unsigned stereo, unsigned samples,
	           int amp = 1) {
		assert(stereo == 1 || stereo == 2);
		write(buffer, stereo * samples, amp);
	}
	void writeSilence(unsigned stereo, unsigned samples) {
		assert(stereo == 1 || stereo == 2);
		writeSilence(stereo * samples);
	}

private:
	void write(const short* buffer, unsigned samples);
	void write(const int* buffer, unsigned samples, int amp);
	void writeSilence(unsigned samples);
};

} // namespace openmsx

#endif
