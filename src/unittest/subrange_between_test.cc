#include "catch.hpp"

#include "subrange_between.hh"

#include <vector>
#include <string>

TEST_CASE("subrange_between: integer vector", "[subrange_between]")
{
	std::vector v = {1, 3, 5, 7, 9, 11};

	SECTION("middle range") {
		auto sub = subrange_between(v, 4, 10);
		std::vector<int> result(sub.begin(), sub.end());
		CHECK(result == std::vector{5, 7, 9});
	}
	SECTION("full range") {
		auto sub = subrange_between(v, 0, 12);
		std::vector<int> result(sub.begin(), sub.end());
		CHECK(result == v);
	}
	SECTION("empty range") {
		auto sub = subrange_between(v, 6, 7);
		CHECK(sub.empty());
	}
	SECTION("range before vector") {
		auto sub = subrange_between(v, -10, 0);
		CHECK(sub.empty());
	}
	SECTION("single element") {
		auto sub = subrange_between(v, 5, 6);
		std::vector<int> result(sub.begin(), sub.end());
		CHECK(result == std::vector{5});
	}
}

struct Item {
	int key;
	std::string name;
};
TEST_CASE("subrange_between: structs with member pointer projection", "[subrange_between]")
{
	std::vector<Item> v = {{1, "a"}, {3, "b"}, {5, "c"}, {7, "d"}, {9, "e"}};
	auto sub = subrange_between(v, 4, 8, {}, &Item::key);

	std::vector<int> keys;
	for (auto& item : sub) keys.push_back(item.key);
	CHECK(keys == std::vector{5,7});
}

TEST_CASE("subrange_between: custom comparator", "[subrange_between]")
{
	std::vector v = {11, 9, 7, 5, 3, 1}; // descending order
	auto sub = subrange_between(v, 10, 4, std::greater{});

	std::vector<int> result(sub.begin(), sub.end());
	CHECK(result == std::vector{9, 7, 5});
}
