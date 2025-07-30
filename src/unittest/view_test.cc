#include "catch.hpp"
#include "view.hh"

#include "hash_map.hh"
#include "stl.hh"
#include "xrange.hh"
#include "StringOp.hh"

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using namespace view;

static std::vector<int> getVector(int n)
{
	return to_vector(xrange(n));
}

TEST_CASE("view::drop_back random-access-range")
{
	SECTION("empty") {
		std::vector<int> v;
		CHECK(to_vector(drop_back(v, 0)) == std::vector<int>{});
		CHECK(to_vector(drop_back(v, 3)) == std::vector<int>{});
	}
	SECTION("non-empty") {
		std::vector<int> v = {1, 2, 3, 4, 5};
		CHECK(to_vector(drop_back(v, 0)) == std::vector<int>{1, 2, 3, 4, 5});
		CHECK(to_vector(drop_back(v, 1)) == std::vector<int>{1, 2, 3, 4});
		CHECK(to_vector(drop_back(v, 2)) == std::vector<int>{1, 2, 3});
		CHECK(to_vector(drop_back(v, 3)) == std::vector<int>{1, 2});
		CHECK(to_vector(drop_back(v, 4)) == std::vector<int>{1});
		CHECK(to_vector(drop_back(v, 5)) == std::vector<int>{});
		CHECK(to_vector(drop_back(v, 6)) == std::vector<int>{});
		CHECK(to_vector(drop_back(v, 7)) == std::vector<int>{});
	}
	SECTION("r-value") {
		CHECK(to_vector(drop_back(getVector(6), 3)) == std::vector<int>{0, 1, 2});
	}
}

TEST_CASE("view::drop_back non-random-access-range")
{
	SECTION("empty") {
		std::list<int> l;
		CHECK(to_vector(drop_back(l, 0)) == std::vector<int>{});
		CHECK(to_vector(drop_back(l, 3)) == std::vector<int>{});
	}
	SECTION("non-empty") {
		std::list<int> l = {1, 2, 3, 4, 5};
		CHECK(to_vector(drop_back(l, 0)) == std::vector<int>{1, 2, 3, 4, 5});
		CHECK(to_vector(drop_back(l, 1)) == std::vector<int>{1, 2, 3, 4});
		CHECK(to_vector(drop_back(l, 2)) == std::vector<int>{1, 2, 3});
		CHECK(to_vector(drop_back(l, 3)) == std::vector<int>{1, 2});
		CHECK(to_vector(drop_back(l, 4)) == std::vector<int>{1});
		CHECK(to_vector(drop_back(l, 5)) == std::vector<int>{});
		CHECK(to_vector(drop_back(l, 6)) == std::vector<int>{});
		CHECK(to_vector(drop_back(l, 7)) == std::vector<int>{});
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
