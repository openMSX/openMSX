#include "catch.hpp"
#include "narrow.hh"
#include <cstdint>
#include <type_traits>

template<typename From, typename To>
bool test_narrow(From from, To expected)
{
	// When compiling unittests this will throw on error.
	// But for normal builds it triggers an assertion.
	auto result = narrow<To>(from);
	static_assert(std::is_same_v<decltype(result), To>);
	CHECK(result == expected);
	return true;
}

TEST_CASE("narrow")
{
	int ERR = 0;

	SECTION("int32_t -> uint8_t") {
		CHECK_NOTHROW(test_narrow(int32_t(   0), uint8_t(  0)));
		CHECK_NOTHROW(test_narrow(int32_t( 100), uint8_t(100)));
		CHECK_NOTHROW(test_narrow(int32_t( 200), uint8_t(200)));
		CHECK_NOTHROW(test_narrow(int32_t( 255), uint8_t(255)));

		CHECK_THROWS (test_narrow(int32_t(-999), uint8_t(ERR)));
		CHECK_THROWS (test_narrow(int32_t(-256), uint8_t(ERR)));
		CHECK_THROWS (test_narrow(int32_t(-255), uint8_t(ERR)));
		CHECK_THROWS (test_narrow(int32_t(  -1), uint8_t(ERR)));
		CHECK_THROWS (test_narrow(int32_t( 256), uint8_t(ERR)));
		CHECK_THROWS (test_narrow(int32_t( 300), uint8_t(ERR)));
		CHECK_THROWS (test_narrow(int32_t(9999), uint8_t(ERR)));
	}
	SECTION("int32_t -> int8_t") {
		CHECK_NOTHROW(test_narrow(int32_t(-128), int8_t(-128)));
		CHECK_NOTHROW(test_narrow(int32_t( -32), int8_t( -32)));
		CHECK_NOTHROW(test_narrow(int32_t(   0), int8_t(   0)));
		CHECK_NOTHROW(test_narrow(int32_t(   7), int8_t(   7)));
		CHECK_NOTHROW(test_narrow(int32_t( 100), int8_t( 100)));
		CHECK_NOTHROW(test_narrow(int32_t( 127), int8_t( 127)));

		CHECK_THROWS (test_narrow(int32_t(-999), int8_t(ERR)));
		CHECK_THROWS (test_narrow(int32_t(-456), int8_t(ERR)));
		CHECK_THROWS (test_narrow(int32_t(-129), int8_t(ERR)));
		CHECK_THROWS (test_narrow(int32_t( 128), int8_t(ERR)));
		CHECK_THROWS (test_narrow(int32_t( 356), int8_t(ERR)));
		CHECK_THROWS (test_narrow(int32_t(9999), int8_t(ERR)));
	}
	SECTION("uint32_t -> uint8_t") {
		CHECK_NOTHROW(test_narrow(uint32_t(   0), uint8_t(  0)));
		CHECK_NOTHROW(test_narrow(uint32_t( 100), uint8_t(100)));
		CHECK_NOTHROW(test_narrow(uint32_t( 200), uint8_t(200)));
		CHECK_NOTHROW(test_narrow(uint32_t( 255), uint8_t(255)));

		CHECK_THROWS (test_narrow(uint32_t( 256), uint8_t(ERR)));
		CHECK_THROWS (test_narrow(uint32_t( 300), uint8_t(ERR)));
		CHECK_THROWS (test_narrow(uint32_t(9999), uint8_t(ERR)));
	}
	SECTION("uint32_t -> int8_t") {
		CHECK_NOTHROW(test_narrow(uint32_t(   0), int8_t(   0)));
		CHECK_NOTHROW(test_narrow(uint32_t(   7), int8_t(   7)));
		CHECK_NOTHROW(test_narrow(uint32_t( 100), int8_t( 100)));
		CHECK_NOTHROW(test_narrow(uint32_t( 127), int8_t( 127)));

		CHECK_THROWS (test_narrow(uint32_t( 128), int8_t(ERR)));
		CHECK_THROWS (test_narrow(uint32_t( 356), int8_t(ERR)));
		CHECK_THROWS (test_narrow(uint32_t(9999), int8_t(ERR)));
	}
}
