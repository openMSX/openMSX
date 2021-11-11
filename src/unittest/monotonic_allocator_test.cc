#include "catch.hpp"
#include "monotonic_allocator.hh"
#include <cstring>

[[nodiscard]] static char* alloc(monotonic_allocator& a, size_t size, size_t alignment)
{
	char* result = static_cast<char*>(a.allocate(size, alignment));
	CHECK(result != nullptr);
	CHECK((reinterpret_cast<uintptr_t>(result) & (alignment - 1)) == 0);
	memset(result, 0xab, size); // this shouldn't crash
	return result;
}

TEST_CASE("monotonic_allocator")
{
	SECTION("default") {
		monotonic_allocator a;
		char* p1 = alloc(a, 300, 4);

		char* p2 = alloc(a, 600, 4);
		CHECK(p1 + 300 == p2); // adjacent

		char* p3 = alloc(a, 600, 4);
		CHECK(p2 + 600 != p3); // not adjacent, doesn't fit in initial 1kB block

		char* p4 = alloc(a, 600, 4);
		CHECK(p3 + 600 == p4); // adjacent again because 2nd block is 2kB

		char* p5 = alloc(a, 10'000, 4);
		CHECK(p4 + 600 != p5); // not adjacent
	}
	SECTION("initial size") {
		monotonic_allocator a(100);
		char* p1 = alloc(a, 32, 4);

		char* p2 = alloc(a, 32, 4);
		CHECK(p1 + 32 == p2); // adjacent

		char* p3 = alloc(a, 32, 4);
		CHECK(p1 + 64 == p3); // adjacent

		char* p4 = alloc(a, 32, 4);
		CHECK(p1 + 96 != p4); // not adjacent
	}
	SECTION("initial buffer") {
		char buf[100];
		monotonic_allocator a(buf, 100);

		char* p1 = alloc(a, 40, 1);
		CHECK(p1 == buf);

		char* p2 = alloc(a, 40, 1);
		CHECK(p2 == buf + 40);

		char* p3 = alloc(a, 40, 1);
		CHECK(p3 != buf + 80);

		char* p4 = alloc(a, 40, 1);
		CHECK(p4 == p3 + 40);

		char* p5 = alloc(a, 40, 1);
		CHECK(p5 == p3 + 80);
	}
	SECTION("alignment") {
		monotonic_allocator a;
		char* p1 = alloc(a, 13, 1);

		char* p2 = alloc(a, 4, 4);
		CHECK(p2 == p1 + 16);

		char* p3 = alloc(a, 3, 2);
		CHECK(p3 == p2 + 4);

		char* p4 = alloc(a, 3, 2);
		CHECK(p4 == p3 + 4);
	}
}
