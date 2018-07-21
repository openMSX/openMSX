#ifndef WAVWRITER_HH
#define WAVWRITER_HH

#include "File.hh"
#include <cassert>
#include <cstdint>

namespace openmsx {

class Filename;

/** Base class for writing WAV files.
  */
class WavWriter
{
public:
	/** Returns false if there has been data written to the wav image.
	 */
	bool isEmpty() const { return bytes == 0; }

	/** Flush data to file and update header. Try to make (possibly)
	  * incomplete file already usable for external programs.
	  */
	void flush();

protected:
	WavWriter(const Filename& filename,
	          unsigned channels, unsigned bits, unsigned frequency);
	~WavWriter();

	File file;
	unsigned bytes;
};

/** Writes 8-bit WAV files.
  */
class Wav8Writer : public WavWriter
{
public:
	Wav8Writer(const Filename& filename, unsigned channels, unsigned frequency)
		: WavWriter(filename, channels, 8, frequency) {}

	void write(const uint8_t* buffer, unsigned stereo, unsigned samples) {
		assert(stereo == 1 || stereo == 2);
		write(buffer, stereo * samples);
	}

private:
	void write(const uint8_t* buffer, unsigned samples);
};

/** Writes 16-bit WAV files.
  */
class Wav16Writer : public WavWriter
{
public:
	Wav16Writer(const Filename& filename, unsigned channels, unsigned frequency)
		: WavWriter(filename, channels, 16, frequency) {}

	void write(const int16_t* buffer, unsigned stereo, unsigned samples) {
		assert(stereo == 1 || stereo == 2);
		write(buffer, stereo * samples);
	}
	void write(const int* buffer, unsigned stereo, unsigned samples,
	           float ampLeft, float ampRight);
	void writeSilence(unsigned stereo, unsigned samples) {
		assert(stereo == 1 || stereo == 2);
		writeSilence(stereo * samples);
	}

private:
	void write(const int16_t* buffer, unsigned samples);
	void writeSilence(unsigned samples);
};

} // namespace openmsx

#endif
