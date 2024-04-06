#include "catch.hpp"
#include "WavData.hh"
#include "MemoryBufferFile.hh"
#include "MSXException.hh"
#include "xrange.hh"

using namespace openmsx;

TEST_CASE("WavData, default constructor")
{
	WavData wav;
	CHECK(wav.getSize() == 0);
	CHECK(wav.getFreq() == 0);
	for (auto i : xrange(10)) {
		CHECK(wav.getSample(i) == 0);
	}
}

TEST_CASE("WavData, parser")
{
	SECTION("empty file") {
		File file = memory_buffer_file(std::span<const uint8_t>());
		try {
			WavData wav(std::move(file));
			CHECK(false);
		} catch (MSXException& e) {
			CHECK(e.getMessage() == "Read beyond end of wav file.");
		}
	}
	SECTION("garbage data") {
		// length of 'WavHeader' but contains garbage data
		static constexpr auto buffer = std::to_array<uint8_t>({
			0x00,0x01,0x02,0x03, 0x04,0x05,0x06,0x07, 0x08,0x09,0x0a,0x0b, 0x0c,0x0d,0x0e,0x0f,
			0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17, 0x18,0x19,0x1a,0x1b, 0x1c,0x1d,0x1e,0x1f,
			0x20,0x21,0x22,0x23,
		});
		try {
			WavData wav(memory_buffer_file(buffer));
			CHECK(false);
		} catch (MSXException& e) {
			CHECK(e.getMessage() == "Invalid WAV file.");
		}
	}
	SECTION("unsupported format") {
		// header check passes, but format check fails
		static constexpr auto buffer = std::to_array<uint8_t>({
			'R', 'I', 'F', 'F',  0x04,0x05,0x06,0x07, 'W', 'A', 'V', 'E',  'f', 'm', 't' ,' ',
			0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17, 0x18,0x19,0x1a,0x1b, 0x1c,0x1d,0x1e,0x1f,
			0x20,0x21,0x22,0x23,
		});
		try {
			WavData wav(memory_buffer_file(buffer));
			CHECK(false);
		} catch (MSXException& e) {
			CHECK(e.getMessage() == "WAV format unsupported, must be 8 or 16 bit PCM.");
		}
	}
	SECTION("missing data chunk") {
		// (almost) correct header, but missing 'data' chunk
		static constexpr auto buffer = std::to_array<uint8_t>({
			'R', 'I', 'F', 'F',  0x04,0x05,0x06,0x07, 'W', 'A', 'V', 'E',  'f', 'm', 't' ,' ',
			0x10,0x00,0x00,0x00, 0x01,0x00,0x01,0x00, 0x44,0xac,0x00,0x00, 0x88,0x58,0x01,0x00,
			0x02,0x00,0x10,0x00,
		});
		try {
			WavData wav(memory_buffer_file(buffer));
			CHECK(false);
		} catch (MSXException& e) {
			CHECK(e.getMessage() == "Read beyond end of wav file.");
		}
	}
	SECTION("data chunk with incorrect length") {
		// data-chunk says there are 100 bytes sample data, but those are missing
		static constexpr auto buffer = std::to_array<uint8_t>({
			'R', 'I', 'F', 'F',  0x04,0x05,0x06,0x07, 'W', 'A', 'V', 'E',  'f', 'm', 't' ,' ',
			0x10,0x00,0x00,0x00, 0x01,0x00,0x01,0x00, 0x44,0xac,0x00,0x00, 0x88,0x58,0x01,0x00,
			0x02,0x00,0x10,0x00, 'd', 'a', 't', 'a',  0x64,0x00,0x00,0x00,
		});
		try {
			WavData wav(memory_buffer_file(buffer));
			CHECK(false);
		} catch (MSXException& e) {
			CHECK(e.getMessage() == "Read beyond end of wav file.");
		}
	}
	SECTION("finally a correct, but empty wav file") {
		// (correct except for the 'riffSize' field)
		static constexpr auto buffer = std::to_array<uint8_t>({
			'R', 'I', 'F', 'F',  0x04,0x05,0x06,0x07, 'W', 'A', 'V', 'E',  'f', 'm', 't' ,' ',
			0x10,0x00,0x00,0x00, 0x01,0x00,0x01,0x00, 0x44,0xac,0x00,0x00, 0x88,0x58,0x01,0x00,
			0x02,0x00,0x10,0x00,
			'd', 'a', 't', 'a',  0x00,0x00,0x00,0x00,
		});
		WavData wav(memory_buffer_file(buffer));
		CHECK(wav.getSize() == 0);
		CHECK(wav.getFreq() == 44100);
	}
}

