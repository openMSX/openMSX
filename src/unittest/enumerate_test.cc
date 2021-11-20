#include "catch.hpp"
#include "enumerate.hh"
#include <type_traits>
#include <vector>

TEST_CASE("enumerate: basic")
{
	char in[3] = {'a', 'd', 'z'};
	std::vector<size_t> out1;
	std::vector<char> out2;

	for (auto [i, e] : enumerate(in)) {
		static_assert(std::is_same_v<decltype(i), const size_t&>);
		static_assert(std::is_same_v<decltype(e), char&>);
		out1.push_back(i);
		out2.push_back(e);
	}

	CHECK(out1 == std::vector<size_t>{0, 1, 2});
	CHECK(out2 == std::vector{'a', 'd', 'z'});
}

TEST_CASE("enumerate: transform")
{
	std::vector in{3, 9, 11};
	std::vector<size_t> out;

	for (auto [i, e] : enumerate(in)) {
		static_assert(std::is_same_v<decltype(i), const size_t&>);
		static_assert(std::is_same_v<decltype(e), int&>);
		out.push_back(i);
		e = 2 * e + i;
	}

	CHECK(in == std::vector{6, 19, 24});
	CHECK(out == std::vector<size_t>{0, 1, 2});
}
