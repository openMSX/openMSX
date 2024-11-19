#ifndef WAVDATA_HH
#define WAVDATA_HH

#include "File.hh"
#include "MemBuffer.hh"
#include "MSXException.hh"

#include "endian.hh"
#include "one_of.hh"
#include "xrange.hh"

#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <string>

namespace openmsx {

class WavData
{
	struct NoFilter {
		void setFreq(unsigned /*freq*/) const {}
		uint16_t operator()(uint16_t x) const { return x; }
	};

public:
	/** Construct empty wav. */
	WavData() = default;

	/** Construct from .wav file and optional filter. */
	template<typename Filter = NoFilter>
	explicit WavData(const std::string& filename, Filter filter = {})
		: WavData(File(filename), filter) {}

	template<typename Filter = NoFilter>
	explicit WavData(File file, Filter filter = {});

	[[nodiscard]] unsigned getFreq() const { return freq; }
	[[nodiscard]] size_t getSize() const { return buffer.size(); }
	[[nodiscard]] int16_t getSample(size_t pos) const {
		return (pos < buffer.size()) ? buffer[pos] : int16_t(0);
	}

private:
	template<typename T>
	[[nodiscard]] static const T* read(std::span<const uint8_t> raw, size_t offset, size_t count = 1);

private:
	MemBuffer<int16_t> buffer;
	unsigned freq = 0;
};

////

template<typename T>
inline const T* WavData::read(std::span<const uint8_t> raw, size_t offset, size_t count)
{
	if ((offset + count * sizeof(T)) > raw.size()) {
		throw MSXException("Read beyond end of wav file.");
	}
	return std::bit_cast<const T*>(raw.data() + offset);
}

template<typename Filter>
inline WavData::WavData(File file, Filter filter)
{
	// Read and check header
	auto raw = file.mmap();
	struct WavHeader {
		std::array<char, 4> riffID;
		Endian::L32 riffSize;
		std::array<char, 4> riffType;
		std::array<char, 4> fmtID;
		Endian::L32 fmtSize;
		Endian::L16 wFormatTag;
		Endian::L16 wChannels;
		Endian::L32 dwSamplesPerSec;
		Endian::L32 dwAvgBytesPerSec;
		Endian::L16 wBlockAlign;
		Endian::L16 wBitsPerSample;
	};
	const auto* header = read<WavHeader>(raw, 0);
	if ((std::string_view{header->riffID.data(),   4} != "RIFF") ||
	    (std::string_view{header->riffType.data(), 4} != "WAVE") ||
	    (std::string_view{header->fmtID.data(),    4} != "fmt ")) {
		throw MSXException("Invalid WAV file.");
	}
	unsigned bits = header->wBitsPerSample;
	if ((header->wFormatTag != 1) || (bits != one_of(8u, 16u))) {
		throw MSXException("WAV format unsupported, must be 8 or 16 bit PCM.");
	}
	freq = header->dwSamplesPerSec;
	unsigned channels = header->wChannels;

	// Skip any extra format bytes
	size_t pos = 20 + header->fmtSize;

	// Find 'data' chunk
	struct DataHeader {
		std::array<char, 4> dataID;
		Endian::L32 chunkSize;
	};
	const DataHeader* dataHeader;
	while (true) {
		// Read chunk header
		dataHeader = read<DataHeader>(raw, pos);
		pos += sizeof(DataHeader);
		if (std::string_view{dataHeader->dataID.data(), 4} == "data") break;
		// Skip non-data chunk
		pos += dataHeader->chunkSize;
	}

	// Read and convert sample data
	size_t length = dataHeader->chunkSize / ((bits / 8) * channels);
	buffer.resize(length);
	filter.setFreq(freq);
	auto convertLoop = [&](const auto* in, auto convertFunc) {
		for (auto i : xrange(length)) {
			buffer[i] = filter(convertFunc(*in));
			in += channels; // discard all but the first channel
		}
	};
	if (bits == 8) {
		convertLoop(read<uint8_t>(raw, pos, length * channels),
		            [](uint8_t u8) { return int16_t((int16_t(u8) - 0x80) << 8); });
	} else {
		convertLoop(read<Endian::L16>(raw, pos, length * channels),
		            [](Endian::L16 s16) { return int16_t(s16); });
	}
}

} // namespace openmsx

#endif
