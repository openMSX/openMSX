#include "catch.hpp"
#include "stl.hh"

#include <list>
#include <ranges>

struct S {
	S()                        { ++default_constructed; }
	S(const S&)                { ++copy_constructed; }
	S(S&&) noexcept            { ++move_constructed; }
	S& operator=(const S&)     { ++copy_assignment; return *this; }
	S& operator=(S&&) noexcept { ++move_assignment; return *this; }
	~S()                       { ++destructed; }

	static void reset() {
		default_constructed = 0;
		copy_constructed = 0;
		move_constructed = 0;
		copy_assignment = 0;
		move_assignment = 0;
		destructed = 0;
	}

	static inline int default_constructed = 0;
	static inline int copy_constructed = 0;
	static inline int move_constructed = 0;
	static inline int copy_assignment = 0;
	static inline int move_assignment = 0;
	static inline int destructed = 0;
};


TEST_CASE("append")
{
	std::vector<int> v;
	std::vector<int> v0;
	std::vector<int> v123 = {1, 2, 3};
	std::vector<int> v45 = {4, 5};

	SECTION("non-empty + non-empty") {
		append(v123, v45);
		CHECK(v123 == std::vector<int>{1, 2, 3, 4, 5});
	}
	SECTION("non-empty + empty") {
		append(v123, v0);
		CHECK(v123 == std::vector<int>{1, 2, 3});
	}
	SECTION("empty + non-empty") {
		append(v, v45);
		CHECK(v == std::vector<int>{4, 5});
	}
	SECTION("empty + empty") {
		append(v, v0);
		CHECK(v == std::vector<int>{});
	}
	SECTION("non-empty + view") {
		append(v45, std::views::drop(v123, 1));
		CHECK(v45 == std::vector<int>{4, 5, 2, 3});
	}
	SECTION("empty + l-value") {
		{
			S::reset();
			std::vector<S> v1;
			std::vector<S> v2 = {S(), S()};
			CHECK(S::default_constructed == 2);
			CHECK(S::copy_constructed    == 2);
			CHECK(S::move_constructed    == 0);
			CHECK(S::copy_assignment     == 0);
			CHECK(S::move_assignment     == 0);
			CHECK(S::destructed          == 2);
			S::reset();

			append(v1, v2);
			CHECK(S::default_constructed == 0);
			CHECK(S::copy_constructed    == 2);
			CHECK(S::move_constructed    == 0);
			CHECK(S::copy_assignment     == 0);
			CHECK(S::move_assignment     == 0);
			CHECK(S::destructed          == 0);
			S::reset();
		}
		CHECK(S::default_constructed == 0);
		CHECK(S::copy_constructed    == 0);
		CHECK(S::move_constructed    == 0);
		CHECK(S::copy_assignment     == 0);
		CHECK(S::move_assignment     == 0);
		CHECK(S::destructed          == 4);
	}
	SECTION("empty + r-value") {
		{
			S::reset();
			std::vector<S> v1;
			std::vector<S> v2 = {S(), S()};
			CHECK(S::default_constructed == 2);
			CHECK(S::copy_constructed    == 2);
			CHECK(S::move_constructed    == 0);
			CHECK(S::copy_assignment     == 0);
			CHECK(S::move_assignment     == 0);
			CHECK(S::destructed          == 2);
			S::reset();

			append(v1, std::move(v2));
			CHECK(S::default_constructed == 0);
			CHECK(S::copy_constructed    == 0);
			CHECK(S::move_constructed    == 0);
			CHECK(S::copy_assignment     == 0);
			CHECK(S::move_assignment     == 0);
			CHECK(S::destructed          == 0);
			S::reset();
		}
		CHECK(S::default_constructed == 0);
		CHECK(S::copy_constructed    == 0);
		CHECK(S::move_constructed    == 0);
		CHECK(S::copy_assignment     == 0);
		CHECK(S::move_assignment     == 0);
		CHECK(S::destructed          == 2);
	}
}


