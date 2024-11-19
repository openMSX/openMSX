#include "catch.hpp"

#include "Base64.hh"

#include "ranges.hh"

#include <bit>

static void test_decode(const std::string& encoded, const std::string& decoded)
{
	auto buf = Base64::decode(encoded);
	REQUIRE(buf.size() == decoded.size());
	CHECK(ranges::equal(std::span{buf}, decoded));
}

static void test(const std::string& decoded, const std::string& encoded)
{
	CHECK(Base64::encode(std::span{std::bit_cast<const uint8_t*>(decoded.data()),
	                               decoded.size()})
	      == encoded);
	test_decode(encoded, decoded);
}

TEST_CASE("Base64")
{
	// Test vectors verified with 'base64' tool from coreutils.
	test("", "");
	test("a", "YQ==");
	test("a\n", "YQo=");
	test("ab\n", "YWIK");
	test("abc\n", "YWJjCg==");
	test("0123456789\n", "MDEyMzQ1Njc4OQo=");
	test("abcdefghijklmnopqrstuvwxyz\n", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXoK");
	test("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n",
	     "MDEyMzQ1Njc4OUFCQ0RFRkdISUpLTE1OT1BRUlNUVVZXWFlaYWJjZGVmZ2hpamtsbW5vcHFyc3R1\n"
	     "dnd4eXoK");
	test("111111111111111111111111111111111111111111111111111111111111111111111111111111"
	     "111111111111111111111111111111111111111111111111111111111111111111111111111111"
	     "111111111111111111111111111111111111111111111111111111111111111111111111111111"
	     "111111111111111111111111111111111111111111111\n",
	     "MTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTEx\n"
	     "MTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTEx\n"
	     "MTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTEx\n"
	     "MTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTEx\n"
	     "MTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExCg==");

	// Decode-only:
	// - extra newlines don't matter
	test_decode("M\nDEyM\nzQ1Njc\n4OQo=", "0123456789\n");
	// - no newlines at all is fine as well
	test_decode("MDEyMzQ1Njc4OUFCQ0RFRkdISUpLTE1OT1BRUlNUVVZXWFlaYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXoK",
	            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n");
}
