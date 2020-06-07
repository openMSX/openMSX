#include "catch.hpp"
#include "TclArgParser.hh"

#include "Interpreter.hh"
#include <optional>

using namespace openmsx;

TEST_CASE("TclArgParser")
{
	Interpreter interp;

	// variables (possibly) filled in by parser
	int int1 = -1;
	std::optional<int> int2;
	double dbl1 = -1.0;
	std::optional<double> dbl2;
	std::string s1;
	std::optional<std::string> s2;
	std::vector<int> ints;
	bool flag = false;

	// description of the parser
	const ArgsInfo table[] = {
		valueArg("-int1", int1),
		valueArg("-int2", int2),
		valueArg("-double1", dbl1),
		valueArg("-double2", dbl2),
		valueArg("-string1", s1),
		valueArg("-string2", s2),
		valueArg("-ints", ints),
		flagArg("-flag", flag),
	};

	SECTION("empty") {
		std::span<const TclObject, 0> in;
		auto out = parseTclArgs(interp, in, table);

		CHECK(out.empty()); // no args
		CHECK(int1 == -1); // other stuff unchanged
		CHECK(!int2);
		CHECK(!flag);
	}
	SECTION("only normal args") {
		TclObject in[] = { TclObject("arg1"), TclObject(2), TclObject(3) };
		auto out = parseTclArgs(interp, in, table);

		CHECK(out.size() == 3);
		CHECK(out[0] == "arg1");
		CHECK(out[1] == "2");
		CHECK(out[2] == "3");
		CHECK(int1 == -1); // other stuff unchanged
		CHECK(!int2);
	}
	SECTION("(regular) integer option") {
		TclObject in[] = { TclObject("-int1"), TclObject(123) };
		auto out = parseTclArgs(interp, in, table);

		CHECK(out.empty()); // no regular args
		CHECK(int1 == 123); // this has a new value
		CHECK(!int2); // other stuff unchanged
	}
	SECTION("(optional) integer option") {
		TclObject in[] = { TclObject("-int2"), TclObject(456) };
		auto out = parseTclArgs(interp, in, table);

		CHECK(out.empty()); // no regular args
		CHECK(int1 == -1); // this is unchanged (or was it explicitly set to -1 ;-)
		CHECK(int2); // with an optional<int> we can check that it was set or not
		CHECK(*int2 == 456);
	}
	SECTION("(regular) double option") {
		TclObject in[] = { TclObject("-double1"), TclObject(2.72) };
		auto out = parseTclArgs(interp, in, table);

		CHECK(out.empty()); // no regular args
		CHECK(dbl1 == 2.72); // this has a new value
	}
	SECTION("(regular) string option") {
		TclObject in[] = { TclObject("-string1"), TclObject("foobar") };
		auto out = parseTclArgs(interp, in, table);

		CHECK(out.empty()); // no regular args
		CHECK(s1 == "foobar"); // this has a new value
	}
	SECTION("flag value") {
		TclObject in[] = { TclObject("-flag") };
		auto out = parseTclArgs(interp, in, table);

		CHECK(out.empty()); // no regular args
		CHECK(flag); // flag was set
	}
	SECTION("multiple options and args") {
		TclObject in[] = { TclObject("bla"), TclObject("-int2"), TclObject(789), TclObject("qwerty"),
		                   TclObject("-double1"), TclObject("6.28"), TclObject("-string2"), TclObject("bar"),
		                   TclObject("zyxwv"), TclObject("-flag"), TclObject("-int1"), TclObject("-30"),
		};
		auto out = parseTclArgs(interp, in, table);

		CHECK(out.size() == 3);
		CHECK(out[0] == "bla");
		CHECK(out[1] == "qwerty");
		CHECK(out[2] == "zyxwv");
		CHECK(int1 == -30);
		CHECK(int2); CHECK(*int2 == 789);
		CHECK(dbl1 == 6.28);
		CHECK(!dbl2);
		CHECK(s1 == "");
		CHECK(s2); CHECK(*s2 == "bar");
		CHECK(flag);
	}
	SECTION("set same option twice") {
		TclObject in[] = { TclObject("-int1"), TclObject(123), TclObject("bla"), TclObject("-int1"), TclObject(234) };
		auto out = parseTclArgs(interp, in, table);

		CHECK(out.size() == 1);
		CHECK(int1 == 234); // take the value of the last option
	}
	SECTION("vector<T> accepts repeated options") {
		TclObject in[] = { TclObject("-ints"), TclObject(11), TclObject("-ints"), TclObject(22) };
		auto out = parseTclArgs(interp, in, table);

		CHECK(out.empty());
		CHECK(ints.size() == 2);
		CHECK(ints[0] == 11);
		CHECK(ints[1] == 22);
	}
	SECTION("no options after --") {
		TclObject in[] = { TclObject("-int1"), TclObject(123), TclObject("--"), TclObject("-int1"), TclObject(234) };
		auto out = parseTclArgs(interp, in, table);

		CHECK(out.size() == 2);
		CHECK(out[0] == "-int1");
		CHECK(out[1] == "234");
		CHECK(int1 == 123); // take the value of the option before --
	}
	SECTION("missing value for option") {
		TclObject in[] = { TclObject("bla"), TclObject("-int1") };
		CHECK_THROWS(parseTclArgs(interp, in, table));
	}
	SECTION("non-integer value for integer-option") {
		TclObject in[] = { TclObject("-int1"), TclObject("bla") };
		CHECK_THROWS(parseTclArgs(interp, in, table));
	}
	SECTION("non-double value for double-option") {
		TclObject in[] = { TclObject("-double2"), TclObject("bla") };
		CHECK_THROWS(parseTclArgs(interp, in, table));
	}
	SECTION("unknown option") {
		TclObject in[] = { TclObject("-bla"), TclObject("bla") };
		CHECK_THROWS(parseTclArgs(interp, in, table));
	}
}
