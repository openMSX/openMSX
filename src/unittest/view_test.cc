#include "catch.hpp"
#include "view.hh"

#include "hash_map.hh"
#include "ranges.hh"
#include "stl.hh"
#include "xrange.hh"
#include "StringOp.hh"

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
		for (const auto& e : reverse(in)) out.push_back(e);
		CHECK(out == vector<int>{4, 3, 2, 1});
	}
	SECTION("r-value") {
		for (const auto& e : reverse(getVector(3))) out.push_back(e);
		CHECK(out == vector<int>{2, 1, 0});
	}
	SECTION("2 x reverse") {
		for (const auto& e : reverse(reverse(getVector(4)))) out.push_back(e);
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
	CHECK(ranges::equal(range, expected));
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
		hash_map<std::string, int> m = {
			{"foo", 1}, {"bar", 2}, {"qux", 3},
			{"baz", 4}, {"a",   5}, {"z",   6}
		};
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
	explicit(false) F(int i_) : i(i_) {}
	explicit(false) operator int() const { return i; }
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

TEST_CASE("view::take") {
	SECTION("n") {
		vector v = {1, 2, 3, 4};
		check(view::take(v, 0), vector<int>{});
		check(view::take(v, 1), vector{1});
		check(view::take(v, 2), vector{1, 2});
		check(view::take(v, 3), vector{1, 2, 3});
		check(view::take(v, 4), vector{1, 2, 3, 4});
		check(view::take(v, 5), vector{1, 2, 3, 4});
		check(view::take(v, 6), vector{1, 2, 3, 4});
	}
	SECTION("split_view") {
		std::string_view str = "abc  def\t \tghi    jkl  mno  pqr";
		auto v = view::take(StringOp::split_view<StringOp::EmptyParts::REMOVE>(str, " \t"), 3);

		auto it = v.begin();
		auto et = v.end();
		REQUIRE(it != et);
		CHECK(*it == "abc");

		++it;
		REQUIRE(it != et);
		CHECK(*it == "def");

		++it;
		REQUIRE(it != et);
		CHECK(*it == "ghi");

		++it;
		REQUIRE(it == et);
	}
}

template<typename In1, typename In2, typename Expected>
void test_zip(const In1& in1, const In2& in2, const Expected& expected)
{
	Expected result;
	for (const auto& t : view::zip(in1, in2)) {
		result.push_back(t);
	}
	CHECK(result == expected);
}

template<typename In1, typename In2, typename In3, typename Expected>
void test_zip(const In1& in1, const In2& in2, const In3& in3, const Expected& expected)
{
	Expected result;
	for (const auto& t : view::zip(in1, in2, in3)) {
		result.push_back(t);
	}
	CHECK(result == expected);
}

TEST_CASE("view::zip")
{
	std::vector v4 = {1, 2, 3, 4};
	std::array a3 = {'a', 'b', 'c'};
	std::list l4 = {1.2, 2.3, 3.4, 4.5};

	test_zip(v4, a3, std::vector<std::tuple<int, char>>{{1, 'a'}, {2, 'b'}, {3, 'c'}});
	test_zip(a3, v4, std::vector<std::tuple<char, int>>{{'a', 1}, {'b', 2}, {'c', 3}});

	test_zip(v4, l4, std::vector<std::tuple<int, double>>{{1, 1.2}, {2, 2.3}, {3, 3.4}, {4, 4.5}});
	test_zip(l4, v4, std::vector<std::tuple<double, int>>{{1.2, 1}, {2.3, 2}, {3.4, 3}, {4.5, 4}});

	test_zip(a3, l4, std::vector<std::tuple<char, double>>{{'a', 1.2}, {'b', 2.3}, {'c', 3.4}});
	test_zip(l4, a3, std::vector<std::tuple<double, char>>{{1.2, 'a'}, {2.3, 'b'}, {3.4, 'c'}});

	test_zip(v4, a3, l4, std::vector<std::tuple<int, char, double>>{{1, 'a', 1.2}, {2, 'b', 2.3}, {3, 'c', 3.4}});

	for (auto [x, y] : zip(v4, a3)) {
		x = 0;
		++y;
	}
	CHECK(v4 == std::vector{0, 0, 0, 4});
	CHECK(a3 == std::array{'b', 'c', 'd'});
}

template<typename In1, typename In2, typename Expected>
void test_zip_equal(const In1& in1, const In2& in2, const Expected& expected)
{
	Expected result;
	for (const auto& t : view::zip_equal(in1, in2)) {
		result.push_back(t);
	}
	CHECK(result == expected);
}

template<typename In1, typename In2, typename In3, typename Expected>
void test_zip_equal(const In1& in1, const In2& in2, const In3& in3, const Expected& expected)
{
	Expected result;
	for (const auto& t : view::zip_equal(in1, in2, in3)) {
		result.push_back(t);
	}
	CHECK(result == expected);
}

TEST_CASE("view::zip_equal")
{
	std::vector v = {1, 2, 3};
	std::array a = {'a', 'b', 'c'};
	std::list l = {1.2, 2.3, 3.4};

	test_zip_equal(v, a, std::vector<std::tuple<int, char>>{{1, 'a'}, {2, 'b'}, {3, 'c'}});
	test_zip_equal(v, l, std::vector<std::tuple<int, double>>{{1, 1.2}, {2, 2.3}, {3, 3.4}});
	test_zip_equal(a, l, std::vector<std::tuple<char, double>>{{'a', 1.2}, {'b', 2.3}, {'c', 3.4}});

	test_zip_equal(v, a, l, std::vector<std::tuple<int, char, double>>{{1, 'a', 1.2}, {2, 'b', 2.3}, {3, 'c', 3.4}});

	for (auto [x, y, z] : zip_equal(v, l, a)) {
		x = 0;
		y *= 2.0;
		z += 2;
	}
	CHECK(v == std::vector{0, 0, 0});
	CHECK(a == std::array{'c', 'd', 'e'});
	CHECK(l == std::list{2.4, 4.6, 6.8});
}
