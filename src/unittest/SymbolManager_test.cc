#include "catch.hpp"
#include "SymbolManager.hh"

using namespace openmsx;

TEST_CASE("SymbolManager: isHexDigit")
{
	CHECK(SymbolManager::isHexDigit('0') == 0);
	CHECK(SymbolManager::isHexDigit('4') == 4);
	CHECK(SymbolManager::isHexDigit('9') == 9);
	CHECK(SymbolManager::isHexDigit('a') == 10);
	CHECK(SymbolManager::isHexDigit('e') == 14);
	CHECK(SymbolManager::isHexDigit('f') == 15);
	CHECK(SymbolManager::isHexDigit('A') == 10);
	CHECK(SymbolManager::isHexDigit('C') == 12);
	CHECK(SymbolManager::isHexDigit('F') == 15);
	CHECK(SymbolManager::isHexDigit('-') == std::nullopt);
	CHECK(SymbolManager::isHexDigit('@') == std::nullopt);
	CHECK(SymbolManager::isHexDigit('G') == std::nullopt);
	CHECK(SymbolManager::isHexDigit('g') == std::nullopt);
}

TEST_CASE("SymbolManager: is4DigitHex")
{
	CHECK(SymbolManager::is4DigitHex("0000") == 0x0000);
	CHECK(SymbolManager::is4DigitHex("12ab") == 0x12ab);
	CHECK(SymbolManager::is4DigitHex("F000") == 0xf000);
	CHECK(SymbolManager::is4DigitHex("fFfF") == 0xffff);
	CHECK(SymbolManager::is4DigitHex("") == std::nullopt);
	CHECK(SymbolManager::is4DigitHex("123") == std::nullopt);
	CHECK(SymbolManager::is4DigitHex("12345") == std::nullopt);
	CHECK(SymbolManager::is4DigitHex("abg1") == std::nullopt);
}

TEST_CASE("SymbolManager: parseValue")
{
	SECTION("ok") {
		// hex
		CHECK(SymbolManager::parseValue("0xa1") == 0xa1);
		CHECK(SymbolManager::parseValue("0x1234") == 0x1234);
		CHECK(SymbolManager::parseValue("0XABCD") == 0xabcd);
		CHECK(SymbolManager::parseValue("0x00002345") == 0x2345);
		CHECK(SymbolManager::parseValue("$fedc") == 0xfedc);
		CHECK(SymbolManager::parseValue("#11aA") == 0x11aa);
		CHECK(SymbolManager::parseValue("bbffh") == 0xbbff);
		CHECK(SymbolManager::parseValue("3210H") == 0x3210);
		CHECK(SymbolManager::parseValue("01234h") == 0x1234);
		// dec
		CHECK(SymbolManager::parseValue("123") == 0x007b);
		CHECK(SymbolManager::parseValue("65535") == 0xffff);
		CHECK(SymbolManager::parseValue("0020") == 0x0014); // NOT interpreted as octal
		// bin
		CHECK(SymbolManager::parseValue("%1100") == 0x000c);
		CHECK(SymbolManager::parseValue("0b000100100011") == 0x0123);
		CHECK(SymbolManager::parseValue("0B1111000001011000") == 0xf058);
	}
	SECTION("error") {
		// wrong format
		CHECK(SymbolManager::parseValue("0xFEDX") == std::nullopt);
		CHECK(SymbolManager::parseValue("1234a") == std::nullopt);
		CHECK(SymbolManager::parseValue("-3") == std::nullopt);
		CHECK(SymbolManager::parseValue("0b00112110") == std::nullopt);
		// overflow
		CHECK(SymbolManager::parseValue("0x10000") == std::nullopt);
		CHECK(SymbolManager::parseValue("65536") == std::nullopt);
		CHECK(SymbolManager::parseValue("%11110000111100001") == std::nullopt);
	}
}

TEST_CASE("SymbolManager: checkLabel")
{
	CHECK(SymbolManager::checkLabel("foo", 123) == Symbol("foo", 123));
	CHECK(SymbolManager::checkLabel("bar:", 234) == Symbol("bar", 234));
	CHECK(SymbolManager::checkLabel("", 345) == std::nullopt);
	CHECK(SymbolManager::checkLabel(":", 456) == std::nullopt);
}

