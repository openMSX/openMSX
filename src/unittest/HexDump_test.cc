#include "catch.hpp"

#include "HexDump.hh"

#include "ranges.hh"

#include <bit>

static void test_decode(const std::string& encoded, const std::string& decoded)
{
	auto buf = HexDump::decode(encoded);
	REQUIRE(buf.size() == decoded.size());
	CHECK(ranges::equal(std::span{buf}, decoded));
}

static void test(const std::string& decoded, const std::string& encoded)
{
	CHECK(HexDump::encode(std::span{std::bit_cast<const uint8_t*>(decoded.data()),
	                                decoded.size()})
	      == encoded);
	test_decode(encoded, decoded);
}

TEST_CASE("HexDump")
{
	test("", "");
	test("a", "61");
	test("ab", "61 62");
	test("abc", "61 62 63");
	test("0123456789", "30 31 32 33 34 35 36 37 38 39");
	test("abcdefghijklmnopqrstuvwxyz",
	     "61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F 70\n"
	     "71 72 73 74 75 76 77 78 79 7A");
	test("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
	     "30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46\n"
	     "47 48 49 4A 4B 4C 4D 4E 4F 50 51 52 53 54 55 56\n"
	     "57 58 59 5A 61 62 63 64 65 66 67 68 69 6A 6B 6C\n"
	     "6D 6E 6F 70 71 72 73 74 75 76 77 78 79 7A");
	test("111111111111111111111111111111111111111111111111111111111111111111111111111111"
	     "111111111111111111111111111111111111111111111111111111111111111111111111111111"
	     "111111111111111111111111111111111111111111111111111111111111111111111111111111"
	     "111111111111111111111111111111111111111111111",
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31\n"
	     "31 31 31 31 31 31 31");

	// Decode-only:
	// - extra newlines don't matter
	test_decode("30 31\n32\n33 34", "01234");
	// - no spaces in between is fine as well
	test_decode("3031323334", "01234");
	// - any non [0-9][a-f][A-F] character is ignored
	test_decode("30|31G32g33+34", "01234");
	// - lower-case [a-f] is allowed
	test_decode("4A+4b4c\n4d", "JKLM");
}
