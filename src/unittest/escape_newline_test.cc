#include "catch.hpp"
#include "escape_newline.hh"

TEST_CASE("escape_newline")
{
	SECTION("round trip") {
		auto test = [](std::string_view s) {
			auto e = escape_newline::encode(s);
			auto d = escape_newline::decode(e);
			CHECK(e.size() >= d.size());
			CHECK(d == s);
		};
		test("");
		test("bla");
		test("bla\nbla");
		test("bla\\bla");
		test("\n\n\\\\");
	}
	SECTION("errors") {
		// * All inputs are valid for encode().
		// * But some inputs should normally never be passed to decode()
		//   (as in: they can never have been the result of a prior
		//   encode()).
		//   Nevertheless we try to recover from such inputs.

		// Single backslash at the end -> gets dropped.
		CHECK(escape_newline::decode("bla\\") == "bla");

		// Normally only "\\" or "\n" should occur, all other "\<x>" are
		// in principle invalid, but we decode them as 'x'
		CHECK(escape_newline::decode("bl\\a") == "bla");
	}
}