TEST_CASE("SymbolManager: checkLabelAndValue")
{
	CHECK(SymbolManager::checkLabelAndValue("foo", "123") == Symbol("foo", 123));
	CHECK(SymbolManager::checkLabelAndValue("", "123") == std::nullopt);
	CHECK(SymbolManager::checkLabelAndValue("foo", "bla") == std::nullopt);
}

TEST_CASE("SymbolManager: detectType")
{
	SECTION("on extension") {
		std::string_view buffer = "content doesn't matter";
		CHECK(SymbolManager::detectType("symbols.noi", buffer) == SymbolFile::Type::NOICE);
		CHECK(SymbolManager::detectType("symbols.NOI", buffer) == SymbolFile::Type::NOICE);
		CHECK(SymbolManager::detectType("symbols.Map", buffer) == SymbolFile::Type::LINKMAP);
		CHECK(SymbolManager::detectType("symbols.symbol", buffer) == SymbolFile::Type::GENERIC); // pasmo
		CHECK(SymbolManager::detectType("symbols.publics", buffer) == SymbolFile::Type::GENERIC); // pasmo
		CHECK(SymbolManager::detectType("symbols.sys", buffer) == SymbolFile::Type::GENERIC); // pasmo
		CHECK(SymbolManager::detectType("symbols.unknown", buffer) == SymbolFile::Type::GENERIC); // unknown extension
	}
	SECTION(".sym extension") {
		CHECK(SymbolManager::detectType("myfile.sym", "; Symbol table from myfile.asm") == SymbolFile::Type::ASMSX);
		CHECK(SymbolManager::detectType("myfile.sym", "bla: %equ 123") == SymbolFile::Type::GENERIC);
		CHECK(SymbolManager::detectType("myfile.sym", "bla: equ 123") == SymbolFile::Type::GENERIC);
		CHECK(SymbolManager::detectType("myfile.sym", "bla equ #123") == SymbolFile::Type::GENERIC);
		CHECK(SymbolManager::detectType("myfile.sym", "Sections:") == SymbolFile::Type::VASM);
		CHECK(SymbolManager::detectType("myfile.sym", "anything else") == SymbolFile::Type::HTC);
	}
}

TEST_CASE("SymbolManager: loadLines")
{
	auto dummyParser = [](std::span<std::string_view> tokens) -> std::optional<Symbol> {
		if (tokens.size() == 1) return Symbol{std::string(tokens[0]), 123};
		return {};
	};

	SECTION("empty file") {
		std::string_view buffer;
		auto file = SymbolManager::loadLines("file.sym", buffer, SymbolFile::Type::GENERIC, dummyParser);
		CHECK(file.filename == "file.sym");
		CHECK(file.type == SymbolFile::Type::GENERIC);
		CHECK(file.symbols.empty());
	}
	SECTION("comment + symbol") {
		std::string_view buffer =
			"; This is a comment\n"
			"bla\n" // ok for dummy parser
			"foo bla\n" // ignored because 2 tokens
			"bar ; comment on same line\n"; // only 1 token after removing comment
		auto file = SymbolManager::loadLines("file2.sym", buffer, SymbolFile::Type::NOICE, dummyParser);
		CHECK(file.filename == "file2.sym");
		CHECK(file.type == SymbolFile::Type::NOICE);
		REQUIRE(file.symbols.size() == 2);
		CHECK(file.symbols[0].name == "bla");
		CHECK(file.symbols[0].value == 123);
		CHECK(file.symbols[1].name == "bar");
		CHECK(file.symbols[1].value == 123);
	}
}

TEST_CASE("SymbolManager: loadGeneric")
{
	std::string_view buffer =
		"foo: equ 1\n"
		"bar equ 2\n"
		"qux: equ 3 ; comment\n"
		"; only comment\n"
		"error equ\n"
		"error equ 123 extra stuff\n";
	auto file = SymbolManager::loadGeneric("myfile.sym", buffer);
	CHECK(file.filename == "myfile.sym");
	CHECK(file.type == SymbolFile::Type::GENERIC);
	REQUIRE(file.symbols.size() == 3);
	CHECK(file.symbols[0].name == "foo");
	CHECK(file.symbols[0].value == 1);
	CHECK(file.symbols[1].name == "bar");
	CHECK(file.symbols[1].value == 2);
	CHECK(file.symbols[2].name == "qux");
	CHECK(file.symbols[2].value == 3);
}