TEST_CASE("WavData, content")
{
	SECTION("mono, 8 bit") {
		// samples get converted 8bit -> 16bit
		static constexpr auto buffer = std::to_array<uint8_t>({
			'R', 'I', 'F', 'F',  0x04,0x05,0x06,0x07, 'W', 'A', 'V', 'E',  'f', 'm', 't' ,' ',
			0x10,0x00,0x00,0x00, 0x01,0x00,0x01,0x00, 0x44,0xac,0x00,0x00, 0x44,0xac,0x00,0x00,
			0x02,0x00,0x08,0x00,
			'd', 'a', 't', 'a',  0x05,0x00,0x00,0x00,
			0x00, 0x45, 0x80, 0xbe, 0xff
		});
		WavData wav(memory_buffer_file(buffer));
		CHECK(wav.getSize() == 5);
		CHECK(wav.getFreq() == 44100);
		CHECK(wav.getSample(0) == -0x8000);
		CHECK(wav.getSample(1) == -0x3b00);
		CHECK(wav.getSample(2) ==  0x0000);
		CHECK(wav.getSample(3) ==  0x3e00);
		CHECK(wav.getSample(4) ==  0x7f00);
		CHECK(wav.getSample(5) ==  0); // past end
	}
	SECTION("mono, 16 bit") {
		// samples are unmodified
		static constexpr auto buffer = std::to_array<uint8_t>({
			'R', 'I', 'F', 'F',  0x04,0x05,0x06,0x07, 'W', 'A', 'V', 'E',  'f', 'm', 't' ,' ',
			0x10,0x00,0x00,0x00, 0x01,0x00,0x01,0x00, 0x44,0xac,0x00,0x00, 0x44,0xac,0x00,0x00,
			0x02,0x00,0x10,0x00,
			'd', 'a', 't', 'a',  0x08,0x00,0x00,0x00,
			0x23,0x45, 0x00,0x7e, 0xff,0xff, 0x23,0xc0
		});
		WavData wav(memory_buffer_file(buffer));
		CHECK(wav.getSize() == 4);
		CHECK(wav.getFreq() == 44100);
		CHECK(wav.getSample(0) ==  0x4523);
		CHECK(wav.getSample(1) ==  0x7e00);
		CHECK(wav.getSample(2) == -0x0001);
		CHECK(wav.getSample(3) == -0x3fdd);
		CHECK(wav.getSample(4) ==  0); // past end
		CHECK(wav.getSample(5) ==  0);
	}
	SECTION("stereo, 8 bit") {
		// 1st channel is kept, 2nd channel gets discarded
		static constexpr auto buffer = std::to_array<uint8_t>({
			'R', 'I', 'F', 'F',  0x04,0x05,0x06,0x07, 'W', 'A', 'V', 'E',  'f', 'm', 't' ,' ',
			0x10,0x00,0x00,0x00, 0x01,0x00,0x02,0x00, 0x44,0xac,0x00,0x00, 0x44,0xac,0x00,0x00,
			0x02,0x00,0x08,0x00,
			'd', 'a', 't', 'a',  0x08,0x00,0x00,0x00,
			0x00,0xff, 0x40,0xef, 0x80,0xdf, 0xc0,0xcf
		});
		WavData wav(memory_buffer_file(buffer));
		CHECK(wav.getSize() == 4);
		CHECK(wav.getFreq() == 44100);
		CHECK(wav.getSample(0) == -0x8000);
		CHECK(wav.getSample(1) == -0x4000);
		CHECK(wav.getSample(2) ==  0x0000);
		CHECK(wav.getSample(3) ==  0x4000);
		CHECK(wav.getSample(4) ==  0); // past end
	}
	SECTION("stereo, 16 bit") {
		// 1st channel is kept, 2nd channel gets discarded
		static constexpr auto buffer = std::to_array<uint8_t>({
			'R', 'I', 'F', 'F',  0x04,0x05,0x06,0x07, 'W', 'A', 'V', 'E',  'f', 'm', 't' ,' ',
			0x10,0x00,0x00,0x00, 0x01,0x00,0x02,0x00, 0x44,0xac,0x00,0x00, 0x44,0xac,0x00,0x00,
			0x02,0x00,0x10,0x00,
			'd', 'a', 't', 'a',  0x10,0x00,0x00,0x00,
			0x12,0x34,0xff,0xee, 0x56,0x78,0xdd,0xcc, 0x9a,0xbc,0xbb,0xaa, 0xde,0xf0,0x99,0x88,
		});
		WavData wav(memory_buffer_file(buffer));
		CHECK(wav.getSize() == 4);
		CHECK(wav.getFreq() == 44100);
		CHECK(wav.getSample(0) ==  0x3412);
		CHECK(wav.getSample(1) ==  0x7856);
		CHECK(wav.getSample(2) == -0x4366);
		CHECK(wav.getSample(3) == -0x0f22);
		CHECK(wav.getSample(4) ==  0); // past end
	}
}
