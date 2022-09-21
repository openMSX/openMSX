#ifndef WAVWRITER_HH
#define WAVWRITER_HH

#include "File.hh"
#include "one_of.hh"
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
	[[nodiscard]] bool isEmpty() const { return bytes == 0; }

	/** Returns the number of bytes (not samples) written so far.
	 */
	[[nodiscard]] unsigned getBytes() const { return bytes; }

	/** Flush data to file and update header. Try to make (possibly)
	  * incomplete file already usable for external programs.
	  */
	void flush();

protected:
	WavWriter(const Filename& filename,
	          unsigned channels, unsigned bits, unsigned frequency);
	~WavWriter();

protected:
	File file;
	unsigned bytes = 0;
};

/** Writes 8-bit WAV files.
  */
class Wav8Writer : public WavWriter
{
public:
	Wav8Writer(const Filename& filename, unsigned channels, unsigned frequency)
		: WavWriter(filename, channels, 8, frequency) {}

	void write(const uint8_t* buffer, unsigned stereo, unsigned samples) {
		assert(stereo == one_of(1u, 2u));
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
		assert(stereo == one_of(1u, 2u));
		write(buffer, stereo * samples);
	}
	void write(const float* buffer, unsigned stereo, unsigned samples,
	           float ampLeft, float ampRight);
	void writeSilence(unsigned stereo, unsigned samples) {
		assert(stereo == one_of(1u, 2u));
		writeSilence(stereo * samples);
	}

private:
	void write(const int16_t* buffer, unsigned samples);
	void writeSilence(unsigned samples);
};

} // namespace openmsx

#endif
