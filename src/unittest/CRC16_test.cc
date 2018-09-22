#include "catch.hpp"
#include "CRC16.hh"

using namespace openmsx;

// TODO this is an extremely minimal test. Extend it.

TEST_CASE("CRC16")
{
	CRC16 crc;
	CHECK(crc.getValue() == 0xFFFF);

	crc.update(0xA1);
	crc.update(0xA1);
	crc.update(0xA1);
	CHECK(crc.getValue() == 0xCDB4);
}
