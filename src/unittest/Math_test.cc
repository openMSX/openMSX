#include "catch.hpp"
#include "Math.hh"

TEST_CASE("Math::clipIntToShort")
{
	CHECK(Math::clipIntToShort(-100000) == -32768);
	CHECK(Math::clipIntToShort( -32769) == -32768);
	CHECK(Math::clipIntToShort( -32768) == -32768);
	CHECK(Math::clipIntToShort( -32767) == -32767);
	CHECK(Math::clipIntToShort( -10000) == -10000);
	CHECK(Math::clipIntToShort(    -10) ==    -10);
	CHECK(Math::clipIntToShort(     -1) ==     -1);
	CHECK(Math::clipIntToShort(      0) ==      0);
	CHECK(Math::clipIntToShort(      1) ==      1);
	CHECK(Math::clipIntToShort(     10) ==     10);
	CHECK(Math::clipIntToShort(   9876) ==   9876);
	CHECK(Math::clipIntToShort(  32766) ==  32766);
	CHECK(Math::clipIntToShort(  32767) ==  32767);
	CHECK(Math::clipIntToShort(  32768) ==  32767);
	CHECK(Math::clipIntToShort( 100000) ==  32767);
}

TEST_CASE("Math::clipIntToByte")
{
	CHECK(Math::clipIntToByte(-100) ==   0);
	CHECK(Math::clipIntToByte( -27) ==   0);
	CHECK(Math::clipIntToByte(  -1) ==   0);
	CHECK(Math::clipIntToByte(   0) ==   0);
	CHECK(Math::clipIntToByte(   1) ==   1);
	CHECK(Math::clipIntToByte(  10) ==  10);
	CHECK(Math::clipIntToByte( 127) == 127);
	CHECK(Math::clipIntToByte( 128) == 128);
	CHECK(Math::clipIntToByte( 222) == 222);
	CHECK(Math::clipIntToByte( 255) == 255);
	CHECK(Math::clipIntToByte( 256) == 255);
	CHECK(Math::clipIntToByte( 257) == 255);
	CHECK(Math::clipIntToByte( 327) == 255);
}

static void testReverseNBits(unsigned x, unsigned n, unsigned expected)
{
	CHECK(Math::reverseNBits(x, n) == expected);
	CHECK(Math::reverseNBits(expected, n) == x);
}
TEST_CASE("Math::reverseNBits")
{
	testReverseNBits(0x0, 4, 0x0);
	testReverseNBits(0x1, 4, 0x8);
	testReverseNBits(0x2, 4, 0x4);
	testReverseNBits(0x3, 4, 0xC);
	testReverseNBits(0x4, 4, 0x2);
	testReverseNBits(0x8, 4, 0x1);
	testReverseNBits(0x6, 4, 0x6);
	testReverseNBits(0xE, 4, 0x7);
	testReverseNBits(0xF, 4, 0xF);
	testReverseNBits(0x00012345, 22, 0x0028B120);
	testReverseNBits(0x0010000F, 32, 0xF0000800);
}

static void testReverseByte(uint8_t x, uint8_t expected)
{
	CHECK(Math::reverseByte (x   ) == expected);
	CHECK(Math::reverseNBits(x, 8) == expected);

	CHECK(Math::reverseByte (expected   ) == x);
	CHECK(Math::reverseNBits(expected, 8) == x);
}
TEST_CASE("Math::reverseByte")
{
	testReverseByte(0x00, 0x00);
	testReverseByte(0x01, 0x80);
	testReverseByte(0x02, 0x40);
	testReverseByte(0x07, 0xE0);
	testReverseByte(0x12, 0x48);
	testReverseByte(0x73, 0xCE);
	testReverseByte(0x7F, 0xFE);
	testReverseByte(0x8C, 0x31);
	testReverseByte(0xAB, 0xD5);
	testReverseByte(0xE4, 0x27);
	testReverseByte(0xF0, 0x0F);
}

TEST_CASE("Math::floodRight")
{
	CHECK(Math::floodRight(0u) == 0);
	CHECK(Math::floodRight(1u) == 1);
	CHECK(Math::floodRight(2u) == 3);
	CHECK(Math::floodRight(3u) == 3);
	CHECK(Math::floodRight(4u) == 7);
	CHECK(Math::floodRight(5u) == 7);
	CHECK(Math::floodRight(6u) == 7);
	CHECK(Math::floodRight(7u) == 7);
	CHECK(Math::floodRight(8u) == 15);
	CHECK(Math::floodRight(9u) == 15);
	CHECK(Math::floodRight(15u) == 15);
	CHECK(Math::floodRight(16u) == 31);
	CHECK(Math::floodRight(32u) == 63);
	CHECK(Math::floodRight(64u) == 127);
	CHECK(Math::floodRight(12345u) == 16383);
	CHECK(Math::floodRight(0x7F001234u) == 0x7FFFFFFF);
	CHECK(Math::floodRight(0x80000000u) == 0xFFFFFFFF);
	CHECK(Math::floodRight(0xF0FEDCBAu) == 0xFFFFFFFF);
	CHECK(Math::floodRight(0x1234F0FEDCBAULL) == 0x1FFFFFFFFFFFULL);
	CHECK(Math::floodRight(uint8_t(0x12)) == uint8_t(0x1F));
	CHECK(Math::floodRight(uint16_t(0x2512)) == uint16_t(0x3FFF));
}

TEST_CASE("Math::div_mod_floor")
{
	auto test = [](int D, int d, int q, int r) {
		REQUIRE(d * q + r == D);
		auto qr = Math::div_mod_floor(D, d);
		CHECK(qr.quotient  == q);
		CHECK(qr.remainder == r);
	};
	test( 10,  3,  3,  1);
	test(-10,  3, -4,  2);
	test( 10, -3, -4, -2);
	test(-10, -3,  3, -1);

	test( 10,  2,  5,  0);
	test(-10,  2, -5,  0);
	test( 10, -2, -5,  0);
	test(-10, -2,  5,  0);
}
