#include "catch.hpp"

#include "sha1.hh"

#include "ranges.hh"
#include "xrange.hh"

#include <bit>
#include <cstring>
#include <sstream>

using namespace openmsx;

TEST_CASE("Sha1Sum: constructors")
{
	SECTION("default") {
		Sha1Sum sum;
		CHECK(sum.empty());
		CHECK(sum.toString() == "0000000000000000000000000000000000000000");
	}

	SECTION("from string, ok") {
		Sha1Sum sum("1234567890123456789012345678901234567890");
		CHECK(!sum.empty());
		CHECK(sum.toString() == "1234567890123456789012345678901234567890");
	}
	SECTION("from string, too short") {
		CHECK_THROWS(Sha1Sum("123456789012345678901234567890123456789"));
	}
	SECTION("from string, too long") {
		CHECK_THROWS(Sha1Sum("12345678901234567890123456789012345678901"));
	}
	SECTION("from string, invalid char") {
		CHECK_THROWS(Sha1Sum("g234567890123456789012345678901234567890"));
	}
}

TEST_CASE("Sha1Sum: parse")
{
	Sha1Sum sum;

	// precondition: string must be 40 chars long
	SECTION("ok") {
		sum.parse40(subspan<40>("abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"));
		CHECK(sum.toString() == "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
	}
	SECTION("invalid char") {
		CHECK_THROWS(sum.parse40(subspan<40>("abcdabcdabcdabcdabcdabcdabcdabcd-bcdabcd")));
	}
}

TEST_CASE("Sha1Sum: clear")
{
	Sha1Sum sum("1111111111111111111111111111111111111111");
	REQUIRE(!sum.empty());
	REQUIRE(sum.toString() != "0000000000000000000000000000000000000000");

	sum.clear();
	CHECK(sum.empty());
	CHECK(sum.toString() == "0000000000000000000000000000000000000000");
}

static void testCompare(const Sha1Sum& x, const Sha1Sum& y, bool expectEqual, bool expectLess)
{
	if (expectEqual) {
		REQUIRE(!expectLess);
		CHECK(  x == y );  CHECK(  y == x );
		CHECK(!(x != y));  CHECK(!(y != x));

		CHECK(!(x <  y));  CHECK(!(y <  x));
		CHECK(  x <= y );  CHECK(  y <= x );
		CHECK(!(x >  y));  CHECK(!(y >  x));
		CHECK(  x >= y );  CHECK(  y >= x );
	} else {
		CHECK(!(x == y));  CHECK(!(y == x));
		CHECK(  x != y );  CHECK(  y != x );
		if (expectLess) {
			CHECK(  x <  y );  CHECK(!(y <  x));
			CHECK(  x <= y );  CHECK(!(y <= x));
			CHECK(!(x >  y));  CHECK(  y >  x );
			CHECK(!(x >= y));  CHECK(  y >= x );
		} else {
			CHECK(!(x <  y));  CHECK(  y <  x );
			CHECK(!(x <= y));  CHECK(  y <= x );
			CHECK(  x >  y );  CHECK(!(y >  x));
			CHECK(  x >= y );  CHECK(!(y >= x));
		}
	}
}

TEST_CASE("Sha1Sum: comparisons")
{
	Sha1Sum sumA ("0000000000000000000000000000000000000000");
	Sha1Sum sumB ("0000000000000000000000000000000000000001");
	Sha1Sum sumB2("0000000000000000000000000000000000000001");
	Sha1Sum sumC ("0000000000100000000000000000000000000001");

	testCompare(sumB, sumB2, true, false);
	testCompare(sumA, sumB, false, true);
	testCompare(sumC, sumB, false, false);
}

TEST_CASE("Sha1Sum: stream")
{
	Sha1Sum sum("abcdef0123ABCDEF0123abcdef0123ABCDEF0123");
	std::stringstream ss;
	ss << sum;
	CHECK(ss.str() == "abcdef0123abcdef0123abcdef0123abcdef0123");
}


TEST_CASE("sha1: calc")
{
	const char* in = "abc";
	Sha1Sum output = SHA1::calc({std::bit_cast<const uint8_t*>(in), strlen(in)});
	CHECK(output.toString() == "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST_CASE("sha1: update,digest")
{
	SHA1 sha1;
	SECTION("single block") {
		const char* in = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
		sha1.update({std::bit_cast<const uint8_t*>(in), strlen(in)});
		Sha1Sum sum1 = sha1.digest();
		Sha1Sum sum2 = sha1.digest(); // call 2nd time is ok
		CHECK(sum1 == sum2);
		CHECK(sum1.toString() == "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
	}
	SECTION("multiple blocks") {
		const char* in = "aaaaaaaaaaaaaaaaaaaaaaaaa";
		REQUIRE(strlen(in) == 25);
		repeat(40000, [&] {
			sha1.update({std::bit_cast<const uint8_t*>(in), strlen(in)});
		});
		// 25 * 40'000 = 1'000'000 repetitions of "a"
		Sha1Sum sum = sha1.digest();
		CHECK(sum.toString() == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
	}
}

TEST_CASE("sha1: finalize")
{
	// white-box test for boundary cases in finalize()
	SHA1 sha1;
	const char* in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	SECTION("0") {
		sha1.update({std::bit_cast<const uint8_t*>(in), size_t(0)});
		auto sum = sha1.digest();
		CHECK(sum.toString() == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
	}
	SECTION("25") {
		sha1.update({std::bit_cast<const uint8_t*>(in), 25});
		auto sum = sha1.digest();
		CHECK(sum.toString() == "44f4647e1542a79d7d68ceb7f75d1dbf77fdebfc");
	}
	SECTION("55") {
		sha1.update({std::bit_cast<const uint8_t*>(in), 55});
		auto sum = sha1.digest();
		CHECK(sum.toString() == "c1c8bbdc22796e28c0e15163d20899b65621d65a");
	}
	SECTION("56") {
		sha1.update({std::bit_cast<const uint8_t*>(in), 56});
		auto sum = sha1.digest();
		CHECK(sum.toString() == "c2db330f6083854c99d4b5bfb6e8f29f201be699");
	}
	SECTION("60") {
		sha1.update({std::bit_cast<const uint8_t*>(in), 60});
		auto sum = sha1.digest();
		CHECK(sum.toString() == "13d956033d9af449bfe2c4ef78c17c20469c4bf1");
	}
	SECTION("63") {
		sha1.update({std::bit_cast<const uint8_t*>(in), 63});
		auto sum = sha1.digest();
		CHECK(sum.toString() == "03f09f5b158a7a8cdad920bddc29b81c18a551f5");
	}
	SECTION("64") {
		sha1.update({std::bit_cast<const uint8_t*>(in), 64});
		auto sum = sha1.digest();
		CHECK(sum.toString() == "0098ba824b5c16427bd7a1122a5a442a25ec644d");
	}
}
