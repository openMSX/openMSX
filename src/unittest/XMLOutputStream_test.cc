#include "catch.hpp"
#include "XMLOutputStream.hh"
#include <sstream>

// Write to a 'stringstream' instead of a file.
// Throws an exception instead of assert'ing on error.
class TestWriter
{
public:
	explicit TestWriter(std::stringstream& ss_)
		: ss(ss_) {}
	void write(std::span<const char> buf) {
		ss.write(buf.data(), buf.size());
	}
	void write1(char c) {
		ss.put(c);
	}
	void check(bool condition) const {
		if (!condition) {
			throw std::logic_error("assertion failed");
		}
	}
private:
	std::stringstream& ss;
};

TEST_CASE("XMLOutputStream: simple")
{
	std::stringstream ss;
	TestWriter writer(ss);
	XMLOutputStream xml(writer);

	SECTION("empty") {
		CHECK(ss.str().empty());
	}
	SECTION("2 nested tags, inner tag empty") {
		xml.begin("abc");
		  xml.begin("def");
		  xml.end("def");
		xml.end("abc");
		CHECK(ss.str() == "<abc>\n  <def/>\n</abc>\n");
	}
	SECTION("2 nested tags, inner tag with data") {
		xml.begin("abc");
		  xml.begin("def");
		    xml.data("foobar");
		  xml.end("def");
		xml.end("abc");
		CHECK(ss.str() == "<abc>\n  <def>foobar</def>\n</abc>\n");
	}
	SECTION("2 nested tags, inner tag with attribute, no data") {
		xml.begin("abc");
		  xml.begin("def");
		    xml.attribute("foo", "bar");
		  xml.end("def");
		xml.end("abc");
		CHECK(ss.str() == "<abc>\n  <def foo=\"bar\"/>\n</abc>\n");
	}
	SECTION("2 nested tags, inner tag with attribute and data") {
		xml.begin("abc");
		  xml.begin("def");
		    xml.attribute("foo", "bar");
		    xml.data("qux");
		  xml.end("def");
		xml.end("abc");
		CHECK(ss.str() == "<abc>\n  <def foo=\"bar\">qux</def>\n</abc>\n");
	}
}

TEST_CASE("XMLOutputStream: complex")
{
	std::stringstream ss;
	TestWriter writer(ss);
	XMLOutputStream xml(writer);

	xml.begin("settings");
	  xml.begin("settings");
	    xml.begin("setting");
	      xml.attribute("id", "noise");
	      xml.data("3.0");
	    xml.end("setting");
	  xml.end("settings");
	  xml.begin("bindings");
	    xml.begin("bind");
	      xml.attribute("key", "keyb F6");
	      xml.data("cycle \"videosource\"");
	    xml.end("bind");
	  xml.end("bindings");
	xml.end("settings");

	std::string expected =
		"<settings>\n"
		"  <settings>\n"
		"    <setting id=\"noise\">3.0</setting>\n"
		"  </settings>\n"
		"  <bindings>\n"
		"    <bind key=\"keyb F6\">cycle &quot;videosource&quot;</bind>\n"
		"  </bindings>\n"
		"</settings>\n";
	CHECK(ss.str() == expected);
}

TEST_CASE("XMLOutputStream: errors")
{
	std::stringstream ss;
	TestWriter writer(ss);
	XMLOutputStream xml(writer);

	SECTION("more end than begin") {
		xml.begin("bla");
		xml.end("bla");
		CHECK_THROWS(xml.end("bla"));
	}
	SECTION("attribute after data") {
		xml.begin("bla");
		  xml.data("qux");
		  CHECK_THROWS(xml.attribute("foo", "bar"));
		//xml.end("bla");
	}
	SECTION("attribute after end") {
		xml.begin("bla");
		  xml.begin("bla");
		  xml.end("bla");
		CHECK_THROWS(xml.attribute("foo", "bar"));
		//xml.end("bla");
	}
	SECTION("begin after data") {
		xml.begin("bla");
		  xml.data("qux");
		  CHECK_THROWS(xml.begin("bla"));
		//  xml.end("bla");
		//xml.end("bla");
	}
	SECTION("data after end") {
		xml.begin("bla");
		  xml.begin("bla");
		  xml.end("bla");
		CHECK_THROWS(xml.data("qux"));
		//xml.end("bla");
	}
	SECTION("data after data") {
		xml.begin("bla");
		  xml.begin("bla");
		    xml.data("bla");
		CHECK_THROWS(xml.data("qux"));
		//xml.end("bla");
		//xml.end("bla");
	}
#ifdef DEBUG // only detected when compiled with -DDEBUG
	SECTION("non-matching begin/end") {
		xml.begin("bla");
		  xml.begin("bla");
		  xml.data("foobar");
		  xml.end("bla");
		CHECK_THROWS(xml.end("err"));
	}
#endif
}
