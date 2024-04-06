#include "catch.hpp"
#include "ranges.hh"
#include "stl.hh"
#include "view.hh"
#include <array>
#include <span>
#include <string>
#include <vector>

TEST_CASE("ranges::copy")
{
	std::array a = {1, 2, 3, 4, 5};
	std::vector v = {9, 9, 9, 9, 9, 9, 9, 9};

	// this is the c++20 std::ranges::copy() version
	SECTION("range to output-iterator") {
		ranges::copy(a, subspan(v, 1).data());
		CHECK(v == std::vector{9, 1, 2, 3, 4, 5, 9, 9});

		ranges::copy(a, v.data());
		CHECK(v == std::vector{1, 2, 3, 4, 5, 5, 9, 9});
	}

	// this is our own extension
	SECTION("sized_range to sized_range") {
		ranges::copy(a, subspan(v, 1));
		CHECK(v == std::vector{9, 1, 2, 3, 4, 5, 9, 9});

		ranges::copy(a, v);
		CHECK(v == std::vector{1, 2, 3, 4, 5, 5, 9, 9});
	}

	// Unfortunately our extension is not 100% backwards compatible.
	// This example breaks:
	SECTION("bw-compat") {
		std::array<int, 10> buffer = {};

		// This now triggers a compilation error: ambiguous overload
		// It compiled fine before our extension.
		//ranges::copy(a, buffer);

		// It worked because c-arrays can (implicitly) decay to pointers
		// when passed to functions. We can do that explicitly to
		// resolve the ambiguity.
		ranges::copy(a, std::begin(buffer));
		CHECK(to_vector(buffer) == std::vector{1, 2, 3, 4, 5, 0, 0, 0, 0, 0});

		// But a better solution is to replace teh c-array with a
		// std::array. (This has other benefits as well, though not
		// relevant for this example).
		std::array<int, 10> buffer2 = {};
		ranges::copy(a, buffer2);
		CHECK(to_vector(buffer2) == std::vector{1, 2, 3, 4, 5, 0, 0, 0, 0, 0});
	}
}

TEST_CASE("ranges::equal")
{
	auto always_equal = [](const auto&, const auto&) { return true; };

	SECTION("sized ranges") {
		std::array  a3 = {1, 2, 3};
		std::vector v3 = {1, 2, 3};
		std::array  a4 = {2, 4, 6, 8};
		std::vector v4 = {1, 2, 3, 4};

		CHECK( ranges::equal(a3, a3));
		CHECK( ranges::equal(a3, v3));
		CHECK(!ranges::equal(a3, a4));
		CHECK(!ranges::equal(a3, v4));

		CHECK( ranges::equal(v3, v3));
		CHECK( ranges::equal(v3, v3));
		CHECK(!ranges::equal(v3, a4));
		CHECK(!ranges::equal(v3, v4));

		CHECK(!ranges::equal(a4, v3));
		CHECK(!ranges::equal(a4, v3));
		CHECK( ranges::equal(a4, a4));
		CHECK(!ranges::equal(a4, v4));

		CHECK(!ranges::equal(v4, v3));
		CHECK(!ranges::equal(v4, v3));
		CHECK(!ranges::equal(v4, a4));
		CHECK( ranges::equal(v4, v4));

		CHECK( ranges::equal(a4, v4, always_equal));
		CHECK(!ranges::equal(a3, a4, always_equal)); // size is different

		auto mul2 = [](const auto& e) { return e * 2; };
		auto div2 = [](const auto& e) { return e / 2; };
		CHECK( ranges::equal(a4, v4, {}, div2));
		CHECK( ranges::equal(a4, v4, {}, {}, mul2));
		CHECK(!ranges::equal(a4, v4, {}, div2, mul2));
		CHECK( ranges::equal(a4, v4, always_equal, div2, mul2));
		CHECK(!ranges::equal(a4, v3, always_equal, div2, mul2)); // size is different
	}
	SECTION("non-sized ranges") {
		std::array a2 = {2, 4};
		std::array a3 = {1, 3, 5};
		std::array a4 = {1, 3, 5, 7};
		std::array a5 = {1, 2, 3, 4, 5};
		auto is_even = [](const auto& e) { return (e & 1) == 0; };
		auto is_odd  = [](const auto& e) { return (e & 1) == 1; };
		auto ve = view::filter(a5, is_even);
		auto vo = view::filter(a5, is_odd);
		// The size of a "view::filter" is only known after the filter
		// has been applied. This makes "view::filter" a non-size range.

		CHECK( ranges::equal(ve, a2));
		CHECK(!ranges::equal(ve, a3));
		CHECK(!ranges::equal(ve, a4));
		CHECK(!ranges::equal(ve, vo));
		CHECK( ranges::equal(ve, ve));

		CHECK(!ranges::equal(vo, a2));
		CHECK( ranges::equal(vo, a3));
		CHECK(!ranges::equal(vo, a4)); // front matches, but a4 is longer
		CHECK( ranges::equal(vo, vo));
		CHECK(!ranges::equal(vo, ve));

		std::array b3 = {9, 9, 9};
		CHECK(!ranges::equal(ve, b3, always_equal)); // different size
		CHECK( ranges::equal(vo, b3, always_equal));
		CHECK(!ranges::equal(b3, ve, always_equal)); // different size
		CHECK( ranges::equal(b3, vo, always_equal));

		struct S {
			int x, y;
		};
		std::array ss = {S{9, 1}, S{9, 3}, S{9, 5}};
		CHECK(!ranges::equal(vo, ss, {}, {}, &S::x));
		CHECK( ranges::equal(vo, ss, {}, {}, &S::y));
		CHECK(!ranges::equal(ss, vo, {}, &S::x));
		CHECK( ranges::equal(ss, vo, {}, &S::y));
	}
}

