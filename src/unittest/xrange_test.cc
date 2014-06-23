#include "catch.hpp"
#include "xrange.hh"
#include <algorithm>
#include <vector>

template<typename T, typename RANGE>
static void test(const RANGE& range, const std::vector<T>& v)
{
	REQUIRE(std::distance(range.begin(), range.end()) == v.size());
	REQUIRE(std::equal(range.begin(), range.end(), v.begin()));
}

TEST_CASE("xrange")
{
	test<int>(xrange( 0), {});
	test<int>(xrange( 1), {0});
	test<int>(xrange( 2), {0, 1});
	test<int>(xrange(10), {0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

	test<int>(xrange(5,  5), {});
	test<int>(xrange(5,  6), {5});
	test<int>(xrange(5,  7), {5, 6});
	test<int>(xrange(5, 10), {5, 6, 7, 8, 9});

	test<unsigned long long>(xrange(0ULL), {});
	test<unsigned long long>(xrange(1ULL), {0ULL});
	test<unsigned long long>(xrange(2ULL), {0ULL, 1ULL});
	test<unsigned long long>(
		xrange(10ULL),
		{0ULL, 1ULL, 2ULL, 3ULL, 4ULL, 5ULL, 6ULL, 7ULL, 8ULL, 9ULL});

	test<unsigned long long>(xrange(5ULL,  5ULL), {});
	test<unsigned long long>(xrange(5ULL,  6ULL), {5ULL});
	test<unsigned long long>(xrange(5ULL,  7ULL), {5ULL, 6ULL});
	test<unsigned long long>(
		xrange(5ULL, 10ULL),
		{5ULL, 6ULL, 7ULL, 8ULL, 9ULL});
	test<unsigned long long>(
		xrange(0x112233445566ULL, 0x112233445568ULL),
		{0x112233445566ULL, 0x112233445567ULL});

	// undefined behavior
	//test<int>(xrange(10, 5), {});
}
