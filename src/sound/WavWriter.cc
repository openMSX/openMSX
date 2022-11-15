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
#include <array>
#include <vector>

namespace openmsx {

WavWriter::WavWriter(const Filename& filename,
                     unsigned channels, unsigned bits, unsigned frequency)
	: file(filename, "wb")
{
	// write wav header
	struct WavHeader {
		std::array<char, 4> chunkID;       // + 0 'RIFF'
		Endian::L32         chunkSize;     // + 4 total size
		std::array<char, 4> format;        // + 8 'WAVE'
		std::array<char, 4> subChunk1ID;   // +12 'fmt '
		Endian::L32         subChunk1Size; // +16 = 16 (fixed)
		Endian::L16         audioFormat;   // +20 =  1 (fixed)
		Endian::L16         numChannels;   // +22
		Endian::L32         sampleRate;    // +24
		Endian::L32         byteRate;      // +28
		Endian::L16         blockAlign;    // +32
		Endian::L16         bitsPerSample; // +34
		std::array<char, 4> subChunk2ID;   // +36 'data'
		Endian::L32         subChunk2Size; // +40
	} header;

	ranges::copy(std::string_view("RIFF"), header.chunkID);
	header.chunkSize     = 0; // actual value filled in later
	ranges::copy(std::string_view("WAVE"), header.format);
	ranges::copy(std::string_view("fmt "), header.subChunk1ID);
	header.subChunk1Size = 16;
	header.audioFormat   = 1;
	header.numChannels   = channels;
	header.sampleRate    = frequency;
	header.byteRate      = (channels * frequency * bits) / 8;
	header.blockAlign    = (channels * bits) / 8;
	header.bitsPerSample = bits;
	ranges::copy(std::string_view("data"), header.subChunk2ID);
	header.subChunk2Size = 0; // actual value filled in later

	file.write(std::span{&header, 1});
}

WavWriter::~WavWriter()
{
	try {
		// data chunk must have an even number of bytes
		if (bytes & 1) {
			std::array<uint8_t, 1> pad = {0};
			file.write(pad);
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
	file.write(std::span{&totalSize, 1});
	file.seek(40);
	file.write(std::span{&wavSize, 1});
	file.seek(file.getSize()); // SEEK_END
	file.flush();
}

void Wav8Writer::write(std::span<const uint8_t> buffer)
{
	file.write(buffer);
	bytes += narrow<uint32_t>(buffer.size_bytes());
}

void Wav16Writer::write(std::span<const int16_t> buffer)
{
	if constexpr (Endian::BIG) {
		// Variable length arrays (VLA) are part of c99 but not of c++
		// (not even c++11). Some compilers (like gcc) do support VLA
		// in c++ mode, others (like VC++) don't. Still other compilers
		// (like clang) only support VLA for POD types.
		// To side-step this issue we simply use a std::vector, this
		// code is anyway not performance critical.
		//VLA(Endian::L16, buf, samples); // doesn't work in clang
		std::vector<Endian::L16> buf(buffer.begin(), buffer.end());
		file.write(std::span{buf});
	} else {
		file.write(buffer);
	}
	bytes += narrow<uint32_t>(buffer.size_bytes());
}

static int16_t float2int16(float f)
{
	return Math::clipToInt16(lrintf(32768.0f * f));
}

void Wav16Writer::write(std::span<const float> buffer, float amp)
{
	std::vector<Endian::L16> buf_(buffer.size());
	std::span buf{buf_};
	ranges::transform(buffer, buf.data(), [=](float f) { return float2int16(f * amp); });
	file.write(buf);
	bytes += narrow<uint32_t>(buf.size_bytes());
}

void Wav16Writer::write(std::span<const StereoFloat> buffer, float ampLeft, float ampRight)
{
	std::vector<Endian::L16> buf(buffer.size() * 2);
	for (auto [i, s] : enumerate(buffer)) { // TODO use zip() in the future
		buf[2 * i + 0] = float2int16(s.left  * ampLeft);
		buf[2 * i + 1] = float2int16(s.right * ampRight);
	}
	std::span s{buf};
	file.write(s);
	bytes += narrow<uint32_t>(s.size_bytes());
}

void Wav16Writer::writeSilence(uint32_t samples)
{
	VLA(int16_t, buf, samples);
	ranges::fill(buf, 0);
	file.write(buf);
	bytes += narrow<uint32_t>(buf.size_bytes());
}

} // namespace openmsx
