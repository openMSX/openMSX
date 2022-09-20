#include "WavWriter.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "endian.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "vla.hh"
#include "xrange.hh"
#include <cstring>
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

	memcpy(header.chunkID,     "RIFF", sizeof(header.chunkID));
	header.chunkSize     = 0; // actual value filled in later
	memcpy(header.format,      "WAVE", sizeof(header.format));
	memcpy(header.subChunk1ID, "fmt ", sizeof(header.subChunk1ID));
	header.subChunk1Size = 16;
	header.audioFormat   = 1;
	header.numChannels   = channels;
	header.sampleRate    = frequency;
	header.byteRate      = (channels * frequency * bits) / 8;
	header.blockAlign    = (channels * bits) / 8;
	header.bitsPerSample = bits;
	memcpy(header.subChunk2ID, "data", sizeof(header.subChunk2ID));
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

void Wav16Writer::write(const float* buffer, unsigned stereo, unsigned samples,
                        float ampLeft, float ampRight)
{
	assert(stereo == one_of(1u, 2u));
	std::vector<Endian::L16> buf(samples * stereo);
	if (stereo == 1) {
		assert(ampLeft == ampRight);
		for (auto i : xrange(samples)) {
			buf[i] = float2int16(buffer[i] * ampLeft);
		}
	} else {
		for (auto i : xrange(samples)) {
			buf[2 * i + 0] = float2int16(buffer[2 * i + 0] * ampLeft);
			buf[2 * i + 1] = float2int16(buffer[2 * i + 1] * ampRight);
		}
	}
	unsigned size = sizeof(int16_t) * samples * stereo;
	file.write(buf.data(), size);
	bytes += size;
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
