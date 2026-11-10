#include "catch.hpp"
#include "fixed_capacity_bitset.hh"

TEST_CASE("fixed_capacity_bitset: basic properties and sizing")
{
	// MAX_SIZE = 24 should result in 4 bytes total object size:
	//   3 bytes storage + 1 byte to track size
	STATIC_REQUIRE(sizeof(fixed_capacity_bitset<24>) == 4);

	fixed_capacity_bitset<24> bs(10);
	SECTION("Initial size and max size are correct") {
		REQUIRE(bs.size() == 10);
		REQUIRE(bs.max_size() == 24);
	}
}

TEST_CASE("fixed_capacity_bitset: bit manipulation")
{
	fixed_capacity_bitset<32> bs(16);

	SECTION("Bits default to false (zeroed out)") {
		for (std::size_t i = 0; i < bs.size(); ++i) {
			CHECK(bs[i] == false);
		}
	}

	SECTION("Can set and get individual bits via operator[]") {
		bs[0] = true;
		bs[5] = true;
		bs[15] = true;
		CHECK(bs[ 0] == true);
		CHECK(bs[ 1] == false);
		CHECK(bs[ 5] == true);
		CHECK(bs[15] == true);

		// Toggle back to false
		bs[5] = false;
		CHECK(bs[ 0] == true);
		CHECK(bs[ 1] == false);
		CHECK(bs[ 5] == false);
		CHECK(bs[15] == true);
	}

	SECTION("Bits across byte boundaries work correctly") {
		// Byte 0 ends at bit 7, Byte 1 starts at bit 8
		bs[7] = true;
		bs[8] = true;

		CHECK(bs[7] == true);
		CHECK(bs[8] == true);
		CHECK(bs[6] == false);
		CHECK(bs[9] == false);
	}
}
