#ifndef WAVWRITER_HH
#define WAVWRITER_HH

#include "File.hh"
#include "Mixer.hh"

#include <cassert>
#include <cstdint>
#include <span>
#include <string>

namespace openmsx {

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
	[[nodiscard]] uint32_t getBytes() const { return bytes; }

	/** Flush data to file and update header. Try to make (possibly)
	  * incomplete file already usable for external programs.
	  */
	void flush();

protected:
	WavWriter(const std::string& filename,
	          unsigned channels, unsigned bits, unsigned frequency);
	~WavWriter();

protected:
	File file;
	uint32_t bytes = 0;
};

/** Writes 8-bit WAV files.
  */
class Wav8Writer : public WavWriter
{
public:
	Wav8Writer(const std::string& filename, unsigned channels, unsigned frequency)
		: WavWriter(filename, channels, 8, frequency) {}

	void write(std::span<const uint8_t> buffer);
};

/** Writes 16-bit WAV files.
  */
class Wav16Writer : public WavWriter
{
public:
	Wav16Writer(const std::string& filename, unsigned channels, unsigned frequency)
		: WavWriter(filename, channels, 16, frequency) {}

	void write(std::span<const int16_t> buffer);
	void write(std::span<const float> buffer, float amp = 1.0f);
	void write(std::span<const StereoFloat> buffer, float ampLeft = 1.0f, float ampRight = 1.0f);

	void writeSilence(uint32_t samples);
};

} // namespace openmsx

#endif
