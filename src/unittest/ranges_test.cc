#include "catch.hpp"
#include "ranges.hh"
#include <array>
#include <vector>

TEST_CASE("binary_find")
{
	SECTION("no projection") {
		std::array a = {3, 5, 9, 13, 19, 22, 45, 87, 98};

		SECTION("found") {
			SECTION("first") {
				auto f = binary_find(a, 3);
				CHECK(*f == 3);
				CHECK(f == &a[0]);
			}
			SECTION("middle") {
				auto f = binary_find(a, 19);
				CHECK(*f == 19);
				CHECK(f == &a[4]);
			}
			SECTION("last") {
				auto f = binary_find(a, 98);
				CHECK(*f == 98);
				CHECK(f == &a[8]);
			}
		}
		SECTION("not found") {
			SECTION("before") {
				CHECK(binary_find(a, 2) == nullptr);
			}
			SECTION("middle") {
				CHECK(binary_find(a, 28) == nullptr);
			}
			SECTION("after") {
				CHECK(binary_find(a, 99) == nullptr);
			}
		}
	}
	SECTION("no projection, reverse sorted") {
		std::vector v = {86, 54, 33, 29, 14, 3};
		CHECK(binary_find(v, 33, std::greater{}) == &v[2]);
		CHECK(binary_find(v, 14, std::greater{}) == &v[4]);
		CHECK(binary_find(v, 48, std::greater{}) == nullptr);
		CHECK(binary_find(v,  7, std::greater{}) == nullptr);
	}
	SECTION("projection") {
		struct S {
			int i;
			std::string s;
		};
		std::vector v = {S{4, "four"}, S{6, "six"}, S{10, "ten"}, S{99, "a lot"}};

		CHECK(binary_find(v,  4, {}, &S::i) == &v[0]);
		CHECK(binary_find(v, 99, {}, &S::i) == &v[3]);
		CHECK(binary_find(v,  6, {}, &S::i)->s == "six");
		CHECK(binary_find(v, 10, {}, &S::i)->s == "ten");
		CHECK(binary_find(v,   1, {}, &S::i) == nullptr);
		CHECK(binary_find(v,  17, {}, &S::i) == nullptr);
		CHECK(binary_find(v, 100, {}, &S::i) == nullptr);
	}
}
