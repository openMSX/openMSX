#include "catch.hpp"
#include "Interpreter.hh"
#include "TclObject.hh"
#include "ranges.hh"
#include "view.hh"
#include <cstdint>
#include <iterator>
#include <string>

using namespace openmsx;

TEST_CASE("TclObject, constructors")
{
	Interpreter interp;
	SECTION("default") {
		TclObject t;
		CHECK(t.getString() == "");
	}
	SECTION("string_view") {
		TclObject t1("foo"); // const char*
		CHECK(t1.getString() == "foo");
		TclObject t2(std::string("bar")); // string
		CHECK(t2.getString() == "bar");
		TclObject t3(std::string_view("qux")); // string_view
		CHECK(t3.getString() == "qux");
	}
	SECTION("bool") {
		TclObject t1(true);
		CHECK(t1.getString() == "1");
		TclObject t2(false);
		CHECK(t2.getString() == "0");
	}
	SECTION("int") {
		TclObject t(42);
		CHECK(t.getString() == "42");
	}
	SECTION("double") {
		TclObject t(6.28);
		CHECK(t.getString() == "6.28");
	}
	SECTION("binary") {
		uint8_t buf[] = {'a', 'b', 'c'};
		TclObject t(std::span<uint8_t>{buf, sizeof(buf)});
		CHECK(t.getString() == "abc");
	}
	SECTION("copy") {
		TclObject t1("bar");
		TclObject t2 = t1;
		CHECK(t1.getString() == "bar");
		CHECK(t2.getString() == "bar");
		t1 = 123;
		CHECK(t1.getString() == "123");
		CHECK(t2.getString() == "bar");
	}
	SECTION("move") {
		TclObject t1("bar");
		TclObject t2 = std::move(t1);
		CHECK(t2.getString() == "bar");
	}
	SECTION("list") {
		TclObject t0 = makeTclList();
		CHECK(t0.getListLength(interp) == 0);

		TclObject t1 = makeTclList(1);
		CHECK(t1.getListLength(interp) == 1);
		CHECK(t1.getListIndex(interp, 0).getInt(interp) == 1);

		TclObject t2 = makeTclList("foo", 2.72);
		CHECK(t2.getListLength(interp) == 2);
		CHECK(t2.getListIndex(interp, 0).getString() == "foo");
		CHECK(t2.getListIndex(interp, 1).getDouble(interp) == 2.72);
	}
	SECTION("dict") {
		TclObject t0 = makeTclDict();
		CHECK(t0.getDictValue(interp, "foo").getString() == "");
		CHECK(t0.getDictValue(interp, "bar").getString() == "");

		TclObject t1 = makeTclDict("foo", true);
		CHECK(t1.getDictValue(interp, "foo").getBoolean(interp) == true);
		CHECK(t1.getDictValue(interp, "bar").getString() == "");

		TclObject t2 = makeTclDict("foo", 123, "bar", "qux");
		CHECK(t2.getDictValue(interp, "foo").getInt(interp) == 123);
		CHECK(t2.getDictValue(interp, "bar").getString() == "qux");
	}
}

TEST_CASE("TclObject, assignment")
{
	Interpreter interp;
	SECTION("copy") {
		TclObject t1(123);
		TclObject t2(987);
		REQUIRE(t1 != t2);
		t2 = t1;
		CHECK(t1 == t2);
		CHECK(t1.getString() == "123");
		CHECK(t2.getString() == "123");
		t1 = 456;
		CHECK(t1 != t2);
		CHECK(t1.getString() == "456");
		CHECK(t2.getString() == "123");
	}
	SECTION("move") {
		TclObject t1(123);
		TclObject t2(987);
		REQUIRE(t1 != t2);
		t2 = std::move(t1);
		CHECK(t2.getString() == "123");
		t1 = 456;
		CHECK(t1 != t2);
		CHECK(t1.getString() == "456");
		CHECK(t2.getString() == "123");
	}
}

// skipped getTclObject() / getTclObjectNonConst()

