#include "catch.hpp"
#include "IterableBitSet.hh"
#include "ranges.hh"
#include <iostream>
#include <vector>

template<size_t N>
void expect(const IterableBitSet<N>& s, const std::vector<size_t>& expected)
{
	auto it = expected.begin();
	s.foreachSetBit([&](size_t i) {
		REQUIRE(it != expected.end());
		CHECK(*it++ == i);
	});
	CHECK(it == expected.end());
}

template<size_t N>
void test(const IterableBitSet<N>& s, std::initializer_list<size_t> list)
{
	REQUIRE((list.size() % 2) == 0);
	std::vector<size_t> v;
	auto f = list.begin();
	auto l = list.end();
	while (f != l) {
		auto b = *f++;
		auto e = *f++;
		for (auto i = b; i != e; ++i) v.push_back(i);
	}
	ranges::sort(v);
	v.erase(std::unique(v.begin(), v.end()), v.end());

	expect(s, v);
}

template<size_t N>
void test(std::initializer_list<size_t> list)
{
	IterableBitSet<N> s1;
	IterableBitSet<N> s2;

	REQUIRE((list.size() % 2) == 0);
	auto f = list.begin();
	auto l = list.end();
	while (f != l) {
		auto b = *f++;
		auto e = *f++;
		REQUIRE(b <= e);
		REQUIRE(e <= 256);
		s1.setPosN(b, e - b);
		s2.setRange(b, e);
	}
	test(s1, list);
	test(s2, list);
}

TEST_CASE("IterableBitSet")
{
	SECTION("empty") {
		SECTION("single word") {
			IterableBitSet<40> s;
			CHECK(s.empty());
			s.set(2);
			CHECK(!s.empty());
		}
		SECTION("multiple words") {
			IterableBitSet<100> s;
			CHECK(s.empty());
			SECTION("1st") {
				s.set(2);
				CHECK(!s.empty());
			}
			SECTION("2nd") {
				s.set(70);
				CHECK(!s.empty());
			}
		}
	}
	SECTION("set single") {
		SECTION("single word") {
			IterableBitSet<23> s;
			expect(s, {});
			s.set(11);
			expect(s, {11});
			s.set(17);
			expect(s, {11, 17});
			s.set(3);
			expect(s, {3, 11, 17});
		}
		SECTION("multiple words") {
			IterableBitSet<111> s;
			expect(s, {});
			s.set(87);
			expect(s, {87});
			s.set(17);
			expect(s, {17, 87});
			s.set(110);
			expect(s, {17, 87, 110});
		}
	}
	SECTION("set range") {
		SECTION("single word") {
			test<32>({});

			test<32>({0, 0});
			test<32>({0, 16});
			test<32>({0, 31});
			test<32>({0, 32});

			test<32>({10, 10});
			test<32>({10, 16});
			test<32>({10, 31});
			test<32>({10, 32});

			test<32>({31, 31});
			test<32>({31, 32});

			test<32>({32, 32});

			test<32>({10, 20,  25, 30});
		}
		SECTION("multiple words") {
			test<256>({});

			test<256>({0, 0});
			test<256>({0, 16});
			test<256>({0, 63});
			test<256>({0, 64});
			test<256>({0, 65});
			test<256>({0, 80});
			test<256>({0, 127});
			test<256>({0, 128});
			test<256>({0, 143});
			test<256>({0, 255});
			test<256>({0, 256});

			test<256>({10, 10});
			test<256>({10, 16});
			test<256>({10, 63});
			test<256>({10, 64});
			test<256>({10, 65});
			test<256>({10, 80});
			test<256>({10, 127});
			test<256>({10, 128});
			test<256>({10, 143});
			test<256>({10, 255});
			test<256>({10, 256});

			test<256>({63, 63});
			test<256>({63, 64});
			test<256>({63, 65});
			test<256>({63, 80});
			test<256>({63, 127});
			test<256>({63, 128});
			test<256>({63, 143});
			test<256>({63, 255});
			test<256>({63, 256});

			test<256>({64, 64});
			test<256>({64, 65});
			test<256>({64, 80});
			test<256>({64, 127});
			test<256>({64, 128});
			test<256>({64, 143});
			test<256>({64, 255});
			test<256>({64, 256});

			test<256>({10, 20,  40, 50});
			test<256>({10, 20,  60, 70});
			test<256>({10, 40,  20, 30});
			test<256>({10, 40,  20, 50});
		}
	}
}
