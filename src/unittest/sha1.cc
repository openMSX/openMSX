#include "catch.hpp"
#include "sha1.hh"

using namespace openmsx;

TEST_CASE("sha1: calc")
{
	const char* input = "abc";
	Sha1Sum output = SHA1::calc(reinterpret_cast<const uint8_t*>(input), 3);
	CHECK(output.toString() == "a9993e364706816aba3e25717850c26c9cd0d89d");
}