TEST_CASE("SymbolManager: loadNoICE")
{
	std::string_view buffer =
		"def foo 1\n"
		"def bar 234h ; comment\n"
		"error def 123\n"
		"def error 99 extra stuff\n";
	auto file = SymbolManager::loadNoICE("noice.sym", buffer);
	CHECK(file.filename == "noice.sym");
	CHECK(file.type == SymbolFile::Type::NOICE);
	REQUIRE(file.symbols.size() == 2);
	CHECK(file.symbols[0].name == "foo");
	CHECK(file.symbols[0].value == 1);
	CHECK(file.symbols[1].name == "bar");
	CHECK(file.symbols[1].value == 0x234);
}

TEST_CASE("SymbolManager: loadHTC")
{
	// TODO verify with an actual HTC file
	std::string_view buffer =
		"foo 1234 bla\n"
		"error 1234\n";
	auto file = SymbolManager::loadHTC("htc.sym", buffer);
	CHECK(file.filename == "htc.sym");
	CHECK(file.type == SymbolFile::Type::HTC);
	REQUIRE(file.symbols.size() == 1);
	CHECK(file.symbols[0].name == "foo");
	CHECK(file.symbols[0].value == 0x1234);
}

TEST_CASE("SymbolManager: loadVASM")
{
	std::string_view buffer =
		"12AB ignore\n"
		"Symbols by value:\n"
		"12AB label\n"
		"bla foo\n";
	auto file = SymbolManager::loadVASM("vasm.sym", buffer);
	CHECK(file.filename == "vasm.sym");
	CHECK(file.type == SymbolFile::Type::VASM);
	REQUIRE(file.symbols.size() == 1);
	CHECK(file.symbols[0].name == "label");
	CHECK(file.symbols[0].value == 0x12ab);
}

TEST_CASE("SymbolManager: loadASMSX")
{
	std::string_view buffer =
		"1234h ignore1\n"
		"12h:3456h ignore2\n"
		"; global and local\n"
		"1234h l1\n"
		"12h:abcdh l2\n";
	auto file = SymbolManager::loadASMSX("asmsx.sym", buffer);
	CHECK(file.filename == "asmsx.sym");
	CHECK(file.type == SymbolFile::Type::ASMSX);
	REQUIRE(file.symbols.size() == 2);
	CHECK(file.symbols[0].name == "l1");
	CHECK(file.symbols[0].value == 0x1234);
	CHECK(file.symbols[1].name == "l2");
	CHECK(file.symbols[1].value == 0xabcd);
}

TEST_CASE("SymbolManager: loadLinkMap")
{
	std::string_view buffer =
		"ignore1             text    2CE7  ignore2                   0AEE\n"
		"ignore3                     2E58  ignore3           text    2E4C\n"
		"         Symbol Table\n"
		"asllmod            text    2CE7  asllsub                    0AEE\n"
		"cret                       2E58  csv                text    2E4C\n"
		"single             text    9876\n"
		"last                       8765\n";
	auto file = SymbolManager::loadLinkMap("link.map", buffer);
	CHECK(file.filename == "link.map");
	CHECK(file.type == SymbolFile::Type::LINKMAP);
	REQUIRE(file.symbols.size() == 6);
	CHECK(file.symbols[0].name == "asllmod");
	CHECK(file.symbols[0].value == 0x2ce7);
	CHECK(file.symbols[1].name == "asllsub");
	CHECK(file.symbols[1].value == 0x0aee);
	CHECK(file.symbols[2].name == "cret");
	CHECK(file.symbols[2].value == 0x2e58);
	CHECK(file.symbols[3].name == "csv");
	CHECK(file.symbols[3].value == 0x2e4c);
	CHECK(file.symbols[4].name == "single");
	CHECK(file.symbols[4].value == 0x9876);
	CHECK(file.symbols[5].name == "last");
	CHECK(file.symbols[5].value == 0x8765);
}
