// Compile with:
//   g++ -std=c++20 -O3 -g -Wall -Wextra -I ../../src/utils/ tsx2wav.cc TsxParser.cc -o tsx2wav

#include "TsxParser.hh"

#include "endian.hh"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>

int main(int argc, const char** argv)
{
	auto arg = std::span(argv, argc);
	if (arg.size() != 3) {
		std::cerr << "Usage: tsx2wav <tsx-input-file> <wav-output-file>\n";
		exit(1);
	}

	// -- Read tsx file --
	std::ifstream in(arg[1], std::ios::binary);
	if (!in.is_open()) {
		std::cerr << "Failed to open: " << arg[1] << '\n';
		exit(1);
	}
	std::vector<uint8_t> inBuf(std::istreambuf_iterator<char>{in},
	                           std::istreambuf_iterator<char>{});

	// -- Parse tsx file --
	std::vector<int8_t> wave;
	try {
		std::cout << "Converting TSX to WAV ...\n" << std::flush;
		TsxParser parser(inBuf);
		wave = parser.stealOutput();

		// print info
		double len = double(wave.size()) / TsxParser::OUTPUT_FREQUENCY;
		std::cout << "  WAV length = " << len << " seconds\n";
		if (auto type = parser.getFirstFileType()) {
			std::cout << "  first file type = " <<
				[&] {
					switch (*type) {
					case TsxParser::FileType::ASCII:  return "ASCII";
					case TsxParser::FileType::BINARY: return "BINARY";
					case TsxParser::FileType::BASIC:  return "BASIC";
					default: return "UNKNOWN";
					}
				}() << '\n';
		}
		for (const auto& msg : parser.getMessages()) {
			std::cout << "  message: " << msg << '\n';
		}
	} catch (const std::string& err) {
		std::cerr << "Invalid TSX file: " << err << '\n';
		exit(1);
	}

	// convert 8-bit signed to unsigned
	for (int8_t& e : wave) {
		e = int8_t(e + 128);
	}

	// -- Write wav --
	std::ofstream out(arg[2], std::ios::binary);
	if (!out.is_open()) {
		std::cerr << "Failed to open: " << arg[2] << '\n';
		exit(1);
	}

	int channels = 1;
	int bits = 8;
	int frequency = TsxParser::OUTPUT_FREQUENCY;
	auto samples = int(wave.size());

	// write header
	struct WavHeader {
		std::array<char, 4> chunkID;     // + 0 'RIFF'
		Endian::L32 chunkSize;           // + 4 total size
		std::array<char, 4> format;      // + 8 'WAVE'
		std::array<char, 4> subChunk1ID; // +12 'fmt '
		Endian::L32 subChunk1Size;       // +16 = 16 (fixed)
		Endian::L16 audioFormat;         // +20 =  1 (fixed)
		Endian::L16 numChannels;         // +22
		Endian::L32 sampleRate;          // +24
		Endian::L32 byteRate;            // +28
		Endian::L16 blockAlign;          // +32
		Endian::L16 bitsPerSample;       // +34
		std::array<char, 4> subChunk2ID; // +36 'data'
		Endian::L32 subChunk2Size;       // +40
	} header;
	memcpy(header.chunkID.data(), "RIFF", header.chunkID.size());
	header.chunkSize     = (samples + 44 - 8 + 1) & ~1; // round up to even number
	memcpy(header.format.data(),      "WAVE", header.format.size());
	memcpy(header.subChunk1ID.data(), "fmt ", header.subChunk1ID.size());
	header.subChunk1Size = 16;
	header.audioFormat   = 1;
	header.numChannels   = channels;
	header.sampleRate    = frequency;
	header.byteRate      = (channels * frequency * bits) / 8;
	header.blockAlign    = (channels * bits) / 8;
	header.bitsPerSample = bits;
	memcpy(header.subChunk2ID.data(), "data", header.subChunk2ID.size());
	header.subChunk2Size = samples;

	out.write(std::bit_cast<char*>(&header), sizeof(header));

	// write data
	out.write(std::bit_cast<char*>(wave.data()), samples);
	// data chunk must have an even number of bytes
	if (samples & 1) {
		char pad = 0;
		out.write(&pad, 1);
	}
	if (out.bad()) {
		std::cerr << "Error writing: " << arg[2] << '\n';
		exit(1);
	}
}