TEST_CASE("TclObject, operator=")
{
	Interpreter interp;
	TclObject t(123);
	REQUIRE(t.getString() == "123");

	SECTION("string") {
		t = "foo";
		CHECK(t.getString() == "foo");
		t = std::string("bar");
		CHECK(t.getString() == "bar");
		t = std::string_view("qux");
		CHECK(t.getString() == "qux");
	}
	SECTION("int") {
		t = 42;
		CHECK(t.getString() == "42");
	}
	SECTION("bool") {
		t = true;
		CHECK(t.getString() == "1");
		t = false;
		CHECK(t.getString() == "0");
	}
	SECTION("double") {
		t = -3.14;
		CHECK(t.getString() == "-3.14");
	}
	SECTION("binary") {
		std::array<uint8_t, 3> buf = {1, 2, 3};
		t = std::span{buf};
		auto result = t.getBinary();
		CHECK(ranges::equal(buf, result));
		// 'buf' was copied into 't'
		CHECK(result.data() != &buf[0]);
		CHECK(result[0] == 1);
		buf[0] = 99;
		CHECK(result[0] == 1);
	}
	SECTION("copy") {
		TclObject t2(true);
		REQUIRE(t2.getString() == "1");
		t = t2;
		CHECK(t .getString() == "1");
		CHECK(t2.getString() == "1");
		t2 = false;
		CHECK(t .getString() == "1");
		CHECK(t2.getString() == "0");
	}
	SECTION("move") {
		TclObject t2(true);
		REQUIRE(t2.getString() == "1");
		t = std::move(t2);
		CHECK(t .getString() == "1");
		t2 = false;
		CHECK(t .getString() == "1");
		CHECK(t2.getString() == "0");
	}
}

TEST_CASE("TclObject, addListElement")
{
	Interpreter interp;

	SECTION("no error") {
		TclObject t;
		CHECK(t.getListLength(interp) == 0);
		t.addListElement("foo bar");
		CHECK(t.getListLength(interp) == 1);
		t.addListElement(false);
		CHECK(t.getListLength(interp) == 2);
		t.addListElement(33);
		CHECK(t.getListLength(interp) == 3);
		t.addListElement(9.23);
		CHECK(t.getListLength(interp) == 4);
		TclObject t2("bla");
		t.addListElement(t2);
		CHECK(t.getListLength(interp) == 5);
		t.addListElement(std::string("qux"));
		CHECK(t.getListLength(interp) == 6);
		uint8_t buf[] = {'x', 'y', 'z'};
		t.addListElement(std::span<uint8_t>{buf, sizeof(buf)});
		CHECK(t.getListLength(interp) == 7);

		TclObject l0 = t.getListIndex(interp, 0);
		TclObject l1 = t.getListIndex(interp, 1);
		TclObject l2 = t.getListIndex(interp, 2);
		TclObject l3 = t.getListIndex(interp, 3);
		TclObject l4 = t.getListIndex(interp, 4);
		TclObject l5 = t.getListIndex(interp, 5);
		TclObject l6 = t.getListIndex(interp, 6);
		CHECK(l0.getString() == "foo bar");
		CHECK(l1.getString() == "0");
		CHECK(l2.getString() == "33");
		CHECK(l3.getString() == "9.23");
		CHECK(l4.getString() == "bla");
		CHECK(l5.getString() == "qux");
		CHECK(l6.getString() == "xyz");

		CHECK(t.getString() == "{foo bar} 0 33 9.23 bla qux xyz");
	}
	SECTION("error") {
		TclObject t("{foo"); // invalid list representation
		CHECK_THROWS(t.getListLength(interp));
		CHECK_THROWS(t.addListElement(123));
	}
}

