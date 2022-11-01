#include "catch.hpp"
#include "CRC16.hh"
#include "xrange.hh"
#include <array>

using namespace openmsx;

TEST_CASE("CRC16")
{
	CRC16 crc;
	REQUIRE(crc.getValue() == 0xFFFF);

	// Test a simple sequence
	SECTION("'3 x A1' in a loop") {
		repeat(3, [&] { crc.update(0xA1); });
		CHECK(crc.getValue() == 0xCDB4);
	}
	SECTION("'3 x A1' in one chunk") {
		static constexpr std::array<uint8_t, 3> buf = { 0xA1, 0xA1, 0xA1 };
		crc.update(buf);
		CHECK(crc.getValue() == 0xCDB4);
	}
	SECTION("'3 x A1' via init") {
		crc.init({0xA1, 0xA1, 0xA1});
		CHECK(crc.getValue() == 0xCDB4);
	}

	// Internally the block-update method works on chunks of 8 bytes,
	// so also try a few bigger examples
	//   not (an integer) multiple of 8
	SECTION("'123456789' in a loop") {
		for (char c : {'1', '2', '3', '4', '5', '6', '7', '8', '9'}) crc.update(c);
		CHECK(crc.getValue() == 0x29B1);
	}
	SECTION("'123456789' in one chunk") {
		static constexpr const char* const digits = "123456789";
		crc.update(std::span{reinterpret_cast<const uint8_t*>(digits), 9});
		CHECK(crc.getValue() == 0x29B1);
	}
	// same as disk sector size
	SECTION("512 bytes") {
		std::array<uint8_t, 512> buf;
		for (auto i : xrange(512)) buf[i] = i & 255;
		SECTION("in a loop") {
			for (char c : buf) crc.update(c);
			CHECK(crc.getValue() == 0x56EE);
		}
		SECTION("in one chunk") {
			crc.update(buf);
			CHECK(crc.getValue() == 0x56EE);
		}
	}

	// Tests for init<..>()
	// It's possible to pass up-to 4 parameters. Verify that this gives
	// the same result as the other 2 CRC calculation methods.
	SECTION("'11' in a loop") {
		for (char c : {0x11}) crc.update(c);
		CHECK(crc.getValue() == 0xE3E0);
	}
	SECTION("'11' in one chunk") {
		static constexpr std::array<uint8_t, 1> buf = {0x11};
		crc.update(buf);
		CHECK(crc.getValue() == 0xE3E0);
	}
	SECTION("'11' via init") {
		crc.init({0x11});
		CHECK(crc.getValue() == 0xE3E0);
	}

	SECTION("'11 22' in a loop") {
		for (char c : {0x11, 0x22}) crc.update(c);
		CHECK(crc.getValue() == 0x296D);
	}
	SECTION("'11 22' in one chunk") {
		static constexpr std::array<uint8_t, 2> buf = {0x11, 0x22};
		crc.update(buf);
		CHECK(crc.getValue() == 0x296D);
	}
	SECTION("'11 22' via init") {
		crc.init({0x11, 0x22});
		CHECK(crc.getValue() == 0x296D);
	}

	SECTION("'11 22 33' in a loop") {
		for (char c : {0x11, 0x22, 0x33}) crc.update(c);
		CHECK(crc.getValue() == 0xDE7B);
	}
	SECTION("'11 22 33' in one chunk") {
		static constexpr std::array<uint8_t, 3> buf = {0x11, 0x22, 0x33};
		crc.update(buf);
		CHECK(crc.getValue() == 0xDE7B);
	}
	SECTION("'11 22 33' via init") {
		crc.init({0x11, 0x22, 0x33});
		CHECK(crc.getValue() == 0xDE7B);
	}

	SECTION("'11 22 33 44' in a loop") {
		for (char c : {0x11, 0x22, 0x33, 0x44}) crc.update(c);
		CHECK(crc.getValue() == 0x59F3);
	}
	SECTION("'11 22 33 44' in one chunk") {
		static constexpr std::array<uint8_t, 4> buf = {0x11, 0x22, 0x33, 0x44};
		crc.update(buf);
		CHECK(crc.getValue() == 0x59F3);
	}
	SECTION("'11 22 33 44' via init") {
		crc.init({0x11, 0x22, 0x33, 0x44});
		CHECK(crc.getValue() == 0x59F3);
	}
}


#if 0

// Functions to inspect the quality of the generated code

uint16_t test_init()
{
	// I verified that gcc is capable of optimizing this to 'return 0xcdb4'.
	CRC16 crc;
	crc.init({0xA1, 0xA1, 0xA1});
	return crc.getValue();
}

#endif
