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

	for (uint64_t dividend : {0ULL, 1ULL, 2ULL, 3ULL, 4ULL, 5ULL, 7ULL,
			          100ULL, 10015ULL, 12410015ULL, 0x1234567890ULL,
				  0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL,
				  0xFFFFFFFF00000000ULL, 0xFFFFFFFFFFFFFFFFULL}) {
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
