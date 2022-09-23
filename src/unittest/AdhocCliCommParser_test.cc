#include "catch.hpp"
#include "AdhocCliCommParser.hh"
#include <string>
#include <vector>

using namespace std;

static vector<string> parse(const string& stream)
{
	vector<string> result;
	AdhocCliCommParser parser([&](const string& cmd) { result.push_back(cmd); });
	parser.parse(stream);
	return result;
}

TEST_CASE("AdhocCliCommParser")
{
	SECTION("whitespace") {
		CHECK(parse("<command>foo</command>") ==
		      vector<string>{"foo"});
		CHECK(parse(" \t<command>foo</command>") ==
		      vector<string>{"foo"});
		CHECK(parse("<command>foo</command>\n ") ==
		      vector<string>{"foo"});
		CHECK(parse("  \n\n<command>foo</command> \t\n ") ==
		      vector<string>{"foo"});
	}
	SECTION("multiple commands") {
		CHECK(parse("<command>foo</command><command>bar</command>") ==
		      vector<string>{"foo", "bar"});
		CHECK(parse("<command>foo</command>  <command>bar</command>") ==
		      vector<string>{"foo", "bar"});
	}
	SECTION("XML-entities") {
		CHECK(parse("<command>&amp;</command>") ==
		      vector<string>{"&"});
		CHECK(parse("<command>&apos;</command>") ==
		      vector<string>{"'"});
		CHECK(parse("<command>&quot;</command>") ==
		      vector<string>{"\""});
		CHECK(parse("<command>&lt;</command>") ==
		      vector<string>{"<"});
		CHECK(parse("<command>&gt;</command>") ==
		      vector<string>{">"});
		CHECK(parse("<command>&#65;</command>") ==
		      vector<string>{"A"});
		CHECK(parse("<command>&#x41;</command>") ==
		      vector<string>{"A"});
		CHECK(parse("<command>&lt;command&gt;</command>") ==
		      vector<string>{"<command>"});
	}
	SECTION("tags other than <command> are ignored") {
		CHECK(parse("<openmsx-control> <command>foo</command> </openmsx-control>") ==
		      vector<string>{"foo"});
		CHECK(parse("<openmsx-control> <unknown>foo</unknown> <command>foo</command>") ==
		      vector<string>{"foo"});
	}
	SECTION("errors, but we do recover") {
		CHECK(parse("<command>&unknown;</command><command>foo</command>") ==
		      vector<string>{"foo"});
		CHECK(parse("<command>&#fffffff;</command><command>foo</command>") ==
		      vector<string>{"foo"});
		CHECK(parse("<<<<<command>foo</command>") ==
		      vector<string>{"foo"});
		CHECK(parse("<foo></bar><command>foo</command>") ==
		      vector<string>{"foo"});
		CHECK(parse("</command><command>foo</command>") ==
		      vector<string>{"foo"});
		CHECK(parse("<command>bar</foobar><command>foo</command>") ==
		      vector<string>{"foo"});
	}
	SECTION("old (XML) parser didn't accept this, AdhocCliCommParser does") {
		// new parser ignores the <openmsx-control> tag
		CHECK(parse("<openmsx-control></openmsx-control><command>foo</command>") ==
		      vector<string>{"foo"});
	}
	SECTION("old (XML) parser did accept this, AdhocCliCommParser does not") {
		// attributes are no longer accepted
		CHECK(parse("<command value=\"3\">foo</command>") ==
		      vector<string>{});
		// tags nested in the <command> tag are no longer accepted
		CHECK(parse("<command><bla></bla>foo</command>") ==
		      vector<string>{});
		// The shorthand notation for an empty tag is no longer recognized:
		//   sort-of OK, was before an empty command, now no command
		CHECK(parse("<command/>") ==
		      vector<string>{});
	}
}
