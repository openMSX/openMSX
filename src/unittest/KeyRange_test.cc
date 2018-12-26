#include "catch.hpp"
#include "KeyRange.hh"
#include "hash_map.hh"
#include "ranges.hh"
#include "string_view.hh"
#include <map>
#include <tuple>
#include <vector>

using namespace std;

template<typename RANGE, typename T>
static void check(const RANGE& range, const vector<T>& expected)
{
	REQUIRE(size_t(distance(range.begin(), range.end())) == expected.size());
	REQUIRE(equal(range.begin(), range.end(), expected.begin()));
}

template<typename RANGE, typename T>
static void check_unordered(const RANGE& range, const vector<T>& expected_)
{
	vector<T> expected = expected_;
	REQUIRE(size_t(distance(range.begin(), range.end())) == expected.size());
	vector<T> result(range.begin(), range.end());
	ranges::sort(result);
	ranges::sort(expected);
	REQUIRE(equal(result.begin(), result.end(), expected.begin()));
}

TEST_CASE("KeyRange") {
	SECTION("std::map") {
		map<int, int> m = {{1, 2}, {3, 4}, {5, 6}, {7, 8}};
		check(keys       (m), vector<int>{1, 3, 5, 7});
		check(values     (m), vector<int>{2, 4, 6, 8});
		check(elements<0>(m), vector<int>{1, 3, 5, 7});
		check(elements<1>(m), vector<int>{2, 4, 6, 8});
		//check(elements<2>(m), vector<int>{2, 4, 6, 8}); // compile error, ok
	}
	SECTION("std::vector<pair>") {
		vector<pair<int, int>> v = {{1, 2}, {3, 4}, {5, 6}, {7, 8}};
		check(keys       (v), vector<int>{1, 3, 5, 7});
		check(values     (v), vector<int>{2, 4, 6, 8});
		check(elements<0>(v), vector<int>{1, 3, 5, 7});
		check(elements<1>(v), vector<int>{2, 4, 6, 8});
	}
	SECTION("hash_map") {
		hash_map<string, int> m =
			{{"foo", 1}, {"bar", 2}, {"qux", 3},
			 {"baz", 4}, {"a",   5}, {"z",   6}};
		check_unordered(keys(m), vector<string_view>{
			"foo", "bar", "qux", "baz", "a", "z"});
		check_unordered(values(m), vector<int>{1, 2, 3, 4, 5, 6});
		check_unordered(elements<0>(m), vector<string_view>{
			"foo", "bar", "qux", "baz", "a", "z"});
		check_unordered(elements<1>(m), vector<int>{1, 2, 3, 4, 5, 6});
	}
	SECTION("std::vector<tuple>") {
		vector<tuple<int, char, double, string>> v = {
			make_tuple(1, 2, 1.2, "foo"),
			make_tuple(3, 4, 3.4, "bar"),
			make_tuple(5, 6, 5.6, "qux")
		};
		check(keys       (v), vector<int>{1, 3, 5});
		check(elements<0>(v), vector<int>{1, 3, 5});
		check(values     (v), vector<char>{2, 4, 6});
		check(elements<1>(v), vector<char>{2, 4, 6});
		check(elements<2>(v), vector<double>{1.2, 3.4, 5.6});
		check(elements<3>(v), vector<string>{"foo", "bar", "qux"});
	}
}
