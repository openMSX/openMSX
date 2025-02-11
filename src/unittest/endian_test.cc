#include "catch.hpp"
#include "endian.hh"

#include <array>

using namespace Endian;

TEST_CASE("endian: byteswap")
{
	ByteSwap swapper;
	CHECK(swapper(uint16_t(0xaabb)) == 0xbbaa);
	CHECK(swapper(uint32_t(0xaabbccdd)) == 0xddccbbaa);
	CHECK(swapper(uint64_t(0xaabbccddeeff0011)) == 0x1100ffeeddccbbaa);
}

// TODO better coverage for aligned vs unaligned versions of the functions
TEST_CASE("endian: 16-bit")
{
	REQUIRE(sizeof(B16) == 2);
	REQUIRE(sizeof(L16) == 2);

	L16 l(0x1234);
	CHECK(read_UA_L16(&l) == 0x1234);
	CHECK(read_UA_B16(&l) == 0x3412);

	B16 b(0x1234);
	CHECK(read_UA_L16(&b) == 0x3412);
	CHECK(read_UA_B16(&b) == 0x1234);

	std::array<uint8_t, 2> buf;
	write_UA_L16(buf.data(), 0xaabb);
	CHECK(read_UA_L16(buf.data()) == 0xaabb);
	CHECK(read_UA_B16(buf.data()) == 0xbbaa);

	write_UA_B16(buf.data(), 0xaabb);
	CHECK(read_UA_L16(buf.data()) == 0xbbaa);
	CHECK(read_UA_B16(buf.data()) == 0xaabb);
}

TEST_CASE("endian: 32-bit")
{
	REQUIRE(sizeof(B32) == 4);
	REQUIRE(sizeof(L32) == 4);

	L32 l(0x12345678);
	CHECK(read_UA_L32(&l) == 0x12345678);
	CHECK(read_UA_B32(&l) == 0x78563412);

	B32 b(0x12345678);
	CHECK(read_UA_L32(&b) == 0x78563412);
	CHECK(read_UA_B32(&b) == 0x12345678);

	std::array<uint8_t, 4> buf;
	write_UA_L32(buf.data(), 0xaabbccdd);
	CHECK(read_UA_L32(buf.data()) == 0xaabbccdd);
	CHECK(read_UA_B32(buf.data()) == 0xddccbbaa);

	write_UA_B32(buf.data(), 0xaabbccdd);
	CHECK(read_UA_L32(buf.data()) == 0xddccbbaa);
	CHECK(read_UA_B32(buf.data()) == 0xaabbccdd);
}


#if 0

// Small functions to inspect code quality

uint16_t testSwap16(uint16_t x) { return std::byteswap(x); }
uint16_t testSwap16()           { return std::byteswap(0x1234); }
uint32_t testSwap32(uint32_t x) { return std::byteswap(x); }
uint32_t testSwap32()           { return std::byteswap(0x12345678); }

void testA(uint16_t& s, uint16_t x) { write_UA_L16(&s, x); }
void testB(uint16_t& s, uint16_t x) { write_UA_B16(&s, x); }
uint16_t testC(uint16_t& s) { return read_UA_L16(&s); }
uint16_t testD(uint16_t& s) { return read_UA_B16(&s); }

void testA(uint32_t& s, uint32_t x) { write_UA_L32(&s, x); }
void testB(uint32_t& s, uint32_t x) { write_UA_B32(&s, x); }
uint32_t testC(uint32_t& s) { return read_UA_L32(&s); }
uint32_t testD(uint32_t& s) { return read_UA_B32(&s); }

#endif
