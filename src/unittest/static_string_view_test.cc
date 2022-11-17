#include "catch.hpp"
#include "static_string_view.hh"

TEST_CASE("static_string_view")
{
	// construct from string-literal
	static_string_view v1("bla"); // ok, this compiles
	std::string_view sv1 = v1;
	CHECK(sv1 == "bla");

	//const char* cp = "foobar";
	//static_string_view v2(cp); // ok, does NOT compile

	//std::string s = "qux";
	//static_string_view v3(s); // ok, does NOT compile

	//char buf[10];
	//static_string_view v4(buf); // ok, does NOT compile

	// This compiles, but ideally it shouldn't :(
	//const char cBuf[3] = { 'f', 'o', 'o' };
	//static_string_view v5(cBuf); // but it does trigger an assert at runtime

	// This compiles, but ideally it shouldn't :(
	const char cBuf0[4] = { 'f', 'o', 'o', '\0' };
	static_string_view v6(cBuf0); // and also no assert
	std::string_view sv6 = v6;
	CHECK(sv6 == "foo");

	// Escape hatch:
	// Not checked by the compiler, it's the programmers responsibility to
	// ensure that the argument passed to the constructor outlives the
	// resulting static_string_view object.
	std::string s7 = "qux";
	static_string_view v7(static_string_view::lifetime_ok_tag{}, s7);
	std::string_view sv7 = v7;
	CHECK(sv7 == "qux");

	std::string s8 = "baz";
	auto [storage8, v8] = make_string_storage(s8);
	std::string_view sv8 = v8;
	CHECK(sv8 == "baz");
	s8 = "changed"; // changing the original string
	CHECK(sv8 == "baz"); // does not change the copy

}
