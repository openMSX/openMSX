#include "catch.hpp"
#include "CircularBuffer.hh"

using namespace openmsx;

TEST_CASE("CircularBuffer")
{
	CircularBuffer<int, 2> buf;
	CHECK(buf.isEmpty());
	CHECK(!buf.isFull());
	CHECK(buf.size() == 0);

	buf.addBack(15);
	CHECK(!buf.isEmpty());
	CHECK(!buf.isFull());
	CHECK(buf.size() == 1);
	CHECK(buf[0] == 15);

	buf[0] = 25;
	CHECK(buf[0] == 25);

	buf.addFront(17);
	CHECK(!buf.isEmpty());
	CHECK(buf.isFull());
	CHECK(buf.size() == 2);
	CHECK(buf[0] == 17);
	CHECK(buf[1] == 25);

	buf[1] = 35;
	CHECK(buf[0] == 17);
	CHECK(buf[1] == 35);
	buf[0] = 27;
	CHECK(buf[0] == 27);
	CHECK(buf[1] == 35);

	int a = buf.removeBack();
	CHECK(a == 35);
	CHECK(buf.size() == 1);
	CHECK(buf[0] == 27);

	int b = buf.removeFront();
	CHECK(b == 27);
	CHECK(buf.isEmpty());
}
