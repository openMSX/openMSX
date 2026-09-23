#include "catch.hpp"
#include "VectorPathDsl.hh"
#include "VectorPath.hh"

#include <array>
#include <cstdint>
#include <span>

using namespace VectorPath;
using namespace VectorPath::dsl;

template<size_t N>
static void checkEqualBytes(std::span<const uint8_t> actual,
                            const std::array<uint8_t, N>& expected)
{
	CHECK(actual.size() == N);
	for (size_t i = 0; i < N; ++i) {
		CHECK(actual[i] == expected[i]);
	}
}


TEST_CASE("path_single_line_int16")
{
	static constexpr auto blob = path<int16_t>(
		from(10, 20),
		line(30, 40));

	static constexpr std::array<uint8_t, 10> expected = {
		1,                   // segment count
		10, 0, 20, 0,        // anchor
		0,                   // flags: line
		30, 0, 40, 0,        // line endpoint
	};
	checkEqualBytes(std::span(blob), expected);

	ByteReader reader(blob);
	CHECK(reader.get<uint8_t>() ==  1); // segment count
	CHECK(reader.get<int16_t>() == 10); // anchor x
	CHECK(reader.get<int16_t>() == 20); // anchor y
	CHECK(reader.get<uint8_t>() ==  0); // segment flags
	CHECK(reader.get<int16_t>() == 30); // line x
	CHECK(reader.get<int16_t>() == 40); // line y
	CHECK(reader.empty());              // fully consumed
}

TEST_CASE("path_cubic_sets_flag_bit")
{
	static constexpr auto blob = path<int16_t>(
		from(0, 0),
		curve(1, 2, 3, 4, 5, 6));

	ByteReader reader(blob);
	CHECK(reader.get<uint8_t>() == 1); // segment count
	CHECK(reader.get<int16_t>() == 0); // anchor x
	CHECK(reader.get<int16_t>() == 0); // anchor y
	CHECK(reader.get<uint8_t>() == 1); // cubic flag bit 0
	CHECK(reader.get<int16_t>() == 1); // cx1
	CHECK(reader.get<int16_t>() == 2); // cy1
	CHECK(reader.get<int16_t>() == 3); // cx2
	CHECK(reader.get<int16_t>() == 4); // cy2
	CHECK(reader.get<int16_t>() == 5); // x
	CHECK(reader.get<int16_t>() == 6); // y
	CHECK(reader.empty());             // fully consumed
}

TEST_CASE("path_mixed_segments_and_second_flag_byte")
{
	static constexpr auto blob = path<int16_t>(
		from(0, 0),
		line(1, 1), line(2, 2), line(3, 3), line(4, 4),
		line(5, 5), line(6, 6), line(7, 7), line(8, 8),
		curve(9, 9, 10, 10, 11, 11));

	CHECK(blob.size() == 1 + 4 + 1 + 8 * 4 + 1 + 12); // nine-segment path size

	ByteReader reader(blob);
	CHECK(reader.get<uint8_t>() == 9); // segment count
	(void)reader.get<int16_t>();
	(void)reader.get<int16_t>(); // anchor
	CHECK(reader.get<uint8_t>() == 0); // first flag byte: all lines
	for (int i = 0; i < 8; ++i) {
		(void)reader.get<int16_t>();
		(void)reader.get<int16_t>();
	}
	CHECK(reader.get<uint8_t>() == 1); // second flag byte: cubic
	CHECK(reader.get<int16_t>() == 9); // curve start
	CHECK(reader.empty() == false); // curve not finished
}

TEST_CASE("path_int8_coordinates")
{
	static constexpr auto blob = path<int8_t>(
		from(1, 2),
		line(3, 4));

	CHECK(blob.size() == 1 + 2 + 1 + 2); // int8 path size

	ByteReader reader(blob);
	CHECK(reader.get<uint8_t>() == 1); // segment count
	CHECK(reader.get<int8_t>()  == 1); // anchor x
	CHECK(reader.get<int8_t>()  == 2); // anchor y
	CHECK(reader.get<uint8_t>() == 0); // flags
	CHECK(reader.get<int8_t>()  == 3); // line x
	CHECK(reader.get<int8_t>()  == 4); // line y
	CHECK(reader.empty());             // fully consumed
}

TEST_CASE("stroke_collection_open_and_closed")
{
	static constexpr auto blob = strokeCollection<int16_t>(
		openPath(from(0, 0), line(1, 1)),
		closedPath(from(2, 2), line(3, 3), line(4, 4)));

	static constexpr size_t path0Size = 1 + 4 + 1 + 4;
	static constexpr size_t path1Size = 1 + 4 + 1 + 4 + 4;
	CHECK(blob.size() == 1 + 1 + path0Size + path1Size); // collection size

	ByteReader reader(blob);
	CHECK(reader.get<uint8_t>() == 2); // path count
	CHECK(reader.get<uint8_t>() == 2); // open=0, closed=1 -> bit 1 set

	// path 0
	CHECK(reader.get<uint8_t>() == 1); // path0 segment count
	CHECK(reader.get<int16_t>() == 0); // path0 anchor x
	CHECK(reader.get<int16_t>() == 0); // path0 anchor y
	CHECK(reader.get<uint8_t>() == 0); // path0 flags
	CHECK(reader.get<int16_t>() == 1); // path0 line x
	CHECK(reader.get<int16_t>() == 1); // path0 line y

	// path 1
	CHECK(reader.get<uint8_t>() == 2); // path1 segment count
	CHECK(reader.get<int16_t>() == 2); // path1 anchor x
	CHECK(reader.get<int16_t>() == 2); // path1 anchor y
	CHECK(reader.get<uint8_t>() == 0); // path1 flags
	CHECK(reader.get<int16_t>() == 3); // path1 line1 x
	CHECK(reader.get<int16_t>() == 3); // path1 line1 y
	CHECK(reader.get<int16_t>() == 4); // path1 line2 x
	CHECK(reader.get<int16_t>() == 4); // path1 line2 y
	CHECK(reader.empty());             // collection fully consumed
}

TEST_CASE("stroke_collection_nine_paths_second_stroke_flag_byte")
{
	static constexpr auto blob = strokeCollection<int16_t>(
		openPath(from(0, 0), line(1, 1)),
		openPath(from(0, 0), line(1, 1)),
		openPath(from(0, 0), line(1, 1)),
		openPath(from(0, 0), line(1, 1)),
		openPath(from(0, 0), line(1, 1)),
		openPath(from(0, 0), line(1, 1)),
		openPath(from(0, 0), line(1, 1)),
		openPath(from(0, 0), line(1, 1)),
		closedPath(from(0, 0), line(1, 1)));

	constexpr size_t pathSize = 1 + 4 + 1 + 4;
	CHECK(blob.size() == 1 + 2 + 9 * pathSize); // nine-path collection size

	ByteReader reader(blob);
	CHECK(reader.get<uint8_t>() == 9); // path count
	CHECK(reader.get<uint8_t>() == 0); // first stroke-flag byte
	CHECK(reader.get<uint8_t>() == 1); // second stroke-flag byte: path 8 closed
}

TEST_CASE("compile_time_path_sizes")
{
	static constexpr auto linePath = path<int16_t>(from(0, 0), line(1, 1));
	static constexpr auto curvePath = path<int16_t>(
		from(0, 0), curve(1, 2, 3, 4, 5, 6));
	static_assert(linePath.size() == 10);
	static_assert(curvePath.size() == 1 + 4 + 1 + 12);
}
