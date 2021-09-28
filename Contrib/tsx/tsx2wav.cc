// Compile with:
//   g++ -O3 -g -Wall -Wextra tsx2wav.cc TsxParser.cc -o tsx2wav

#include "TsxParser.hh"
#include "endian.hh"
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, const char** argv)
{
	if (argc != 3) {
		std::cerr << "Usage: tsx2wav <tsx-input-file> <wav-output-file>\n";
		exit(1);
	}

	// -- Read tsx file --
	std::ifstream in(argv[1], std::ios::binary);
	if (!in.is_open()) {
		std::cerr << "Failed to open: " << argv[1] << '\n';
		exit(1);
	}
	std::vector<uint8_t> inBuf(std::istreambuf_iterator<char>{in},
	                           std::istreambuf_iterator<char>{});

	// -- Parse tsx file --
	std::vector<int8_t> wave;
	try {
		std::cout << "Converting TSX to WAV ..." << std::endl;
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
		e += 128;
	}

	// -- Write wav --
	std::ofstream out(argv[2], std::ios::binary);
	if (!out.is_open()) {
		std::cerr << "Failed to open: " << argv[2] << '\n';
		exit(1);
	}

	int channels = 1;
	int bits = 8;
	int frequency = TsxParser::OUTPUT_FREQUENCY;
	int samples = wave.size();

	// write header
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
	memcpy(header.chunkID, "RIFF", sizeof(header.chunkID));
	header.chunkSize     = (samples + 44 - 8 + 1) & ~1; // round up to even number
	memcpy(header.format,     "WAVE", sizeof(header.format));
	memcpy(header.subChunk1ID,"fmt ", sizeof(header.subChunk1ID));
	header.subChunk1Size = 16;
	header.audioFormat   = 1;
	header.numChannels   = channels;
	header.sampleRate    = frequency;
	header.byteRate      = (channels * frequency * bits) / 8;
	header.blockAlign    = (channels * bits) / 8;
	header.bitsPerSample = bits;
	memcpy(header.subChunk2ID, "data", sizeof(header.subChunk2ID));
	header.subChunk2Size = samples;

	out.write(reinterpret_cast<char*>(&header), sizeof(header));

	// write data
	out.write(reinterpret_cast<char*>(wave.data()), samples);
	// data chunk must have an even number of bytes
	if (samples & 1) {
		char pad = 0;
		out.write(&pad, 1);
	}
	if (out.bad()) {
		std::cerr << "Error writing: " << argv[2] << '\n';
		exit(1);
	}
}
