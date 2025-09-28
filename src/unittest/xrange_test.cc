#include "catch.hpp"
#include "xrange.hh"

#include <algorithm>
#include <type_traits>
#include <vector>

template<typename T, typename RANGE>
static void test(const RANGE& range, const std::vector<T>& v)
{
	CHECK(std::equal(range.begin(), range.end(), v.begin(), v.end()));
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

TEST_CASE("xrange, random-access")
{
	auto r = xrange(20, 45);
	static_assert(std::is_same_v<decltype(r.begin())::iterator_category,
	                             std::random_access_iterator_tag>);

	auto b = r.begin();  REQUIRE(*b == 20);
	auto m = b + 10;

	auto i1 = b; auto j1 = i1++;  CHECK(*i1 == 21);  CHECK(*j1 == 20);  CHECK(i1 != j1);
	auto i2 = b; auto j2 = ++i2;  CHECK(*i2 == 21);  CHECK(*j2 == 21);  CHECK(i2 == j2);
	auto i3 = m; auto j3 = i3--;  CHECK(*i3 == 29);  CHECK(*j3 == 30);  CHECK(i3 != j3);
	auto i4 = m; auto j4 = --i4;  CHECK(*i4 == 29);  CHECK(*j4 == 29);  CHECK(i4 == j4);

	auto i5 = b; i5 += 7;  CHECK(*i5 == 27);
	auto i6 = m; i6 -= 7;  CHECK(*i6 == 23);

	auto i7 = b + 4;  CHECK(*i7 == 24);
	auto i8 = 6 + m;  CHECK(*i8 == 36);
	auto i9 = m - 5;  CHECK(*i9 == 25);

	CHECK((m - b) == 10);
	CHECK(m[4] == 34);

	CHECK(b <  m);  CHECK(!(b <  b));
	CHECK(b <= m);  CHECK(  b <= b );
	CHECK(m >  b);  CHECK(!(b >  b));
	CHECK(m >= b);  CHECK(  b >= b );
}
