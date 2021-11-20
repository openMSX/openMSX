#include "catch.hpp"
#include "join.hh"

#include "strCat.hh"
#include "view.hh"
#include <string_view>

TEST_CASE("join: vector<string_view>, char")
{
	auto check = [](const std::vector<std::string_view>& v, const std::string& expected) {
		std::string result = join(v, '-');
		CHECK(result == expected);
	};

	check({}, "");
	check({""}, "");
	check({"foo"}, "foo");
	check({"", ""}, "-");
	check({"foo", ""}, "foo-");
	check({"", "foo"}, "-foo");
	check({"foo", "bar"}, "foo-bar");
	check({"foo", "bar", "qux"}, "foo-bar-qux");
	check({"", "bar", "qux"}, "-bar-qux");
	check({"foo", "bar", ""}, "foo-bar-");
}

TEST_CASE("join: various types")
{
	std::vector<std::string> vs = {"foo", "bar", "qux"};
	std::vector<int> vi = {1, -89, 673, 0};
	const char* ac[] = {"blabla", "xyz", "4567"};

	char sep1 = '-';
	const char* sep2 = ", ";
	std::string sep3 = "<-->";
	int sep4 = 123;

	auto check = [](const auto& range, const auto& sep, const std::string& expected) {
		std::string result1 = join(range, sep);
		CHECK(result1 == expected);

		std::ostringstream ss;
		ss << join(range, sep);
		std::string result2 = ss.str();
		CHECK(result2 == expected);
	};

	check(vs, sep1, "foo-bar-qux");
	check(vs, sep2, "foo, bar, qux");
	check(vs, sep3, "foo<-->bar<-->qux");
	check(vs, sep4, "foo123bar123qux");

	check(vi, sep1, "1--89-673-0");
	check(vi, sep2, "1, -89, 673, 0");
	check(vi, sep3, "1<-->-89<-->673<-->0");
	check(vi, sep4, "1123-891236731230");

	check(ac, sep1, "blabla-xyz-4567");
	check(ac, sep2, "blabla, xyz, 4567");
	check(ac, sep3, "blabla<-->xyz<-->4567");
	check(ac, sep4, "blabla123xyz1234567");

	auto quote = [](auto& s) { return strCat('\'', s, '\''); };
	check(view::transform(vs, quote), ", ", "'foo', 'bar', 'qux'");
	check(view::transform(vi, quote), ", ", "'1', '-89', '673', '0'");
	check(view::transform(ac, quote), ", ", "'blabla', 'xyz', '4567'");
}