TEST_CASE("TclObject, addListElements")
{
	Interpreter interp;
	int ints[] = {7, 6, 5};
	double doubles[] = {1.2, 5.6};

	SECTION("no error") {
		TclObject t;
		CHECK(t.getListLength(interp) == 0);
		// iterator-pair
		t.addListElements(std::begin(ints), std::end(ints));
		CHECK(t.getListLength(interp) == 3);
		CHECK(t.getListIndex(interp, 1).getString() == "6");
		// range (array)
		t.addListElements(doubles);
		CHECK(t.getListLength(interp) == 5);
		CHECK(t.getListIndex(interp, 3).getString() == "1.2");
		// view::transform
		t.addListElements(view::transform(ints, [](int i) { return 2 * i; }));
		CHECK(t.getListLength(interp) == 8);
		CHECK(t.getListIndex(interp, 7).getString() == "10");
		// multiple
		t.addListElement("one", 2, 3.14);
		CHECK(t.getListLength(interp) == 11);
		CHECK(t.getListIndex(interp, 8).getString() == "one");
		CHECK(t.getListIndex(interp, 9).getString() == "2");
		CHECK(t.getListIndex(interp, 10).getString() == "3.14");
	}
	SECTION("error") {
		TclObject t("{foo"); // invalid list representation
		CHECK_THROWS(t.addListElements(std::begin(doubles), std::end(doubles)));
		CHECK_THROWS(t.addListElements(ints));
	}
}

TEST_CASE("TclObject, addDictKeyValue(s)")
{
	Interpreter interp;
	TclObject t;

	t.addDictKeyValue("one", 1);
	CHECK(t.getDictValue(interp, "one").getInt(interp) == 1);
	CHECK(t.getDictValue(interp, "two").getString() == "");
	CHECK(t.getDictValue(interp, "three").getString() == "");

	t.addDictKeyValues("two", 2, "three", 3.14);
	CHECK(t.getDictValue(interp, "one").getInt(interp) == 1);
	CHECK(t.getDictValue(interp, "two").getInt(interp) == 2);
	CHECK(t.getDictValue(interp, "three").getDouble(interp) == 3.14);

	t.addDictKeyValues("four", false, "one", "een");
	CHECK(t.getDictValue(interp, "one").getString() == "een");
	CHECK(t.getDictValue(interp, "two").getInt(interp) == 2);
	CHECK(t.getDictValue(interp, "three").getDouble(interp) == 3.14);
	CHECK(t.getDictValue(interp, "four").getBoolean(interp) == false);
}

// there are no setting functions (yet?) for dicts

TEST_CASE("TclObject, getXXX")
{
	Interpreter interp;
	TclObject t0;
	TclObject t1("Off");
	TclObject t2(1);
	TclObject t3(2.71828);

	SECTION("getString") { // never fails
		CHECK(t0.getString() == "");
		CHECK(t1.getString() == "Off");
		CHECK(t2.getString() == "1");
		CHECK(t3.getString() == "2.71828");
	}
	SECTION("getInt") {
		CHECK_THROWS(t0.getInt(interp));
		CHECK_THROWS(t1.getInt(interp));
		CHECK       (t2.getInt(interp) == 1);
		CHECK_THROWS(t3.getInt(interp));
	}
	SECTION("getBoolean") {
		CHECK_THROWS(t0.getBoolean(interp));
		CHECK       (t1.getBoolean(interp) == false);
		CHECK       (t2.getBoolean(interp) == true);
		CHECK       (t3.getBoolean(interp) == true);
	}
	SECTION("getDouble") {
		CHECK_THROWS(t0.getDouble(interp));
		CHECK_THROWS(t1.getDouble(interp));
		CHECK       (t2.getDouble(interp) == 1.0);
		CHECK       (t3.getDouble(interp) == 2.71828);
	}
}

// getBinary() already tested above
// getListLength and getListIndex() already tested above

TEST_CASE("TclObject, getDictValue")
{
	Interpreter interp;

	SECTION("no error") {
		TclObject t("one 1 two 2.0 three drie");
		CHECK(t.getDictValue(interp, TclObject("two"  )).getString() == "2.0");
		CHECK(t.getDictValue(interp, TclObject("one"  )).getString() == "1");
		CHECK(t.getDictValue(interp, TclObject("three")).getString() == "drie");
		// missing key -> empty string .. can be improved when needed
		CHECK(t.getDictValue(interp, TclObject("four" )).getString() == "");
	}
	SECTION("invalid dict") {
		TclObject t("{foo");
		CHECK_THROWS(t.getDictValue(interp, TclObject("foo")));
	}
}

