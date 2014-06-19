#include "catch.hpp"
#include "Math.hh"

TEST_CASE("Math::isPowerOfTwo")
{
	// don't check 0
	CHECK( Math::isPowerOfTwo(1));
	CHECK( Math::isPowerOfTwo(2));
	CHECK(!Math::isPowerOfTwo(3));
	CHECK( Math::isPowerOfTwo(4));
	CHECK(!Math::isPowerOfTwo(5));
	CHECK(!Math::isPowerOfTwo(6));
	CHECK(!Math::isPowerOfTwo(7));
	CHECK( Math::isPowerOfTwo(8));
	CHECK(!Math::isPowerOfTwo(9));
	CHECK(!Math::isPowerOfTwo(15));
	CHECK( Math::isPowerOfTwo(16));
	CHECK(!Math::isPowerOfTwo(17));
	CHECK( Math::isPowerOfTwo(32));
	CHECK( Math::isPowerOfTwo(64));
	CHECK(!Math::isPowerOfTwo(255));
	CHECK( Math::isPowerOfTwo(256));
	CHECK( Math::isPowerOfTwo(512));
	CHECK( Math::isPowerOfTwo(1024));
	CHECK( Math::isPowerOfTwo(2048));
	CHECK( Math::isPowerOfTwo(4096));
	CHECK( Math::isPowerOfTwo(8192));
	CHECK(!Math::isPowerOfTwo(0xffff));
	CHECK( Math::isPowerOfTwo(0x10000));
	CHECK( Math::isPowerOfTwo(0x80000000));
	CHECK(!Math::isPowerOfTwo(0xffffffff));
}

TEST_CASE("Math::powerOfTwo")
{
	CHECK(Math::powerOfTwo(0) == 1);
	CHECK(Math::powerOfTwo(1) == 1);
	CHECK(Math::powerOfTwo(2) == 2);
	CHECK(Math::powerOfTwo(3) == 4);
	CHECK(Math::powerOfTwo(4) == 4);
	CHECK(Math::powerOfTwo(5) == 8);
	CHECK(Math::powerOfTwo(6) == 8);
	CHECK(Math::powerOfTwo(7) == 8);
	CHECK(Math::powerOfTwo(8) == 8);
	CHECK(Math::powerOfTwo(9) == 16);
	CHECK(Math::powerOfTwo(15) == 16);
	CHECK(Math::powerOfTwo(16) == 16);
	CHECK(Math::powerOfTwo(17) == 32);
	CHECK(Math::powerOfTwo(32) == 32);
	CHECK(Math::powerOfTwo(64) == 64);
	CHECK(Math::powerOfTwo(255) == 256);
	CHECK(Math::powerOfTwo(256) == 256);
	CHECK(Math::powerOfTwo(512) == 512);
	CHECK(Math::powerOfTwo(1024) == 1024);
	CHECK(Math::powerOfTwo(2048) == 2048);
	CHECK(Math::powerOfTwo(4096) == 4096);
	CHECK(Math::powerOfTwo(8192) == 8192);
	CHECK(Math::powerOfTwo(0xffff) == 0x10000);
	CHECK(Math::powerOfTwo(0x10000) == 0x10000);
	CHECK(Math::powerOfTwo(0x80000000) == 0x80000000);
	// values > 0x80000000 don't work,
	//   result can't be represented in 32bit
}

TEST_CASE("Math::clip")
{
	CHECK((Math::clip<10, 20>(-6)) == 10);
	CHECK((Math::clip<10, 20>( 0)) == 10);
	CHECK((Math::clip<10, 20>( 9)) == 10);
	CHECK((Math::clip<10, 20>(10)) == 10);
	CHECK((Math::clip<10, 20>(11)) == 11);
	CHECK((Math::clip<10, 20>(14)) == 14);
	CHECK((Math::clip<10, 20>(19)) == 19);
	CHECK((Math::clip<10, 20>(20)) == 20);
	CHECK((Math::clip<10, 20>(21)) == 20);
	CHECK((Math::clip<10, 20>(99)) == 20);

	CHECK((Math::clip<10, 10>( 9)) == 10);
	CHECK((Math::clip<10, 10>(10)) == 10);
	CHECK((Math::clip<10, 10>(11)) == 10);

	CHECK((Math::clip<-10, 10>(-20)) == -10);
	CHECK((Math::clip<-10, 10>( -3)) ==  -3);
	CHECK((Math::clip<-10, 10>( 20)) ==  10);

	CHECK((Math::clip<-100, -10>(-200)) == -100);
	CHECK((Math::clip<-100, -10>( -53)) ==  -53);
	CHECK((Math::clip<-100, -10>( 200)) ==  -10);

	// ok, compiler error (invalid range)
	//CHECK((Math::clip<6, 3>(1)) == 1);
}

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

static unsigned classic_gcd(unsigned a, unsigned b)
{
	while (unsigned t = b % a) { b = a; a = t; }
	return a;
}
static void testGcd(unsigned a, unsigned b)
{
	unsigned expected = classic_gcd(a, b);
	CHECK(Math::gcd(a, b) == expected);
	CHECK(Math::gcd(b, a) == expected);
}
TEST_CASE("Math::gcd")
{
	testGcd(1, 1);
	testGcd(1, 2);
	testGcd(1, 1234500);
	testGcd(14, 1);
	testGcd(14, 2);
	testGcd(14, 7);
	testGcd(14, 21);
	testGcd(14, 291);
	testGcd(14, 6398);
	testGcd(1464, 6398);
	testGcd(1464, 6398);
	CHECK(Math::gcd(320, 1280) == 320);
	CHECK(Math::gcd(123 * 121972, 123 * 9710797) == 123);
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
