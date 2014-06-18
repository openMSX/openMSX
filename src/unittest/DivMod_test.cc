#include "catch.hpp"
#include "DivModByConst.hh"
#include "DivModBySame.hh"

using namespace openmsx;

template<uint32_t DIVISOR>
static void test(const DivModByConst<DIVISOR>& c, const DivModBySame& s,
                 uint64_t dividend)
{
	uint64_t rd = dividend / DIVISOR;
	if (uint32_t(rd) == rd) {
		CHECK(c.div(dividend) == (dividend / DIVISOR));
		CHECK(s.div(dividend) == (dividend / DIVISOR));
		CHECK(c.mod(dividend) == (dividend % DIVISOR));
		CHECK(s.mod(dividend) == (dividend % DIVISOR));
	}
}

template<uint32_t DIVISOR>
static void test()
{
	DivModByConst<DIVISOR> c;
	DivModBySame s; s.setDivisor(DIVISOR);

	for (uint64_t dividend : {0ull, 1ull, 2ull, 3ull, 4ull, 5ull, 7ull,
			          100ull, 10015ull, 12410015ull, 0x1234567890ull,
				  0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull,
				  0xFFFFFFFF00000000ull, 0xFFFFFFFFFFFFFFFFull}) {
		test(c, s, dividend);
	}
}

TEST_CASE("DivModByConst, DivModBySame")
{
	test<1>();
	test<2>();
	test<3>();
	test<4>();
	test<5>();
	test<7>();
	test<10>();
	test<128>();
	test<1000>();
	test<90000>();
	test<90017>();
}
