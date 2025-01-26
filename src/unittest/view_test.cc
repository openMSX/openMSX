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

using std::vector;
using namespace view;

static vector<int> getVector(int n)
{
	return to_vector(xrange(n));
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
