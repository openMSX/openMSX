#include "catch.hpp"
#include "endian.hh"

using namespace Endian;

union T16 {
	B16 be;
	L16 le;
};

union T32 {
	B32 be;
	L32 le;
};

TEST_CASE("endian: byteswap")
{
	CHECK(byteswap16(0x1122) == 0x2211);
	CHECK(byteswap32(0x11223344) == 0x44332211);
	CHECK(byteswap64(0x1122334455667788) == 0x8877665544332211);

	CHECK(byteswap(uint16_t(0x1234)) == 0x3412);
	CHECK(byteswap(uint32_t(0x12345678)) == 0x78563412);
	CHECK(byteswap(uint64_t(0x123456789abcdef0)) == 0xf0debc9a78563412);

	ByteSwap swapper;
	CHECK(swapper(uint16_t(0xaabb)) == 0xbbaa);
	CHECK(swapper(uint32_t(0xaabbccdd)) == 0xddccbbaa);
	CHECK(swapper(uint64_t(0xaabbccddeeff0011)) == 0x1100ffeeddccbbaa);
}

// TODO better coverage for aligned vs unaligned versions of the functions
TEST_CASE("endian: 16-bit")
{
	T16 t;
	REQUIRE(sizeof(t) == 2);

	t.le = 0x1234;
	CHECK(t.le == 0x1234);
	CHECK(t.be == 0x3412);
	CHECK(read_UA_L16(&t) == 0x1234);
	CHECK(read_UA_B16(&t) == 0x3412);

	t.be = 0x1234;
	CHECK(t.le == 0x3412);
	CHECK(t.be == 0x1234);
	CHECK(read_UA_L16(&t) == 0x3412);
	CHECK(read_UA_B16(&t) == 0x1234);

	write_UA_L16(&t, 0xaabb);
	CHECK(t.le == 0xaabb);
	CHECK(t.be == 0xbbaa);
	CHECK(read_UA_L16(&t) == 0xaabb);
	CHECK(read_UA_B16(&t) == 0xbbaa);

	write_UA_B16(&t, 0xaabb);
	CHECK(t.le == 0xbbaa);
	CHECK(t.be == 0xaabb);
	CHECK(read_UA_L16(&t) == 0xbbaa);
	CHECK(read_UA_B16(&t) == 0xaabb);
}

TEST_CASE("endian: 32-bit")
{
	T32 t;
	REQUIRE(sizeof(t) == 4);

	t.le = 0x12345678;
	CHECK(t.le == 0x12345678);
	CHECK(t.be == 0x78563412);
	CHECK(read_UA_L32(&t) == 0x12345678);
	CHECK(read_UA_B32(&t) == 0x78563412);

	t.be = 0x12345678;
	CHECK(t.le == 0x78563412);
	CHECK(t.be == 0x12345678);
	CHECK(read_UA_L32(&t) == 0x78563412);
	CHECK(read_UA_B32(&t) == 0x12345678);

	write_UA_L32(&t, 0xaabbccdd);
	CHECK(t.le == 0xaabbccdd);
	CHECK(t.be == 0xddccbbaa);
	CHECK(read_UA_L32(&t) == 0xaabbccdd);
	CHECK(read_UA_B32(&t) == 0xddccbbaa);

	write_UA_B32(&t, 0xaabbccdd);
	CHECK(t.le == 0xddccbbaa);
	CHECK(t.be == 0xaabbccdd);
	CHECK(read_UA_L32(&t) == 0xddccbbaa);
	CHECK(read_UA_B32(&t) == 0xaabbccdd);
}


#if 0

// Small functions to inspect code quality

uint16_t testSwap16(uint16_t x) { return byteswap16(x); }
uint16_t testSwap16()           { return byteswap16(0x1234); }
uint32_t testSwap32(uint32_t x) { return byteswap32(x); }
uint32_t testSwap32()           { return byteswap32(0x12345678); }

void test1(T16& t, uint16_t x) { t.le = x; }
void test2(T16& t, uint16_t x) { t.be = x; }
uint16_t test3(T16& t) { return t.le; }
uint16_t test4(T16& t) { return t.be; }

void testA(uint16_t& s, uint16_t x) { write_UA_L16(&s, x); }
void testB(uint16_t& s, uint16_t x) { write_UA_B16(&s, x); }
uint16_t testC(uint16_t& s) { return read_UA_L16(&s); }
uint16_t testD(uint16_t& s) { return read_UA_B16(&s); }


void test1(T32& t, uint32_t x) { t.le = x; }
void test2(T32& t, uint32_t x) { t.be = x; }
uint32_t test3(T32& t) { return t.le; }
uint32_t test4(T32& t) { return t.be; }

void testA(uint32_t& s, uint32_t x) { write_UA_L32(&s, x); }
void testB(uint32_t& s, uint32_t x) { write_UA_B32(&s, x); }
uint32_t testC(uint32_t& s) { return read_UA_L32(&s); }
uint32_t testD(uint32_t& s) { return read_UA_B32(&s); }

#endif
