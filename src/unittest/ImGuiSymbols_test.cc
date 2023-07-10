#include "catch.hpp"
#include "ImGuiSymbols.hh"


static void test1(std::string_view buffer, std::string_view label, uint16_t value)
{
	auto symbols = openmsx::parseSymbolBuffer("some-filename", buffer);
	REQUIRE(symbols.size() == 1);
	auto& sym = symbols.front();

	CHECK(sym.file == "some-filename");
	CHECK(sym.name == label);
	CHECK(sym.value == value);
}

static void error1(std::string_view buffer)
{
	auto symbols = openmsx::parseSymbolBuffer("some-filename", buffer);
	CHECK(symbols.empty());
}

TEST_CASE("parseSymbolBuffer")
{
	SECTION("various value formats") {
		SECTION("ok") {
			// hex
			test1("bla equ 0x1234", "bla", 0x1234);
			test1("bla equ 0XABCD", "bla", 0xabcd);
			test1("bla equ $fedc",  "bla", 0xfedc);
			test1("bla equ #11aA",  "bla", 0x11aa);
			test1("bla equ bbffh",  "bla", 0xbbff);
			test1("bla equ 3210H",  "bla", 0x3210);
			test1("bla equ 01234h", "bla", 0x1234);
			// dec
			test1("bla equ 123",    "bla", 0x007b);
			test1("bla equ 65535",  "bla", 0xffff);
			test1("bla equ 0020",   "bla", 0x0014); // NOT interpreted as octal
			// bin
			test1("bla equ %1100",              "bla", 0x000c);
			test1("bla equ 0b000100100011",     "bla", 0x0123);
			test1("bla equ 0B1111000001011000", "bla", 0xf058);
		}
		SECTION("error") {
			// wrong format
			error1("bla equ 0xFEDX");
			error1("bla equ 1234a");
			error1("bla equ -3");
			error1("bla equ 0b00112110");
			// overflow
			error1("bla equ 0x10000");
			error1("bla equ 65536");
			error1("bla equ %11110000111100001");
		}
	}
	SECTION("labels") {
		test1("foo equ 1",  "foo", 1);
		test1("Bar equ 2",  "Bar", 2); // preserve case
		test1("QUX: equ 3", "QUX", 3); // strip colon
	}
	SECTION("various formats") {
		SECTION("ok") {
			test1("frmt equ 1",  "frmt", 1);
			test1("frmt %equ 2", "frmt", 2);
			test1("def frmt 3",  "frmt", 3);
		}
		SECTION("error") {
			error1("frmt equals 1"); // must be exactly 'equ', not just start with it
			error1("frmt %equ 1 extra"); // must have exactly 3 parts
		}
	}
	SECTION("white space") {
		test1("  ws equ 1", "ws", 1); // in front
		test1("ws equ 2  ", "ws", 2); // behind
		test1("ws   equ   3", "ws", 3); // middle
		test1("ws\t   equ\t3  \t  \t", "ws", 3); // tab
	}
	SECTION("comments") {
		test1("cmnt equ 1 ; bla bla", "cmnt", 1);
	}
	SECTION("multi") {
		std::string_view buffer =
			"; start with a comment\n"
			"\n" // empty line
			"bla equ 1\n"
			"error equals 2\n" // error
			"foo: %equ 3\n"
			"def bar %100 ; comment\n"
			"this is an error\n" // error
			"\n"
			"Qux:\tequ\t0x5\n";
		auto symbols = openmsx::parseSymbolBuffer("some-filename", buffer);
		REQUIRE(symbols.size() == 4);
		CHECK(symbols[0].value == 1); // bla
		CHECK(symbols[1].value == 3); // foo
		CHECK(symbols[2].value == 4); // bar
		CHECK(symbols[3].value == 5); // Qux
	}
}
