#include "catch.hpp"
#include "ConsoleLine.hh"
#include "ranges.hh"

using namespace openmsx;

static void invariants(const ConsoleLine& line)
{
	const auto& str = line.str();
	const auto& chunks = line.getChunks();

	CHECK(ranges::is_sorted(chunks, {}, &ConsoleLine::Chunk::pos));
	if (!str.empty()) {
		REQUIRE(!chunks.empty());
		CHECK(chunks.front().pos == 0);
		CHECK(chunks.back().pos <= str.size());
	} else {
		if (!chunks.empty()) {
			CHECK(chunks.back().pos == 0);
		}
	}
}

static void invariants(const ConsoleLine& line1, const ConsoleLine& line2)
{
	invariants(line1);
	invariants(line2);
}

static void isEmpty(const ConsoleLine& line)
{
	CHECK(line.str().empty());
	CHECK(line.getChunks().empty());
}

static void checkString(const ConsoleLine& line1, const ConsoleLine& line2)
{
	auto full = line1.str() + line2.str();
	CHECK(full == "abcdefghijklmnopqrstuvwxyz");
}

TEST_CASE("ConsoleLine")
{
	SECTION("empty") {
		ConsoleLine line;
		SECTION("at-end") {
			auto rest = line.splitAtColumn(0);
			invariants(line, rest);
			isEmpty(line);
			isEmpty(rest);
		}
		SECTION("past-end") {
			auto rest = line.splitAtColumn(10);
			invariants(line, rest);
			isEmpty(line);
			isEmpty(rest);
		}
	}
	SECTION("single chunk") {
		ConsoleLine line("abcdefghijklmnopqrstuvwxyz");
		SECTION("front") {
			auto rest = line.splitAtColumn(0);
			invariants(line, rest);
			checkString(line, rest);
			isEmpty(line);
			CHECK(rest.str().size() == 26);
			CHECK(rest.getChunks().size() == 1);
		}
		SECTION("middle") {
			auto rest = line.splitAtColumn(13);
			invariants(line, rest);
			checkString(line, rest);
			CHECK(line.str().size() == 13);
			CHECK(line.getChunks().size() == 1);
			CHECK(rest.str().size() == 13);
			CHECK(rest.getChunks().size() == 1);
		}
		SECTION("end") {
			auto rest = line.splitAtColumn(26);
			invariants(line, rest);
			checkString(line, rest);
			CHECK(line.str().size() == 26);
			CHECK(line.getChunks().size() == 1);
			isEmpty(rest);
		}
		SECTION("past") {
			auto rest = line.splitAtColumn(40);
			invariants(line, rest);
			checkString(line, rest);
			CHECK(line.str().size() == 26);
			CHECK(line.getChunks().size() == 1);
			isEmpty(rest);
		}
	}
	SECTION("multiple chunks") {
		ConsoleLine line;
		line.addChunk("abcdef",       101); // [0..6)
		line.addChunk("ghijklmnopqr", 102); // [6..18)
		line.addChunk("stuvwxyz",     103); // [16..26)
		SECTION("front") {
			auto rest = line.splitAtColumn(0);
			invariants(line, rest);
			checkString(line, rest);
			isEmpty(line);
			CHECK(rest.str().size() == 26);
			CHECK(rest.getChunks().size() == 3);
			CHECK(rest.getChunks()[0].rgb == 101);
			CHECK(rest.getChunks()[0].pos == 0);
			CHECK(rest.getChunks()[1].rgb == 102);
			CHECK(rest.getChunks()[1].pos == 6);
			CHECK(rest.getChunks()[2].rgb == 103);
			CHECK(rest.getChunks()[2].pos == 18);
		}
		SECTION("split 1st chunk") {
			auto rest = line.splitAtColumn(3);
			invariants(line, rest);
			checkString(line, rest);
			CHECK(line.str().size() == 3);
			CHECK(line.getChunks().size() == 1);
			CHECK(line.getChunks()[0].rgb == 101);
			CHECK(line.getChunks()[0].pos == 0);
			CHECK(rest.str().size() == 23);
			CHECK(rest.getChunks().size() == 3);
			CHECK(rest.getChunks()[0].rgb == 101);
			CHECK(rest.getChunks()[0].pos == 0);
			CHECK(rest.getChunks()[1].rgb == 102);
			CHECK(rest.getChunks()[1].pos == 3);
			CHECK(rest.getChunks()[2].rgb == 103);
			CHECK(rest.getChunks()[2].pos == 15);
		}
		SECTION("at 1st chunk") {
			auto rest = line.splitAtColumn(6);
			invariants(line, rest);
			checkString(line, rest);
			CHECK(line.str().size() == 6);
			CHECK(line.getChunks().size() == 1);
			CHECK(line.getChunks()[0].rgb == 101);
			CHECK(line.getChunks()[0].pos == 0);
			CHECK(rest.str().size() == 20);
			CHECK(rest.getChunks().size() == 2);
			CHECK(rest.getChunks()[0].rgb == 102);
			CHECK(rest.getChunks()[0].pos == 0);
			CHECK(rest.getChunks()[1].rgb == 103);
			CHECK(rest.getChunks()[1].pos == 12);
		}
		SECTION("split 2nd chunk") {
			auto rest = line.splitAtColumn(13);
			invariants(line, rest);
			checkString(line, rest);
			CHECK(line.str().size() == 13);
			CHECK(line.getChunks().size() == 2);
			CHECK(line.getChunks()[0].rgb == 101);
			CHECK(line.getChunks()[0].pos == 0);
			CHECK(line.getChunks()[1].rgb == 102);
			CHECK(line.getChunks()[1].pos == 6);
			CHECK(rest.str().size() == 13);
			CHECK(rest.getChunks().size() == 2);
			CHECK(rest.getChunks()[0].rgb == 102);
			CHECK(rest.getChunks()[0].pos == 0);
			CHECK(rest.getChunks()[1].rgb == 103);
			CHECK(rest.getChunks()[1].pos == 5);
		}
		SECTION("at 2nd chunk") {
			auto rest = line.splitAtColumn(18);
			invariants(line, rest);
			checkString(line, rest);
			CHECK(line.str().size() == 18);
			CHECK(line.getChunks().size() == 2);
			CHECK(line.getChunks()[0].rgb == 101);
			CHECK(line.getChunks()[0].pos == 0);
			CHECK(line.getChunks()[1].rgb == 102);
			CHECK(line.getChunks()[1].pos == 6);
			CHECK(rest.str().size() == 8);
			CHECK(rest.getChunks().size() == 1);
			CHECK(rest.getChunks()[0].rgb == 103);
			CHECK(rest.getChunks()[0].pos == 0);
		}
		SECTION("split 3rd chunk") {
			auto rest = line.splitAtColumn(20);
			invariants(line, rest);
			checkString(line, rest);
			CHECK(line.str().size() == 20);
			CHECK(line.getChunks().size() == 3);
			CHECK(line.getChunks()[0].rgb == 101);
			CHECK(line.getChunks()[0].pos == 0);
			CHECK(line.getChunks()[1].rgb == 102);
			CHECK(line.getChunks()[1].pos == 6);
			CHECK(line.getChunks()[2].rgb == 103);
			CHECK(line.getChunks()[2].pos == 18);
			CHECK(rest.str().size() == 6);
			CHECK(rest.getChunks().size() == 1);
			CHECK(rest.getChunks()[0].rgb == 103);
			CHECK(rest.getChunks()[0].pos == 0);
		}
		SECTION("at 3rd chunk") {
			auto rest = line.splitAtColumn(26);
			invariants(line, rest);
			checkString(line, rest);
			CHECK(line.str().size() == 26);
			CHECK(line.getChunks().size() == 3);
			CHECK(line.getChunks()[0].rgb == 101);
			CHECK(line.getChunks()[0].pos == 0);
			CHECK(line.getChunks()[1].rgb == 102);
			CHECK(line.getChunks()[1].pos == 6);
			CHECK(line.getChunks()[2].rgb == 103);
			CHECK(line.getChunks()[2].pos == 18);
			isEmpty(rest);
		}
		SECTION("past end") {
			auto rest = line.splitAtColumn(40);
			invariants(line, rest);
			checkString(line, rest);
			CHECK(line.str().size() == 26);
			CHECK(line.getChunks().size() == 3);
			CHECK(line.getChunks()[0].rgb == 101);
			CHECK(line.getChunks()[0].pos == 0);
			CHECK(line.getChunks()[1].rgb == 102);
			CHECK(line.getChunks()[1].pos == 6);
			CHECK(line.getChunks()[2].rgb == 103);
			CHECK(line.getChunks()[2].pos == 18);
			isEmpty(rest);
		}
	}
}
