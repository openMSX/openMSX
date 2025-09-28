#include "catch.hpp"
#include "StringOp.hh"

#include "narrow.hh"

#include <type_traits>

using namespace StringOp;

static void checkTrimRight(const std::string& s, char c, const std::string& expected)
{
	std::string test = s;
	trimRight(test, c);
	CHECK(test == expected);

	std::string_view ref = s;
	trimRight(ref, c);
	CHECK(ref == expected);
}
static void checkTrimRight(const std::string& s, const char* chars, const std::string& expected)
{
	std::string test = s;
	trimRight(test, chars);
	CHECK(test == expected);

	std::string_view ref = s;
	trimRight(ref, chars);
	CHECK(ref == expected);
}

static void checkTrimLeft(const std::string& s, char c, const std::string& expected)
{
	std::string test = s;
	trimLeft(test, c);
	CHECK(test == expected);

	std::string_view ref = s;
	trimLeft(ref, c);
	CHECK(ref == expected);
}
static void checkTrimLeft(const std::string& s, const char* chars, const std::string& expected)
{
	std::string test = s;
	trimLeft(test, chars);
	CHECK(test == expected);

	std::string_view ref = s;
	trimLeft(ref, chars);
	CHECK(ref == expected);
}

static void checkSplitOnFirst(const std::string& s, const std::string& first, const std::string& last)
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

static void checkSplitOnLast(const std::string& s, const std::string& first, const std::string& last)
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

template<StringOp::EmptyParts keepOrRemove, typename Separators>
static void checkSplit(Separators separators, const std::string& s, const std::vector<std::string_view>& expected)
{
	//CHECK(split(s, '-') == expected);

	std::vector<std::string_view> result;
	for (const auto& ss : StringOp::split_view<keepOrRemove>(s, separators)) {
		result.push_back(ss);
	}
	CHECK(result == expected);
}

static void checkParseRange(const std::string& s, const std::vector<unsigned>& expected)
{
	auto parsed = parseRange(s, 0, 63);
	std::vector<unsigned> result;
	parsed.foreachSetBit([&](size_t i) { result.push_back(narrow<unsigned>(i)); });
	CHECK(result == expected);
}


