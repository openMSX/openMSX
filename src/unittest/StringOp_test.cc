#include "catch.hpp"
#include "StringOp.hh"
#include <type_traits>

using namespace StringOp;
using std::string;
using std::string_view;
using std::vector;

static void testStringToInt(const string& s, bool ok, int expected)
{
	int result;
	bool success = stringToInt(s, result);
	REQUIRE(success == ok);
	if (ok) {
		CHECK(result == expected);
		CHECK(stringToInt(s) == expected);
	}
}

static void checkTrimRight(const string& s, char c, const string& expected)
{
	string test = s;
	trimRight(test, c);
	CHECK(test == expected);

	string_view ref = s;
	trimRight(ref, c);
	CHECK(ref == expected);
}
static void checkTrimRight(const string& s, const char* chars, const string& expected)
{
	string test = s;
	trimRight(test, chars);
	CHECK(test == expected);

	string_view ref = s;
	trimRight(ref, chars);
	CHECK(ref == expected);
}

static void checkTrimLeft(const string& s, char c, const string& expected)
{
	string test = s;
	trimLeft(test, c);
	CHECK(test == expected);

	string_view ref = s;
	trimLeft(ref, c);
	CHECK(ref == expected);
}
static void checkTrimLeft(const string& s, const char* chars, const string& expected)
{
	string test = s;
	trimLeft(test, chars);
	CHECK(test == expected);

	string_view ref = s;
	trimLeft(ref, chars);
	CHECK(ref == expected);
}

static void checkSplitOnFirst(const string& s, const string& first, const string& last)
{
	auto [f1, l1] = splitOnFirst(s, '-');
	auto [f2, l2] = splitOnFirst(s, " -+");
	static_assert(std::is_same_v<decltype(f1), std::string_view>);
	static_assert(std::is_same_v<decltype(f2), std::string_view>);
	static_assert(std::is_same_v<decltype(l1), std::string_view>);
	static_assert(std::is_same_v<decltype(l2), std::string_view>);
	CHECK(f1 == first);
	CHECK(f2 == first);
	CHECK(l1 == last);
	CHECK(l2 == last);
}

static void checkSplitOnLast(const string& s, const string& first, const string& last)
{
	auto [f1, l1] = splitOnLast(s, '-');
	auto [f2, l2] = splitOnLast(s, " -+");
	static_assert(std::is_same_v<decltype(f1), std::string_view>);
	static_assert(std::is_same_v<decltype(f2), std::string_view>);
	static_assert(std::is_same_v<decltype(l1), std::string_view>);
	static_assert(std::is_same_v<decltype(l2), std::string_view>);
	CHECK(f1 == first);
	CHECK(f2 == first);
	CHECK(l1 == last);
	CHECK(l2 == last);
}

static void checkSplit(const string& s, const vector<string_view> expected)
{
	CHECK(split(s, '-') == expected);
}

static void checkParseRange(const string& s, const vector<unsigned>& expected)
{
	CHECK(parseRange(s, 0, 99) == expected);
}

