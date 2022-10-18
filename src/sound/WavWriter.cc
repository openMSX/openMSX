#include "WavWriter.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "Mixer.hh"
#include "endian.hh"
#include "enumerate.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "vla.hh"
#include "xrange.hh"
#include <vector>

namespace openmsx {

WavWriter::WavWriter(const Filename& filename,
                     unsigned channels, unsigned bits, unsigned frequency)
	: file(filename, "wb")
{
	// write wav header
	struct WavHeader {
		char        chunkID[4];     // + 0 'RIFF'
		Endian::L32 chunkSize;      // + 4 total size
		char        format[4];      // + 8 'WAVE'
		char        subChunk1ID[4]; // +12 'fmt '
		Endian::L32 subChunk1Size;  // +16 = 16 (fixed)
		Endian::L16 audioFormat;    // +20 =  1 (fixed)
		Endian::L16 numChannels;    // +22
		Endian::L32 sampleRate;     // +24
		Endian::L32 byteRate;       // +28
		Endian::L16 blockAlign;     // +32
		Endian::L16 bitsPerSample;  // +34
		char        subChunk2ID[4]; // +36 'data'
		Endian::L32 subChunk2Size;  // +40
	} header;

	ranges::copy(std::string_view("RIFF"), std::begin(header.chunkID)); // TODO simplify
	header.chunkSize     = 0; // actual value filled in later
	ranges::copy(std::string_view("WAVE"), std::begin(header.format)); // TODO
	ranges::copy(std::string_view("fmt "), std::begin(header.subChunk1ID)); // TODO
	header.subChunk1Size = 16;
	header.audioFormat   = 1;
	header.numChannels   = channels;
	header.sampleRate    = frequency;
	header.byteRate      = (channels * frequency * bits) / 8;
	header.blockAlign    = (channels * bits) / 8;
	header.bitsPerSample = bits;
	ranges::copy(std::string_view("data"), std::begin(header.subChunk2ID)); // TODO
	header.subChunk2Size = 0; // actual value filled in later

	file.write(&header, sizeof(header));
}

WavWriter::~WavWriter()
{
	try {
		// data chunk must have an even number of bytes
		if (bytes & 1) {
			uint8_t pad = 0;
			file.write(&pad, 1);
		}

		flush(); // write header
	} catch (MSXException&) {
		// ignore, can't throw from destructor
	}
}

void WavWriter::flush()
{
	Endian::L32 totalSize = (bytes + 44 - 8 + 1) & ~1; // round up to even number
	Endian::L32 wavSize   = bytes;

	file.seek(4);
	file.write(&totalSize, 4);
	file.seek(40);
	file.write(&wavSize, 4);
	file.seek(file.getSize()); // SEEK_END
	file.flush();
}

void Wav8Writer::write(const uint8_t* buffer, unsigned samples)
{
	file.write(buffer, samples);
	bytes += samples;
}

void Wav16Writer::write(const int16_t* buffer, unsigned samples)
{
	unsigned size = sizeof(int16_t) * samples;
	if constexpr (Endian::BIG) {
		// Variable length arrays (VLA) are part of c99 but not of c++
		// (not even c++11). Some compilers (like gcc) do support VLA
		// in c++ mode, others (like VC++) don't. Still other compilers
		// (like clang) only support VLA for POD types.
		// To side-step this issue we simply use a std::vector, this
		// code is anyway not performance critical.
		//VLA(Endian::L16, buf, samples); // doesn't work in clang
		std::vector<Endian::L16> buf(buffer, buffer + samples);
		file.write(buf.data(), size);
	} else {
		file.write(buffer, size);
	}
	bytes += size;
}

static int16_t float2int16(float f)
{
	return Math::clipIntToShort(lrintf(32768.0f * f));
}

void Wav16Writer::write(std::span<const float> buffer, float amp)
{
	std::vector<Endian::L16> buf_(buffer.size());
	std::span buf{buf_};
	ranges::transform(buffer, buf.data(), [=](float f) { return float2int16(f * amp); });
	file.write(buf.data(), buf.size_bytes());
	bytes += buf.size_bytes();
}

void Wav16Writer::write(std::span<const StereoFloat> buffer, float ampLeft, float ampRight)
{
	std::vector<Endian::L16> buf(buffer.size() * 2);
	for (auto [i, s] : enumerate(buffer)) { // TODO use zip() in the future
		buf[2 * i + 0] = float2int16(s.left  * ampLeft);
		buf[2 * i + 1] = float2int16(s.right * ampRight);
	}
	file.write(buf.data(), buffer.size_bytes());
	bytes += buffer.size_bytes();
}

void Wav16Writer::writeSilence(unsigned samples)
{
	VLA(int16_t, buf_, samples);
	std::span buf{buf_, samples};
	ranges::fill(buf, 0);
	file.write(buf.data(), buf.size_bytes());
	bytes += buf.size_bytes();
}

} // namespace openmsx
