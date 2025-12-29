#include "catch.hpp"

#include "find_closest.h"

#include <array>
#include <chrono>
#include <functional>
#include <vector>

TEST_CASE("find_closest_basic_integer_cases")
{
	std::vector values = {2, 5, 9, 13, 21};

	SECTION("exact match returns zero distance")
	{
		auto [it, dist] = find_closest(values, 9);
		CHECK(it == values.begin() + 2);
		CHECK(dist == 0);
	}

	SECTION("before first element")
	{
		auto [it, dist] = find_closest(values, -3);
		CHECK(it == values.begin());
		CHECK(dist == 5);
	}

	SECTION("after last element")
	{
		auto [it, dist] = find_closest(values, 99);
		CHECK(it == values.end() - 1);
		CHECK(dist == 78);
	}

	SECTION("tie prefers earlier element")
	{
		auto [it, dist] = find_closest(values, 7);
		CHECK(it == values.begin() + 1); // 5 beats 9 when distances tie
		CHECK(dist == 2);
	}

	SECTION("middle selects nearest neighbour")
	{
		auto [it, dist] = find_closest(values, 12);
		CHECK(it == values.begin() + 3); // 13 is closest to 12
		CHECK(dist == 1);
	}
}

TEST_CASE("find_closest_handles_empty_range")
{
	std::vector<int> empty;
	auto [it, _] = find_closest(empty, 42);
	CHECK(it == empty.end());
}

TEST_CASE("find_closest_with_time_points_and_projection")
{
	struct Sample {
		int id;
		std::chrono::steady_clock::time_point time;
	};

	using namespace std::chrono_literals;
	std::array samples = {
	        Sample{1, std::chrono::steady_clock::time_point{0ms}},
	        Sample{2, std::chrono::steady_clock::time_point{10ms}},
	        Sample{3, std::chrono::steady_clock::time_point{30ms}}
	};
	auto [it, dist] = find_closest(
		samples, std::chrono::steady_clock::time_point{18ms}, {}, &Sample::time);
	REQUIRE(it != samples.end());
	CHECK(it->id == 2);
	CHECK(dist == 8ms);
	static_assert(std::is_same_v<decltype(dist), std::chrono::steady_clock::duration>);
}