TEST_CASE("ranges::all_equal")
{
	std::array<int, 0> a = {};
	std::array b = {3};
	std::array c = {3, 3};
	std::array d = {3, 3, 3};
	std::array e = {1, 3, 3};
	std::array f = {3, 1, 3};
	std::array g = {3, 3, 1};
	CHECK( ranges::all_equal(a));
	CHECK( ranges::all_equal(b));
	CHECK( ranges::all_equal(c));
	CHECK( ranges::all_equal(d));
	CHECK(!ranges::all_equal(e));
	CHECK(!ranges::all_equal(f));
	CHECK(!ranges::all_equal(g));

	struct S {
		int x, y;
	};
	std::array s = {S{9, 1}, S{9, 3}, S{9, 5}, S{9, 7}};
	CHECK( ranges::all_equal(s, &S::x));
	CHECK(!ranges::all_equal(s, &S::y));
}

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

TEST_CASE("subspan")
{
	SECTION("from vector") {
		std::vector v = {2, 6, 7, 9, 1, 3, 4, 5, 6, 0, 1};
		SECTION("dynamic size") {
			SECTION("full") {
				auto s = subspan(v, 0);
				CHECK(s.data() == &v[0]);
				CHECK(s.size() == 11);
				CHECK(s.extent == std::dynamic_extent);
			}
			SECTION("till end") {
				auto s = subspan(v, 6);
				CHECK(s.data() == &v[6]);
				CHECK(s.size() == 5);
				CHECK(s.extent == std::dynamic_extent);
			}
			SECTION("till end, empty") {
				auto s = subspan(v, 11);
				CHECK(s.empty());
				CHECK(s.extent == std::dynamic_extent);
			}
			SECTION("from start, with size") {
				auto s = subspan(v, 0, 3);
				CHECK(s.data() == &v[0]);
				CHECK(s.size() == 3);
				CHECK(s.extent == std::dynamic_extent);
			}
			SECTION("middle") {
				auto s = subspan(v, 4, 5);
				CHECK(s.data() == &v[4]);
				CHECK(s.size() == 5);
				CHECK(s.extent == std::dynamic_extent);
			}
		}
		SECTION("fixed size") {
			SECTION("full") {
				auto s = subspan<11>(v);
				CHECK(s.data() == &v[0]);
				CHECK(s.size() == 11);
				CHECK(s.extent == 11);
			}
			SECTION("from start") {
				auto s = subspan<5>(v);
				CHECK(s.data() == &v[0]);
				CHECK(s.size() == 5);
				CHECK(s.extent == 5);
			}
			SECTION("middle") {
				auto s = subspan<7>(v, 2);
				CHECK(s.data() == &v[2]);
				CHECK(s.size() == 7);
				CHECK(s.extent == 7);
			}
			SECTION("empty") {
				auto s = subspan<0>(v, 2);
				CHECK(s.empty());
				CHECK(s.extent == 0);
			}
		}
	}
	SECTION("from array") {
		std::array a = {3, 5, 1, 2, 3, 9, 1, 0};

		auto s1 = subspan(a, 2, 3);
		CHECK(s1.data() == &a[2]);
		CHECK(s1.size() == 3);
		CHECK(s1.extent == std::dynamic_extent);

		auto s2 = subspan<4>(a, 1);
		CHECK(s2.data() == &a[1]);
		CHECK(s2.size() == 4);
		CHECK(s2.extent == 4);
	}
	SECTION("from string") {
		std::string s = "abcdefghijklmnopqrstuvwxyz";

		auto s1 = subspan(s, 20);
		CHECK(s1.data() == &s[20]);
		CHECK(s1.size() == 6);
		CHECK(s1.extent == std::dynamic_extent);

		auto s2 = subspan<3>(s, 6);
		CHECK(s2.data() == &s[6]);
		CHECK(s2.size() == 3);
		CHECK(s2.extent == 3);
	}
	SECTION("from another span") {
		std::vector v = {2, 4, 6, 8, 4, 2};
		std::span s = v;

		auto s1 = subspan(s, 0, 3);
		CHECK(s1.data() == &v[0]);
		CHECK(s1.size() == 3);
		CHECK(s1.extent == std::dynamic_extent);

		auto s2 = subspan<4>(s, 2);
		CHECK(s2.data() == &v[2]);
		CHECK(s2.size() == 4);
		CHECK(s2.extent == 4);
	}
	SECTION("from a class with begin/end methods") {
		struct S {
			const auto* begin() const { return a.begin(); }
			const auto* end()   const { return a.end(); }
			std::array<int, 10> a;
		} s;

		auto s1 = subspan(s, 4, 5);
		CHECK(s1.data() == &s.a[4]);
		CHECK(s1.size() == 5);
		CHECK(s1.extent == std::dynamic_extent);

		auto s2 = subspan<7>(s, 1);
		CHECK(s2.data() == &s.a[1]);
		CHECK(s2.size() == 7);
		CHECK(s2.extent == 7);
	}
}