TEST_CASE("StringOp")
{
	SECTION("stringTo<int>") {
		std::optional<int> NOK;
		using OK = std::optional<int>;

		// empty string is invalid
		CHECK(StringOp::stringTo<int>("") == NOK);

		// valid decimal values, positive ..
		CHECK(StringOp::stringTo<int>("0") == OK(0));
		CHECK(StringOp::stringTo<int>("03") == OK(3));
		CHECK(StringOp::stringTo<int>("097") == OK(97));
		CHECK(StringOp::stringTo<int>("12") == OK(12));
		// .. and negative
		CHECK(StringOp::stringTo<int>("-0") == OK(0));
		CHECK(StringOp::stringTo<int>("-11") == OK(-11));

		// invalid
		CHECK(StringOp::stringTo<int>("-") == NOK);
		CHECK(StringOp::stringTo<int>("zz") == NOK);
		CHECK(StringOp::stringTo<int>("+") == NOK);
		CHECK(StringOp::stringTo<int>("+12") == NOK);

		// leading whitespace is invalid, trailing stuff is invalid
		CHECK(StringOp::stringTo<int>(" 14") == NOK);
		CHECK(StringOp::stringTo<int>("15 ") == NOK);
		CHECK(StringOp::stringTo<int>("15bar") == NOK);

		// hexadecimal
		CHECK(StringOp::stringTo<int>("0x1a") == OK(26));
		CHECK(StringOp::stringTo<int>("0x1B") == OK(27));
		CHECK(StringOp::stringTo<int>("0X1c") == OK(28));
		CHECK(StringOp::stringTo<int>("0X1D") == OK(29));
		CHECK(StringOp::stringTo<int>("-0x100") == OK(-256));
		CHECK(StringOp::stringTo<int>("0x") == NOK);
		CHECK(StringOp::stringTo<int>("0x12g") == NOK);
		CHECK(StringOp::stringTo<int>("0x-123") == NOK);

		// binary
		CHECK(StringOp::stringTo<int>("0b") == NOK);
		CHECK(StringOp::stringTo<int>("0b2") == NOK);
		CHECK(StringOp::stringTo<int>("0b100") == OK(4));
		CHECK(StringOp::stringTo<int>("-0B1001") == OK(-9));
		CHECK(StringOp::stringTo<int>("0b-11") == NOK);

		// overflow
		CHECK(StringOp::stringTo<int>("-2147483649") == NOK);
		CHECK(StringOp::stringTo<int>("2147483648") == NOK);
		CHECK(StringOp::stringTo<int>("999999999999999") == NOK);
		CHECK(StringOp::stringTo<int>("-999999999999999") == NOK);
		// edge cases (no overflow)
		CHECK(StringOp::stringTo<int>("-2147483648") == OK(-2147483648));
		CHECK(StringOp::stringTo<int>("2147483647") == OK(2147483647));
		CHECK(StringOp::stringTo<int>("-0x80000000") == OK(-2147483648));
		CHECK(StringOp::stringTo<int>("0x7fffffff") == OK(2147483647));
	}
	SECTION("stringTo<unsigned>") {
		std::optional<unsigned> NOK;
		using OK = std::optional<unsigned>;

		// empty string is invalid
		CHECK(StringOp::stringTo<unsigned>("") == NOK);

		// valid decimal values, only positive ..
		CHECK(StringOp::stringTo<unsigned>("0") == OK(0));
		CHECK(StringOp::stringTo<unsigned>("08") == OK(8));
		CHECK(StringOp::stringTo<unsigned>("0123") == OK(123));
		CHECK(StringOp::stringTo<unsigned>("13") == OK(13));
		// negative is invalid
		CHECK(StringOp::stringTo<unsigned>("-0") == NOK);
		CHECK(StringOp::stringTo<unsigned>("-12") == NOK);

		// invalid
		CHECK(StringOp::stringTo<unsigned>("-") == NOK);
		CHECK(StringOp::stringTo<unsigned>("zz") == NOK);
		CHECK(StringOp::stringTo<unsigned>("+") == NOK);
		CHECK(StringOp::stringTo<unsigned>("+12") == NOK);

		// leading whitespace is invalid, trailing stuff is invalid
		CHECK(StringOp::stringTo<unsigned>(" 16") == NOK);
		CHECK(StringOp::stringTo<unsigned>("17 ") == NOK);
		CHECK(StringOp::stringTo<unsigned>("17qux") == NOK);

		// hexadecimal
		CHECK(StringOp::stringTo<unsigned>("0x2a") == OK(42));
		CHECK(StringOp::stringTo<unsigned>("0x2B") == OK(43));
		CHECK(StringOp::stringTo<unsigned>("0X2c") == OK(44));
		CHECK(StringOp::stringTo<unsigned>("0X2D") == OK(45));
		CHECK(StringOp::stringTo<unsigned>("0x") == NOK);
		CHECK(StringOp::stringTo<unsigned>("-0x456") == NOK);
		CHECK(StringOp::stringTo<unsigned>("0x-123") == NOK);

		// binary
		CHECK(StringOp::stringTo<unsigned>("0b1100") == OK(12));
		CHECK(StringOp::stringTo<unsigned>("0B1010") == OK(10));
		CHECK(StringOp::stringTo<unsigned>("0b") == NOK);
		CHECK(StringOp::stringTo<unsigned>("-0b101") == NOK);
		CHECK(StringOp::stringTo<unsigned>("0b2") == NOK);
		CHECK(StringOp::stringTo<unsigned>("0b-11") == NOK);

		// overflow
		CHECK(StringOp::stringTo<unsigned>("4294967296") == NOK);
		CHECK(StringOp::stringTo<unsigned>("999999999999999") == NOK);
		// edge case (no overflow)
		CHECK(StringOp::stringTo<unsigned>("4294967295") == OK(4294967295));
		CHECK(StringOp::stringTo<unsigned>("0xffffffff") == OK(4294967295));
	}

	SECTION("stringToBool") {
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
	}
	/*SECTION("toLower") {
		CHECK(toLower("") == "");
		CHECK(toLower("foo") == "foo");
		CHECK(toLower("FOO") == "foo");
		CHECK(toLower("fOo") == "foo");
		CHECK(toLower(std::string("FoO")) == "foo");
	}*/
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
		using enum StringOp::EmptyParts;
		checkSplit<KEEP>('-', "", {});
		checkSplit<KEEP>('-', "-", {""});
		checkSplit<KEEP>('-', "foo-", {"foo"});
		checkSplit<KEEP>('-', "-foo", {"", "foo"});
		checkSplit<KEEP>('-', "foo-bar", {"foo", "bar"});
		checkSplit<KEEP>('-', "foo-bar-qux", {"foo", "bar", "qux"});
		checkSplit<KEEP>('-', "-bar-qux", {"", "bar", "qux"});
		checkSplit<KEEP>('-', "foo-bar-", {"foo", "bar"});
		checkSplit<KEEP>('-', "foo--bar", {"foo", "", "bar"});
		checkSplit<KEEP>('-', "--foo--bar--", {"", "", "foo", "", "bar", ""});
		checkSplit<KEEP>(" \t", "foo\t\t  bar", {"foo", "", "", "", "bar"});

		checkSplit<REMOVE>('-', "", {});
		checkSplit<REMOVE>('-', "-", {});
		checkSplit<REMOVE>('-', "foo-", {"foo"});
		checkSplit<REMOVE>('-', "-foo", {"foo"});
		checkSplit<REMOVE>('-', "foo-bar", {"foo", "bar"});
		checkSplit<REMOVE>('-', "foo-bar-qux", {"foo", "bar", "qux"});
		checkSplit<REMOVE>('-', "-bar-qux", {"bar", "qux"});
		checkSplit<REMOVE>('-', "foo-bar-", {"foo", "bar"});
		checkSplit<REMOVE>('-', "foo--bar", {"foo", "bar"});
		checkSplit<REMOVE>('-', "--foo--bar--", {"foo", "bar"});
		checkSplit<REMOVE>(" \t", "foo\t\t  bar", {"foo", "bar"});
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
	SECTION("containsCaseInsensitive") {
		CHECK( StringOp::containsCaseInsensitive("abc def", "abc"));
		CHECK( StringOp::containsCaseInsensitive("abc def", "def"));
		CHECK(!StringOp::containsCaseInsensitive("abc def", "xyz"));
		CHECK( StringOp::containsCaseInsensitive("ABC DEF", "abc"));
		CHECK( StringOp::containsCaseInsensitive("ABC DEF", "def"));
		CHECK(!StringOp::containsCaseInsensitive("ABC DEF", "xyz"));
		CHECK( StringOp::containsCaseInsensitive("abc def", "ABC"));
		CHECK( StringOp::containsCaseInsensitive("abc def", "DEF"));
		CHECK(!StringOp::containsCaseInsensitive("abc def", "XYZ"));
		CHECK( StringOp::containsCaseInsensitive("ABC DEF", "ABC"));
		CHECK( StringOp::containsCaseInsensitive("ABC DEF", "DEF"));
		CHECK(!StringOp::containsCaseInsensitive("ABC DEF", "XYZ"));
	}
}
