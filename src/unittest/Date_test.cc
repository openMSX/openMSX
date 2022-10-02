#include "catch.hpp"
#include "Date.hh"
#include "ranges.hh"
#include <cstring>

using namespace openmsx;

static void test(time_t t, const char* s)
{
	REQUIRE(strlen(s) >= 24); // precondition
	CHECK(Date::fromString(std::span<const char, 24>{s, 24}) == t);
	CHECK(Date::toString(t) == s);
}

TEST_CASE("Date")
{
	putenv(const_cast<char*>("TZ=UTC")); tzset();
	test(         0, "Thu Jan 01 00:00:00 1970");
	test(         1, "Thu Jan 01 00:00:01 1970");
	test(        60, "Thu Jan 01 00:01:00 1970");
	test(      3600, "Thu Jan 01 01:00:00 1970");
	test(1318850077, "Mon Oct 17 11:14:37 2011");
	test(1403092862, "Wed Jun 18 12:01:02 2014");

	// Check invalid formats
	// - invalid separator characters
	CHECK(Date::fromString(subspan<24>("WedXJun 18 12:01:02 2014")) == Date::INVALID_TIME_T);
	CHECK(Date::fromString(subspan<24>("Wed JunX18 12:01:02 2014")) == Date::INVALID_TIME_T);
	CHECK(Date::fromString(subspan<24>("Wed Jun 18X12:01:02 2014")) == Date::INVALID_TIME_T);
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 12X01:02 2014")) == Date::INVALID_TIME_T);
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 12:01X02 2014")) == Date::INVALID_TIME_T);
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 12:01:02X2014")) == Date::INVALID_TIME_T);
	// - weekday is not verified
	// - invalid month (must also have correct case)
	CHECK(Date::fromString(subspan<24>("Wed Foo 18 12:01:02 2014")) == Date::INVALID_TIME_T);
	CHECK(Date::fromString(subspan<24>("Wed jun 18 12:01:02 2014")) == Date::INVALID_TIME_T);
	// - invalid day
	CHECK(Date::fromString(subspan<24>("Wed Jun 00 12:01:02 2014")) == Date::INVALID_TIME_T);
	CHECK(Date::fromString(subspan<24>("Wed Jun 32 12:01:02 2014")) == Date::INVALID_TIME_T);
	// - invalid hour
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 24:01:02 2014")) == Date::INVALID_TIME_T);
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 xx:01:02 2014")) == Date::INVALID_TIME_T);
	// - invalid minute
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 12:60:02 2014")) == Date::INVALID_TIME_T);
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 12:-1:02 2014")) == Date::INVALID_TIME_T);
	// - invalid second
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 12:01:60 2014")) == Date::INVALID_TIME_T);
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 12:01:0 2014 ")) == Date::INVALID_TIME_T);
	// - invalid year
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 12:01:02 1800")) == Date::INVALID_TIME_T);
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 12:01:02 X800")) == Date::INVALID_TIME_T);

	// extra characters at the end are ignored, even digits
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 12:01:02 2014x")) == 1403092862);
	CHECK(Date::fromString(subspan<24>("Wed Jun 18 12:01:02 20140")) == 1403092862);
}
