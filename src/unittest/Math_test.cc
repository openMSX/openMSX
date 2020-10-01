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

TEST_CASE("Math::countLeadingZeros")
{
	// undefined for 0
	CHECK(Math::countLeadingZeros(0x00000001) == 31);
	CHECK(Math::countLeadingZeros(0x00000002) == 30);
	CHECK(Math::countLeadingZeros(0x00000003) == 30);
	CHECK(Math::countLeadingZeros(0x00000004) == 29);
	CHECK(Math::countLeadingZeros(0x00000005) == 29);
	CHECK(Math::countLeadingZeros(0x00000007) == 29);
	CHECK(Math::countLeadingZeros(0x00000008) == 28);
	CHECK(Math::countLeadingZeros(0x00000081) == 24);
	CHECK(Math::countLeadingZeros(0x00000134) == 23);
	CHECK(Math::countLeadingZeros(0x00000234) == 22);
	CHECK(Math::countLeadingZeros(0x00008234) == 16);
	CHECK(Math::countLeadingZeros(0x00021234) == 14);
	CHECK(Math::countLeadingZeros(0x00421234) ==  9);
	CHECK(Math::countLeadingZeros(0x01421234) ==  7);
	CHECK(Math::countLeadingZeros(0x01421234) ==  7);
	CHECK(Math::countLeadingZeros(0x11421234) ==  3);
	CHECK(Math::countLeadingZeros(0x31421234) ==  2);
	CHECK(Math::countLeadingZeros(0x61421234) ==  1);
	CHECK(Math::countLeadingZeros(0x91421234) ==  0);
	CHECK(Math::countLeadingZeros(0xF1421234) ==  0);
}

TEST_CASE("Math::countTrailingZeros")
{
	// undefined for 0
	CHECK(Math::countTrailingZeros(0x00000000'00000001ull) ==  0);
	CHECK(Math::countTrailingZeros(0x00000000'00000002ull) ==  1);
	CHECK(Math::countTrailingZeros(0x00000000'00000003ull) ==  0);
	CHECK(Math::countTrailingZeros(0x00000000'00000004ull) ==  2);
	CHECK(Math::countTrailingZeros(0x00000000'00000100ull) ==  8);
	CHECK(Math::countTrailingZeros(0x00000000'80000000ull) == 31);
	CHECK(Math::countTrailingZeros(0x00000001'00000000ull) == 32);
	CHECK(Math::countTrailingZeros(0x80000000'00000000ull) == 63);
	CHECK(Math::countTrailingZeros(0x80000000'00000001ull) ==  0);
}

TEST_CASE("Math::findFirstSet")
{
	// undefined for 0
	CHECK(Math::findFirstSet(0x00000000ull) ==  0);
	CHECK(Math::findFirstSet(0x00000001ull) ==  1);
	CHECK(Math::findFirstSet(0x00000002ull) ==  2);
	CHECK(Math::findFirstSet(0x00000003ull) ==  1);
	CHECK(Math::findFirstSet(0x00000004ull) ==  3);
	CHECK(Math::findFirstSet(0x00000100ull) ==  9);
	CHECK(Math::findFirstSet(0x80000000ull) == 32);
	CHECK(Math::findFirstSet(0x80000001ull) ==  1);
}