TEST_CASE("TclObject, STL interface on Tcl list")
{
	Interpreter interp;

	SECTION("empty") {
		TclObject t;
		CHECK(t.size() == 0);
		CHECK(t.empty() == true);
		CHECK(t.begin() == t.end());
	}
	SECTION("not empty") {
		TclObject t("1 1 2 3 5 8 13 21 34 55");
		CHECK(t.size() == 10);
		CHECK(t.empty() == false);
		auto b = t.begin();
		auto e = t.end();
		CHECK(std::distance(b, e) == 10);
		CHECK(*b == "1");
		std::advance(b, 5);
		CHECK(*b == "8");
		++b;
		CHECK(*b == "13");
		std::advance(b, 4);
		CHECK(b == e);
	}
	SECTION("invalid list") {
		// acts as if the list is empty .. can be improved when needed
		TclObject t("{foo bar qux");
		CHECK(t.size() == 0);
		CHECK(t.empty() == true);
		CHECK(t.begin() == t.end());
	}
}

TEST_CASE("TclObject, evalBool")
{
	Interpreter interp;
	CHECK(TclObject("23 == (20 + 3)").evalBool(interp) == true);
	CHECK(TclObject("1 >= (6-2)"    ).evalBool(interp) == false);
	CHECK_THROWS(TclObject("bla").evalBool(interp));
}

TEST_CASE("TclObject, executeCommand")
{
	Interpreter interp;
	CHECK(TclObject("return foobar").executeCommand(interp).getString() == "foobar");
	CHECK(TclObject("set n 2").executeCommand(interp).getString() == "2");
	TclObject cmd("string repeat bla $n");
	CHECK(cmd.executeCommand(interp, true).getString() == "blabla");
	CHECK(TclObject("incr n").executeCommand(interp).getString() == "3");
	CHECK(cmd.executeCommand(interp, true).getString() == "blablabla");

	CHECK_THROWS(TclObject("qux").executeCommand(interp));
}

TEST_CASE("TclObject, operator==, operator!=")
{
	Interpreter interp;
	TclObject t0;
	TclObject t1("foo");
	TclObject t2("bar qux");
	TclObject t3("foo");

	CHECK(  t0 == t0 ); CHECK(!(t0 != t0));
	CHECK(!(t0 == t1)); CHECK(  t0 != t1 );
	CHECK(!(t0 == t2)); CHECK(  t0 != t2 );
	CHECK(!(t0 == t3)); CHECK(  t0 != t3 );
	CHECK(  t1 == t1 ); CHECK(!(t1 != t1));
	CHECK(!(t1 == t2)); CHECK(  t1 != t2 );
	CHECK(  t1 == t3 ); CHECK(!(t1 != t3));
	CHECK(  t2 == t2 ); CHECK(!(t2 != t2));
	CHECK(!(t2 == t3)); CHECK(  t2 != t3 );
	CHECK(  t3 == t3 ); CHECK(!(t3 != t3));

	CHECK(t0 == ""   ); CHECK(!(t0 != ""   )); CHECK(""    == t0); CHECK(!(""    != t0));
	CHECK(t0 != "foo"); CHECK(!(t0 == "foo")); CHECK("foo" != t0); CHECK(!("foo" == t0));
	CHECK(t1 != ""   ); CHECK(!(t1 == ""   )); CHECK(""    != t1); CHECK(!(""    == t1));
	CHECK(t1 == "foo"); CHECK(!(t1 != "foo")); CHECK("foo" == t1); CHECK(!("foo" != t1));
	CHECK(t2 != ""   ); CHECK(!(t2 == ""   )); CHECK(""    != t2); CHECK(!(""    == t2));
	CHECK(t2 != "foo"); CHECK(!(t2 == "foo")); CHECK("foo" != t2); CHECK(!("foo" == t2));
}

// skipped XXTclHasher
