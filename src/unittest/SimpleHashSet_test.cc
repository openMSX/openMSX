#include "catch.hpp"

#include "SimpleHashSet.hh"


TEST_CASE("SimpleHashSet")
{
	// construct
	SimpleHashSet<-1, std::hash<int>, std::equal_to<>> s;
	CHECK(s.capacity() == 0);
	CHECK(s.size() == 0);
	CHECK(s.empty());

	// insert 42
	CHECK(!s.contains(42));
	CHECK(!s.find(42));
	CHECK(s.insert(42)); // {42}
	CHECK(s.contains(42));
	CHECK(*(s.find(42)) == 42);
	CHECK(s.capacity() == 2);
	CHECK(s.size() == 1);
	CHECK(!s.empty());

	// reserve
	s.reserve(4);
	CHECK(s.capacity() == 4);
	CHECK(s.size() == 1);

	// insert 42 again
	CHECK(!s.insert(42)); // {42}
	CHECK(s.contains(42));
	CHECK(*(s.find(42)) == 42);
	CHECK(s.capacity() == 4);
	CHECK(s.size() == 1);

	// insert 1, 2, 3
	CHECK(s.insert(1));
	CHECK(s.insert(2));
	CHECK(s.insert(3)); // {1, 2, 3, 42}
	CHECK(*s.find(42) == 42);
	CHECK(*s.find(1) == 1);
	CHECK(*s.find(2) == 2);
	CHECK(*s.find(3) == 3);
	CHECK(!s.find(4));
	CHECK(!s.find(5));
	CHECK(s.size() == 4);
	CHECK(s.capacity() == 4);

	// insert 10, must grow
	CHECK(s.insert(10)); // {1, 2, 3, 10, 42}
	CHECK(*s.find(42) == 42);
	CHECK(*s.find(1) == 1);
	CHECK(*s.find(2) == 2);
	CHECK(*s.find(3) == 3);
	CHECK(!s.find(4));
	CHECK(!s.find(5));
	CHECK(*s.find(10) == 10);
	CHECK(s.size() == 5);
	CHECK(s.capacity() == 8);

	// erase non-existing
	CHECK(!s.erase(99));
	CHECK(s.size() == 5);

	// erase 1, 3
	CHECK(s.erase(1));
	CHECK(s.erase(3)); // {2, 10, 42}
	CHECK(*s.find(42) == 42);
	CHECK(!s.find(1));
	CHECK(*s.find(2) == 2);
	CHECK(!s.find(3));
	CHECK(!s.find(4));
	CHECK(!s.find(5));
	CHECK(*s.find(10) == 10);
	CHECK(s.size() == 3);
	CHECK(s.capacity() == 8);
}
