#include "catch.hpp"
#include "CircularBuffer.hh"

using namespace openmsx;

TEST_CASE("CircularBuffer")
{
	CircularBuffer<int, 3> buf;
	CHECK(buf.empty());
	CHECK(!buf.full());
	CHECK(buf.size() == 0);

	buf.push_back(15);
	CHECK(!buf.empty());
	CHECK(!buf.full());
	CHECK(buf.size() == 1);
	CHECK(buf[0] == 15);
	CHECK(buf.front() == 15);
	CHECK(buf.back() == 15);

	buf[0] = 25;
	CHECK(buf[0] == 25);

	buf.push_front(17);
	CHECK(!buf.empty());
	CHECK(!buf.full());
	CHECK(buf.size() == 2);
	CHECK(buf[0] == 17);
	CHECK(buf[1] == 25);
	CHECK(buf.front() == 17);
	CHECK(buf.back() == 25);

	buf[1] = 35;
	CHECK(buf[0] == 17);
	CHECK(buf[1] == 35);
	buf[0] = 27;
	CHECK(buf[0] == 27);
	CHECK(buf[1] == 35);

	buf.push_back(11);
	CHECK(buf.full());
	CHECK(!buf.empty());
	CHECK(buf.size() == 3);
	CHECK(buf.front() == 27);
	CHECK(buf.back() == 11);

	CHECK(buf.pop_front() == 27);
	CHECK(!buf.full());
	CHECK(buf.size() == 2);
	CHECK(buf.front() == 35);
	CHECK(buf.back() == 11);

	CHECK(buf.pop_back() == 11);
	CHECK(buf.size() == 1);
	CHECK(buf[0] == 35);

	CHECK(buf.pop_front() == 35);
	CHECK(buf.empty());
}
