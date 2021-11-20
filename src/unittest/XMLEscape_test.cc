#include "catch.hpp"
#include "XMLEscape.hh"

TEST_CASE("XMLEscape")
{
	SECTION("1 special char") {
		CHECK(XMLEscape("<") == "&lt;");
	}
	SECTION("data + char") {
		CHECK(XMLEscape("foobar>") == "foobar&gt;");
	}
	SECTION("char + data") {
		CHECK(XMLEscape("&foobar") == "&amp;foobar");
	}
	SECTION("char + data + char") {
		CHECK(XMLEscape("'foobar\"") == "&apos;foobar&quot;");
	}
	SECTION("data + char + data") {
		CHECK(XMLEscape("foo\007bar") == "foo&#x07;bar");
	}
}
