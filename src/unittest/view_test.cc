#include "catch.hpp"
#include "view.hh"

#include "hash_map.hh"
#include "ranges.hh"
#include "stl.hh"
#include "xrange.hh"
#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using std::vector;
using namespace view;

static vector<int> getVector(int n)
{
	return to_vector(xrange(n));
}

TEST_CASE("view::drop random-access-range")
{
	SECTION("empty") {
		vector<int> v;
		CHECK(to_vector(drop(v, 0)) == vector<int>{});
		CHECK(to_vector(drop(v, 3)) == vector<int>{});
	}
	SECTION("non-empty") {
		vector<int> v = {1, 2, 3, 4, 5};
		CHECK(to_vector(drop(v, 0)) == vector<int>{1, 2, 3, 4, 5});
		CHECK(to_vector(drop(v, 1)) == vector<int>{2, 3, 4, 5});
		CHECK(to_vector(drop(v, 2)) == vector<int>{3, 4, 5});
		CHECK(to_vector(drop(v, 3)) == vector<int>{4, 5});
		CHECK(to_vector(drop(v, 4)) == vector<int>{5});
		CHECK(to_vector(drop(v, 5)) == vector<int>{});
		CHECK(to_vector(drop(v, 6)) == vector<int>{});
		CHECK(to_vector(drop(v, 7)) == vector<int>{});
	}
	SECTION("r-value") {
		CHECK(to_vector(drop(getVector(6), 3)) == vector<int>{3, 4, 5});
	}
}

TEST_CASE("view::drop non-random-access-range")
{
	SECTION("empty") {
		std::list<int> l;
		CHECK(to_vector(drop(l, 0)) == vector<int>{});
		CHECK(to_vector(drop(l, 3)) == vector<int>{});
	}
	SECTION("non-empty") {
		std::list<int> l = {1, 2, 3, 4, 5};
		CHECK(to_vector(drop(l, 0)) == vector<int>{1, 2, 3, 4, 5});
		CHECK(to_vector(drop(l, 1)) == vector<int>{2, 3, 4, 5});
		CHECK(to_vector(drop(l, 2)) == vector<int>{3, 4, 5});
		CHECK(to_vector(drop(l, 3)) == vector<int>{4, 5});
		CHECK(to_vector(drop(l, 4)) == vector<int>{5});
		CHECK(to_vector(drop(l, 5)) == vector<int>{});
		CHECK(to_vector(drop(l, 6)) == vector<int>{});
		CHECK(to_vector(drop(l, 7)) == vector<int>{});
	}
}

TEST_CASE("view::drop capture")
{
	REQUIRE(sizeof(vector<int>*) != sizeof(vector<int>));
	SECTION("l-value") {
		vector<int> v = {0, 1, 2, 3};
		auto d = drop(v, 1);
		// 'd' stores a reference to 'v'
		CHECK(sizeof(d) == (sizeof(vector<int>*) + sizeof(size_t)));
	}
	SECTION("r-value") {
		auto d = drop(getVector(4), 1);
		// 'd' stores a vector by value
		CHECK(sizeof(d) == (sizeof(vector<int>) + sizeof(size_t)));
	}
}


TEST_CASE("view::drop_back random-access-range")
{
	SECTION("empty") {
		vector<int> v;
		CHECK(to_vector(drop_back(v, 0)) == vector<int>{});
		CHECK(to_vector(drop_back(v, 3)) == vector<int>{});
	}
	SECTION("non-empty") {
		vector<int> v = {1, 2, 3, 4, 5};
		CHECK(to_vector(drop_back(v, 0)) == vector<int>{1, 2, 3, 4, 5});
		CHECK(to_vector(drop_back(v, 1)) == vector<int>{1, 2, 3, 4});
		CHECK(to_vector(drop_back(v, 2)) == vector<int>{1, 2, 3});
		CHECK(to_vector(drop_back(v, 3)) == vector<int>{1, 2});
		CHECK(to_vector(drop_back(v, 4)) == vector<int>{1});
		CHECK(to_vector(drop_back(v, 5)) == vector<int>{});
		CHECK(to_vector(drop_back(v, 6)) == vector<int>{});
		CHECK(to_vector(drop_back(v, 7)) == vector<int>{});
	}
	SECTION("r-value") {
		CHECK(to_vector(drop_back(getVector(6), 3)) == vector<int>{0, 1, 2});
	}
}

TEST_CASE("view::drop_back non-random-access-range")
{
	SECTION("empty") {
		std::list<int> l;
		CHECK(to_vector(drop_back(l, 0)) == vector<int>{});
		CHECK(to_vector(drop_back(l, 3)) == vector<int>{});
	}
	SECTION("non-empty") {
		std::list<int> l = {1, 2, 3, 4, 5};
		CHECK(to_vector(drop_back(l, 0)) == vector<int>{1, 2, 3, 4, 5});
		CHECK(to_vector(drop_back(l, 1)) == vector<int>{1, 2, 3, 4});
		CHECK(to_vector(drop_back(l, 2)) == vector<int>{1, 2, 3});
		CHECK(to_vector(drop_back(l, 3)) == vector<int>{1, 2});
		CHECK(to_vector(drop_back(l, 4)) == vector<int>{1});
		CHECK(to_vector(drop_back(l, 5)) == vector<int>{});
		CHECK(to_vector(drop_back(l, 6)) == vector<int>{});
		CHECK(to_vector(drop_back(l, 7)) == vector<int>{});
	}
}