TEST_CASE("StringOp")
{
	SECTION("stringToXXX") {
		testStringToInt("", true, 0);
		testStringToInt("0", true, 0);
		testStringToInt("1234", true, 1234);
		testStringToInt("-1234", true, -1234);
		testStringToInt("0xabcd", true, 43981);
		testStringToInt("0x7fffffff", true, 2147483647);
		testStringToInt("-0x80000000", true, -2147483648);
		testStringToInt("bla", false, 0);
		//testStringToInt("0x80000000", true, 0); not detected correctly

		// TODO stringToUint
		// TODO stringToUint64

		CHECK(stringToBool("0") == false);
		CHECK(stringToBool("1") == true);
		CHECK(stringToBool("Yes") == true);
		CHECK(stringToBool("yes") == true);
		CHECK(stringToBool("YES") == true);
		CHECK(stringToBool("No") == false);
		CHECK(stringToBool("no") == false);
		CHECK(stringToBool("NO") == false);
		CHECK(stringToBool("True") == true);
		CHECK(stringToBool("true") == true);
		CHECK(stringToBool("TRUE") == true);
		CHECK(stringToBool("False") == false);
		CHECK(stringToBool("false") == false);
		CHECK(stringToBool("FALSE") == false);
		// These two behave different as Tcl
		CHECK(stringToBool("2") == false); // is true in Tcl
		CHECK(stringToBool("foobar") == false); // is error in Tcl

		// TODO stringToDouble
	}
	SECTION("toLower") {
		CHECK(toLower("") == "");
		CHECK(toLower("foo") == "foo");
		CHECK(toLower("FOO") == "foo");
		CHECK(toLower("fOo") == "foo");
		CHECK(toLower(string("FoO")) == "foo");
	}
	SECTION("startsWith") {
		CHECK      (startsWith("foobar", "foo"));
		CHECK_FALSE(startsWith("foobar", "bar"));
		CHECK_FALSE(startsWith("ba", "bar"));
		CHECK      (startsWith("", ""));
		CHECK_FALSE(startsWith("", "bar"));
		CHECK      (startsWith("foobar", ""));

		CHECK      (startsWith("foobar", 'f'));
		CHECK_FALSE(startsWith("foobar", 'b'));
		CHECK_FALSE(startsWith("", 'b'));
	}
	SECTION("endsWith") {
		CHECK      (endsWith("foobar", "bar"));
		CHECK_FALSE(endsWith("foobar", "foo"));
		CHECK_FALSE(endsWith("ba", "bar"));
		CHECK_FALSE(endsWith("ba", "baba"));
		CHECK      (endsWith("", ""));
		CHECK_FALSE(endsWith("", "bar"));
		CHECK      (endsWith("foobar", ""));

		CHECK      (endsWith("foobar", 'r'));
		CHECK_FALSE(endsWith("foobar", 'o'));
		CHECK_FALSE(endsWith("", 'b'));
	}
	SECTION("trimRight") {
		checkTrimRight("", ' ', "");
		checkTrimRight("  ", ' ', "");
		checkTrimRight("foo", ' ', "foo");
		checkTrimRight("  foo", ' ', "  foo");
		checkTrimRight("foo  ", ' ', "foo");

		checkTrimRight("", "o ", "");
		checkTrimRight(" o ", "o ", "");
		checkTrimRight("foobar", "o ", "foobar");
		checkTrimRight("  foobar", "o ", "  foobar");
		checkTrimRight("foo  ", "o ", "f");
	}
	SECTION("trimLeft") {
		checkTrimLeft("", ' ', "");
		checkTrimLeft("  ", ' ', "");
		checkTrimLeft("foo", ' ', "foo");
		checkTrimLeft("foo  ", ' ', "foo  ");
		checkTrimLeft("  foo", ' ', "foo");

		checkTrimLeft("", "f ", "");
		checkTrimLeft(" f ", "f ", "");
		checkTrimLeft("foo", "f ", "oo");
		checkTrimLeft("barfoo  ", "f ", "barfoo  ");
		checkTrimLeft("  foo", "f ", "oo");
	}
	SECTION("splitOnFirst") {
		checkSplitOnFirst("", "", "");
		checkSplitOnFirst("-", "", "");
		checkSplitOnFirst("foo-", "foo", "");
		checkSplitOnFirst("-foo", "", "foo");
		checkSplitOnFirst("foo-bar", "foo", "bar");
		checkSplitOnFirst("foo-bar-qux", "foo", "bar-qux");
		checkSplitOnFirst("-bar-qux", "", "bar-qux");
		checkSplitOnFirst("foo-bar-", "foo", "bar-");
	}
	SECTION("splitOnLast") {
		checkSplitOnLast("", "", "");
		checkSplitOnLast("-", "", "");
		checkSplitOnLast("foo-", "foo", "");
		checkSplitOnLast("-foo", "", "foo");
		checkSplitOnLast("foo-bar", "foo", "bar");
		checkSplitOnLast("foo-bar-qux", "foo-bar", "qux");
		checkSplitOnLast("-bar-qux", "-bar", "qux");
		checkSplitOnLast("foo-bar-", "foo-bar", "");
	}
	SECTION("split") {
		checkSplit("", {});
		checkSplit("-", {""});
		checkSplit("foo-", {"foo"});
		checkSplit("-foo", {"", "foo"});
		checkSplit("foo-bar", {"foo", "bar"});
		checkSplit("foo-bar-qux", {"foo", "bar", "qux"});
		checkSplit("-bar-qux", {"", "bar", "qux"});
		checkSplit("foo-bar-", {"foo", "bar"});
	}
	SECTION("parseRange") {
		checkParseRange("", {});
		checkParseRange("5", {5});
		checkParseRange("5,8", {5,8});
		checkParseRange("5,5", {5});
		checkParseRange("5-7", {5,6,7});
		checkParseRange("7-5", {5,6,7});
		checkParseRange("5-7,19", {5,6,7,19});
		checkParseRange("15,5-7", {5,6,7,15});
		checkParseRange("6,5-7", {5,6,7});
		checkParseRange("5-8,10-12", {5,6,7,8,10,11,12});
		checkParseRange("5-9,6-10", {5,6,7,8,9,10});

		CHECK_THROWS (parseRange( "4", 5, 10));
		CHECK_NOTHROW(parseRange( "5", 5, 10));
		CHECK_NOTHROW(parseRange("10", 5, 10));
		CHECK_THROWS (parseRange("11", 5, 10));
	}
	SECTION("caseless") {
		caseless op;
		CHECK( op("abc", "xyz"));
		CHECK(!op("xyz", "abc"));
		CHECK(!op("abc", "abc"));
		CHECK( op("ABC", "xyz"));
		CHECK(!op("xyz", "ABC"));
		CHECK(!op("ABC", "abc"));
		CHECK( op("aBC", "Xyz"));
		CHECK(!op("xYz", "AbC"));
		CHECK(!op("ABc", "abC"));

		CHECK( op("abc", "ABCdef"));
		CHECK(!op("AbcDef", "AbC"));
	}
	SECTION("casecmp") {
		casecmp op;
		CHECK( op("abc", "abc"));
		CHECK( op("abc", "ABC"));
		CHECK(!op("abc", "xyz"));
		CHECK(!op("ab", "abc"));
		CHECK(!op("ab", "ABC"));
		CHECK(!op("abc", "ab"));
		CHECK(!op("abc", "AB"));
	}
}
