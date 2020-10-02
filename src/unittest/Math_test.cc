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
	CHECK(Math::floodRight(0) == 0);
	CHECK(Math::floodRight(1) == 1);
	CHECK(Math::floodRight(2) == 3);
	CHECK(Math::floodRight(3) == 3);
	CHECK(Math::floodRight(4) == 7);
	CHECK(Math::floodRight(5) == 7);
	CHECK(Math::floodRight(6) == 7);
	CHECK(Math::floodRight(7) == 7);
	CHECK(Math::floodRight(8) == 15);
	CHECK(Math::floodRight(9) == 15);
	CHECK(Math::floodRight(15) == 15);
	CHECK(Math::floodRight(16) == 31);
	CHECK(Math::floodRight(32) == 63);
	CHECK(Math::floodRight(64) == 127);
	CHECK(Math::floodRight(12345) == 16383);
	CHECK(Math::floodRight(0x7F001234) == 0x7FFFFFFF);
	CHECK(Math::floodRight(0x80000000) == 0xFFFFFFFF);
	CHECK(Math::floodRight(0xF0FEDCBA) == 0xFFFFFFFF);
	CHECK(Math::floodRight(0x1234F0FEDCBAULL) == 0x1FFFFFFFFFFFULL);
	CHECK(Math::floodRight(uint8_t(0x12)) == uint8_t(0x1F));
	CHECK(Math::floodRight(uint16_t(0x2512)) == uint16_t(0x3FFF));
}