TEST_CASE("to_vector: from list")
{
	SECTION("deduce type") {
		std::list<int> l = {1, 2, 3};
		auto v = to_vector(l);
		CHECK(v.size() == 3);
		CHECK(v[0] == 1);
		CHECK(v[1] == 2);
		CHECK(v[2] == 3);
		CHECK(std::is_same_v<decltype(v)::value_type, int>);
	}
	SECTION("convert type") {
		std::list<int> l = {1, 2, 3};
		auto v = to_vector<char>(l);
		CHECK(v.size() == 3);
		CHECK(v[0] == 1);
		CHECK(v[1] == 2);
		CHECK(v[2] == 3);
		CHECK(std::is_same_v<decltype(v)::value_type, char>);
	}
}

TEST_CASE("to_vector: from view")
{
	std::vector<int> v1 = {1, 2, 3};
	SECTION("deduce type") {
		auto v = to_vector(std::views::drop(v1, 2));
		CHECK(v.size() == 1);
		CHECK(v[0] == 3);
		CHECK(std::is_same_v<decltype(v)::value_type, int>);
	}
	SECTION("convert type") {
		auto v = to_vector<long long>(std::views::drop(v1, 1));
		CHECK(v.size() == 2);
		CHECK(v[0] == 2LL);
		CHECK(v[1] == 3LL);
		CHECK(std::is_same_v<decltype(v)::value_type, long long>);
	}
}


TEST_CASE("to_vector: optimized r-value")
{
	SECTION("l-value") {
		S::reset();
		{
			std::vector<S> v1 = {S(), S(), S()};
			CHECK(S::default_constructed == 3);
			CHECK(S::copy_constructed    == 3);
			CHECK(S::move_constructed    == 0);
			CHECK(S::copy_assignment     == 0);
			CHECK(S::move_assignment     == 0);
			CHECK(S::destructed          == 3);
			S::reset();

			auto v = to_vector(v1);
			CHECK(S::default_constructed == 0);
			CHECK(S::copy_constructed    == 3);
			CHECK(S::move_constructed    == 0);
			CHECK(S::copy_assignment     == 0);
			CHECK(S::move_assignment     == 0);
			CHECK(S::destructed          == 0);
			S::reset();
		}
		CHECK(S::default_constructed == 0);
		CHECK(S::copy_constructed    == 0);
		CHECK(S::move_constructed    == 0);
		CHECK(S::copy_assignment     == 0);
		CHECK(S::move_assignment     == 0);
		CHECK(S::destructed          == 6);
	}
	SECTION("r-value") {
		S::reset();
		{
			std::vector<S> v1 = {S(), S(), S()};
			CHECK(S::default_constructed == 3);
			CHECK(S::copy_constructed    == 3);
			CHECK(S::move_constructed    == 0);
			CHECK(S::copy_assignment     == 0);
			CHECK(S::move_assignment     == 0);
			CHECK(S::destructed          == 3);
			S::reset();

			auto v = to_vector(std::move(v1));
			CHECK(S::default_constructed == 0);
			CHECK(S::copy_constructed    == 0);
			CHECK(S::move_constructed    == 0);
			CHECK(S::copy_assignment     == 0);
			CHECK(S::move_assignment     == 0);
			CHECK(S::destructed          == 0);
			S::reset();
		}
		CHECK(S::default_constructed == 0);
		CHECK(S::copy_constructed    == 0);
		CHECK(S::move_constructed    == 0);
		CHECK(S::copy_assignment     == 0);
		CHECK(S::move_assignment     == 0);
		CHECK(S::destructed          == 3);
	}
}


// check quality of generated code
#if 0
std::vector<int> check1(const std::vector<int>& v)
{
	return to_vector(std::views::drop(v, 1));
}
std::vector<int> check2(const std::vector<int>& v)
{
	return std::vector<int>(begin(v) + 1, end(v));
}
#endif
