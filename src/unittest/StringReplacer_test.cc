#include "catch.hpp"
#include "StringReplacer.hh"

TEST_CASE("StringReplacer")
{
	// up-to-4 strings is a special case
	SECTION("no replacements") {
		static constexpr auto replacer = StringReplacer::create();
		CHECK(replacer("") == "");
		CHECK(replacer("foo") == "foo");
		CHECK(replacer("1234") == "1234");
	}
	SECTION("1 replacement") {
		static constexpr auto replacer = StringReplacer::create(
			"foo", "bar");
		CHECK(replacer("") == "");
		CHECK(replacer("foo") == "bar");
		CHECK(replacer("1234") == "1234");
	}
	SECTION("2 replacements") {
		static constexpr auto replacer = StringReplacer::create(
			"foo", "bar",
			"qux", "baz");
		CHECK(replacer("") == "");
		CHECK(replacer("foo") == "bar");
		CHECK(replacer("qux") == "baz");
		CHECK(replacer("Qux") == "Qux"); // case sensitive
		CHECK(replacer("1234") == "1234");
	}
	SECTION("3 replacements") {
		static constexpr auto replacer = StringReplacer::create(
			"foo", "bar",
			"qux", "baz",
			"a", "b");
		CHECK(replacer("") == "");
		CHECK(replacer("foo") == "bar");
		CHECK(replacer("qux") == "baz");
		CHECK(replacer("a") == "b");
		CHECK(replacer("1234") == "1234");
	}
	SECTION("4 replacements") {
		static constexpr auto replacer = StringReplacer::create(
			"foo", "bar",
			"qux", "baz",
			"a", "b",
			"1", "one");
		CHECK(replacer("") == "");
		CHECK(replacer("foo") == "bar");
		CHECK(replacer("qux") == "baz");
		CHECK(replacer("a") == "b");
		CHECK(replacer("1") == "one");
		CHECK(replacer("1234") == "1234");
	}
	// more than 4 switches to a different internal implementation
	SECTION("many replacements") {
		static constexpr auto replacer = StringReplacer::create(
			"1", "one",
			"22", "two",
			"333", "three",
			"4444", "four",
			"5", "five",
			"6", "six",
			"777", "seven",
			"8", "eight",
			"9", "nine",
			"10101010", "ten");
		CHECK(replacer("1") == "one");
		CHECK(replacer("22") == "two");
		CHECK(replacer("333") == "three");
		CHECK(replacer("4444") == "four");
		CHECK(replacer("5") == "five");
		CHECK(replacer("6") == "six");
		CHECK(replacer("777") == "seven");
		CHECK(replacer("8") == "eight");
		CHECK(replacer("9") == "nine");
		CHECK(replacer("10101010") == "ten");

		CHECK(replacer("") == "");
		CHECK(replacer("foo") == "foo");
		CHECK(replacer("11") == "11");
		CHECK(replacer("7") == "7");
	}
}