TEST_CASE("view::reverse")
{
	vector<int> out;
	SECTION("l-value") {
		vector<int> in = {1, 2, 3, 4};
		for (auto& e : reverse(in)) out.push_back(e);
		CHECK(out == vector<int>{4, 3, 2, 1});
	}
	SECTION("r-value") {
		for (auto& e : reverse(getVector(3))) out.push_back(e);
		CHECK(out == vector<int>{2, 1, 0});
	}
	SECTION("2 x reverse") {
		for (auto& e : reverse(reverse(getVector(4)))) out.push_back(e);
		CHECK(out == vector<int>{0, 1, 2, 3});
	}
}

TEST_CASE("view::transform")
{
	auto square = [](auto& x) { return x * x; };
	size_t i = 1;
	auto plus_i = [&](auto& x) { return int(x + i); };

	SECTION("l-value") {
		vector<int> v = {1, 2, 3, 4};
		CHECK(to_vector(transform(v, square)) == vector<int>{1, 4, 9, 16});
	}
	SECTION("r-value") {
		i = 10;
		CHECK(to_vector(transform(getVector(4), plus_i)) == vector<int>{10, 11, 12, 13});
	}
}

/*
No longer true since we use semiregular_t<> in TransformIterator
TEST_CASE("view::transform sizes")
{
	auto square = [](auto& x) { return x * x; };
	size_t i = 1;
	auto plus_i = [&](auto& x) { return int(x + i); };

	vector<int> v = {1, 2, 3, 4};

	SECTION("l-value, stateless") {
		auto vw = transform(v, square);
		CHECK(sizeof(vw)         == sizeof(std::vector<int>*));
		CHECK(sizeof(vw.begin()) == sizeof(std::vector<int>::iterator));
	}
	SECTION("l-value, state") {
		auto vw = transform(v, plus_i);
		CHECK(sizeof(vw)         == (sizeof(size_t&) + sizeof(std::vector<int>*)));
		CHECK(sizeof(vw.begin()) == (sizeof(size_t&) + sizeof(std::vector<int>::iterator)));
	}
	SECTION("r-value, stateless") {
		auto vw = transform(getVector(3), square);
		CHECK(sizeof(vw)         == sizeof(std::vector<int>));
		CHECK(sizeof(vw.begin()) == sizeof(std::vector<int>::iterator));
	}
	SECTION("r-value, state") {
		auto vw = transform(getVector(3), plus_i);
		CHECK(sizeof(vw)         == (sizeof(size_t&) + sizeof(std::vector<int>)));
		CHECK(sizeof(vw.begin()) == (sizeof(size_t&) + sizeof(std::vector<int>::iterator)));
	}
}*/

template<typename RANGE, typename T>
static void check(const RANGE& range, const vector<T>& expected)
{
	CHECK(equal(range.begin(), range.end(), expected.begin(), expected.end()));
}

template<typename RANGE, typename T>
static void check_unordered(const RANGE& range, const vector<T>& expected_)
{
	auto result = to_vector<T>(range);
	auto expected = expected_;
	ranges::sort(result);
	ranges::sort(expected);
	CHECK(result == expected);
}

TEST_CASE("view::keys, view::values") {
	SECTION("std::map") {
		std::map<int, int> m = {{1, 2}, {3, 4}, {5, 6}, {7, 8}};
		check(keys  (m), vector<int>{1, 3, 5, 7});
		check(values(m), vector<int>{2, 4, 6, 8});
	}
	SECTION("std::vector<std::pair>") {
		vector<std::pair<int, int>> v = {{1, 2}, {3, 4}, {5, 6}, {7, 8}};
		check(keys  (v), vector<int>{1, 3, 5, 7});
		check(values(v), vector<int>{2, 4, 6, 8});
	}
	SECTION("hash_map") {
		hash_map<std::string, int> m =
		{{"foo", 1}, {"bar", 2}, {"qux", 3},
			{"baz", 4}, {"a",   5}, {"z",   6}};
		check_unordered(keys(m), vector<std::string>{
				"foo", "bar", "qux", "baz", "a", "z"});
		check_unordered(values(m), vector<int>{1, 2, 3, 4, 5, 6});
	}
	SECTION("std::vector<std::tuple>") {
		vector<std::tuple<int, char, double, std::string>> v = {
			std::tuple(1, 2, 1.2, "foo"),
			std::tuple(3, 4, 3.4, "bar"),
			std::tuple(5, 6, 5.6, "qux")
		};
		check(keys  (v), vector<int>{1, 3, 5});
		check(values(v), vector<char>{2, 4, 6});
	}
}

struct F {
	int i;
	F(int i_) : i(i_) {}
	operator int() const { return i; }
	bool isOdd() const { return i & 1; }
};

TEST_CASE("view::filter") {
	vector v = {1, 2, 3, 4, 5, 6, 7, 8, 9};

	SECTION("removed front") {
		check(view::filter(v, [](int i) { return i > 5; }),
		      vector{6, 7, 8, 9});
	}
	SECTION("removed back") {
		check(view::filter(v, [](int i) { return i < 5; }),
		      vector{1, 2, 3, 4});
	}
	SECTION("keep front and back") {
		check(view::filter(v, [](int i) { return i & 1; }),
		      vector{1, 3, 5, 7, 9});
	}
	SECTION("remove front and back") {
		check(view::filter(v, [](int i) { return (i & 1) == 0; }),
		      vector{2, 4, 6, 8});
	}

	SECTION("projection") {
		vector<F> f = {1, 2, 3, 4, 5};
		auto view = view::filter(f, &F::isOdd);
		auto it = view.begin();
		auto et = view.end();
		REQUIRE(it != et);
		CHECK(*it == 1);
		++it;
		REQUIRE(it != et);
		CHECK(*it == 3);
		it++;
		REQUIRE(it != et);
		CHECK(*it == 5);
		++it;
		REQUIRE(it == et);
	}
}
