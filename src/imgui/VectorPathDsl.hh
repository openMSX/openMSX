#ifndef VECTOR_PATH_DSL_HH
#define VECTOR_PATH_DSL_HH

// Compile-time DSL for building VectorPath wire-format byte arrays.
//
// See VectorPath.hh for the binary layout and draw API. This header is
// independent, it only produces std::array<uint8_t, N> blobs.
//
// Example:
//   using namespace VectorPath::dsl;
//   static constexpr auto outline = path<int16_t>(
//       from(10, 20),
//       line(30, 40),
//       curve(12, 13, 14, 15, 16, 17));
//   static constexpr auto panel = strokeCollection<int16_t>(
//       openPath(from(0, 0), line(10, 0)),
//       closedPath(from(0, 0), line(10, 0), line(10, 10), line(0, 10)));

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace VectorPath::dsl {

// --- Segment types ---

template<typename T>
struct From {
	T x, y;
};

template<typename T>
struct Line {
	T x, y;
};

template<typename T>
struct Curve {
	T cx1, cy1, cx2, cy2, x, y;
};

template<typename Anchor, typename... Segs>
struct OpenStrokePath {
	Anchor anchor;
	std::tuple<Segs...> segments;
};

template<typename Anchor, typename... Segs>
struct ClosedStrokePath {
	Anchor anchor;
	std::tuple<Segs...> segments;
};

// --- Segment factories ---

template<typename T>
[[nodiscard]] consteval From<T> from(T x, T y)
{
	return {x, y};
}

template<typename T>
[[nodiscard]] consteval Line<T> line(T x, T y)
{
	return {x, y};
}

template<typename T>
[[nodiscard]] consteval Curve<T> curve(T cx1, T cy1, T cx2, T cy2, T x, T y)
{
	return {cx1, cy1, cx2, cy2, x, y};
}

namespace detail {

class ByteWriter {
public:
	consteval explicit ByteWriter(std::span<uint8_t> data_)
		: data(data_) {}

	template<typename T>
	consteval void put(T value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		if (pos + sizeof(T) > data.size()) {
			throw "VectorPath DSL: buffer overflow";
		}
		auto a = std::bit_cast<std::array<uint8_t, sizeof(T)>>(value);
		std::copy_n(a.data(), a.size(), &data[pos]);
		pos += sizeof(T);
	}

	consteval void putBytes(std::span<const uint8_t> bytes)
	{
		std::copy_n(bytes.data(), bytes.size(), &data[pos]);
		pos += bytes.size();
	}

	[[nodiscard]] constexpr size_t size() const { return pos; }

private:
	std::span<uint8_t> data;
	size_t pos = 0;
};

template<typename T> inline constexpr bool is_from_v = false;
template<typename T> inline constexpr bool is_from_v<From<T>> = true;
template<typename T> inline constexpr bool is_line_v = false;
template<typename T> inline constexpr bool is_line_v<Line<T>> = true;
template<typename T> inline constexpr bool is_curve_v = false;
template<typename T> inline constexpr bool is_curve_v<Curve<T>> = true;

template<typename T> concept FromSeg = is_from_v<T>;
template<typename T> concept LineSeg = is_line_v<T>;
template<typename T> concept CurveSeg = is_curve_v<T>;
template<typename T> concept LineOrCurveSeg = is_line_v<T> || is_curve_v<T>;


template<typename CoordT, typename T>
[[nodiscard]] consteval CoordT toCoord(T v)
{
	return CoordT(v);
}

template<typename CoordT, typename T>
consteval void writeFrom(ByteWriter& w, From<T> f)
{
	w.put(toCoord<CoordT>(f.x));
	w.put(toCoord<CoordT>(f.y));
}

template<typename CoordT, LineSeg Seg>
consteval void writeSegment(ByteWriter& w, Seg seg)
{
	w.put(toCoord<CoordT>(seg.x));
	w.put(toCoord<CoordT>(seg.y));
}

template<typename CoordT, CurveSeg Seg>
consteval void writeSegment(ByteWriter& w, Seg seg)
{
	w.put(toCoord<CoordT>(seg.cx1));
	w.put(toCoord<CoordT>(seg.cy1));
	w.put(toCoord<CoordT>(seg.cx2));
	w.put(toCoord<CoordT>(seg.cy2));
	w.put(toCoord<CoordT>(seg.x));
	w.put(toCoord<CoordT>(seg.y));
}

template<typename CoordT, LineOrCurveSeg Seg>
[[nodiscard]] consteval size_t segmentWireSize()
{
	if constexpr (CurveSeg<Seg>) {
		return 6 * sizeof(CoordT);
	} else {
		return 2 * sizeof(CoordT);
	}
}

template<typename CoordT, LineOrCurveSeg... Segs>
[[nodiscard]] consteval size_t pathWireSize()
{
	constexpr size_t segmentCount = sizeof...(Segs);
	return 1 + 2 * sizeof(CoordT)
	     + (segmentCount + 7) / 8
	     + (0 + ... + segmentWireSize<CoordT, Segs>());
}

template<size_t I, typename CoordT, size_t N, typename Tuple>
consteval void writeSegmentAt(ByteWriter& w,
                              const std::array<bool, N>& curveFlags,
                              Tuple segments)
{
	if ((I % 8) == 0) {
		uint8_t flags = 0;
		for (size_t j = 0; j < 8 && (I + j) < N; ++j) {
			if (curveFlags[I + j]) {
				flags |= uint8_t(1 << j);
			}
		}
		w.put(flags);
	}
	writeSegment<CoordT>(w, std::get<I>(segments));
}

template<typename CoordT, size_t N, typename Tuple, size_t... I>
consteval void writeAllSegments(ByteWriter& w,
                                const std::array<bool, N>& curveFlags,
                                Tuple segments,
                                std::index_sequence<I...>)
{
	(writeSegmentAt<I, CoordT, N>(w, curveFlags, segments), ...);
}

template<typename CoordT, FromSeg Anchor, LineOrCurveSeg... Segs>
[[nodiscard]] consteval auto packPath(Anchor anchor, Segs... segments)
{
	constexpr size_t segmentCount = sizeof...(Segs);
	static_assert(segmentCount >= 1, "path requires at least one segment after from()");
	static_assert(segmentCount <= 255, "path segment count must fit in uint8_t");

	std::array<uint8_t, pathWireSize<CoordT, Segs...>()> result{};
	ByteWriter w{std::span<uint8_t>{result.data(), result.size()}};

	w.put(uint8_t(segmentCount));
	writeFrom<CoordT>(w, anchor);

	constexpr std::array<bool, segmentCount> curveFlags = {is_curve_v<Segs>...};
	const auto segTuple = std::tuple{segments...};
	writeAllSegments<CoordT, segmentCount>(
		w, curveFlags, segTuple, std::make_index_sequence<segmentCount>{});

	if (w.size() != result.size()) {
		throw "VectorPath DSL: path size mismatch";
	}
	return result;
}

template<typename CoordT, typename Anchor, typename... Segs, size_t... I>
consteval auto packOpenDescriptor(OpenStrokePath<Anchor, Segs...> desc,
                                  std::index_sequence<I...>)
{
	return packPath<CoordT>(desc.anchor, std::get<I>(desc.segments)...);
}

template<typename CoordT, typename Anchor, typename... Segs, size_t... I>
consteval auto packClosedDescriptor(ClosedStrokePath<Anchor, Segs...> desc,
                                    std::index_sequence<I...>)
{
	return packPath<CoordT>(desc.anchor, std::get<I>(desc.segments)...);
}

template<typename CoordT, typename Anchor, typename... Segs>
consteval auto packDescriptor(OpenStrokePath<Anchor, Segs...> desc)
{
	return packOpenDescriptor<CoordT, Anchor, Segs...>(
		desc, std::make_index_sequence<sizeof...(Segs)>{});
}

template<typename CoordT, typename Anchor, typename... Segs>
consteval auto packDescriptor(ClosedStrokePath<Anchor, Segs...> desc)
{
	return packClosedDescriptor<CoordT, Anchor, Segs...>(
		desc, std::make_index_sequence<sizeof...(Segs)>{});
}

template<typename T>
struct is_closed_stroke : std::false_type {};

template<typename Anchor, typename... Segs>
struct is_closed_stroke<ClosedStrokePath<Anchor, Segs...>> : std::true_type {};

template<typename T>
inline constexpr bool is_closed_stroke_v = is_closed_stroke<T>::value;

template<typename CoordT, typename Desc, size_t... I>
consteval size_t descriptorWireSizeImpl(std::index_sequence<I...>)
{
	return pathWireSize<CoordT, std::tuple_element_t<I, decltype(Desc::segments)>...>();
}

template<typename CoordT, typename Desc>
consteval size_t descriptorWireSize()
{
	constexpr size_t n = std::tuple_size_v<decltype(Desc::segments)>;
	return descriptorWireSizeImpl<CoordT, Desc>(std::make_index_sequence<n>{});
}

template<typename CoordT, typename... Desc>
[[nodiscard]] consteval auto packStrokeCollection(Desc... desc)
{
	constexpr size_t pathCount = sizeof...(Desc);
	static_assert(pathCount >= 1, "stroke collection requires at least one path");
	static_assert(pathCount <= 255, "stroke collection path count must fit in uint8_t");

	constexpr std::array<bool, pathCount> closed = {is_closed_stroke_v<Desc>...};
	constexpr size_t headerSize = 1 + (pathCount + 7) / 8;
	constexpr size_t pathsSize = (descriptorWireSize<CoordT, Desc>() + ...);
	constexpr size_t totalSize = headerSize + pathsSize;

	std::array<uint8_t, totalSize> result{};
	ByteWriter w{std::span<uint8_t>{result.data(), result.size()}};
	w.put(uint8_t(pathCount));

	size_t index = 0;
	auto writeOne = [&](auto item) {
		if ((index % 8) == 0) {
			uint8_t flags = 0;
			for (size_t j = 0; j < 8 && (index + j) < pathCount; ++j) {
				if (closed[index + j]) {
					flags |= uint8_t(1 << j);
				}
			}
			w.put(flags);
		}
		auto bytes = packDescriptor<CoordT>(item);
		w.putBytes(std::span<const uint8_t>(bytes));
		++index;
	};
	(writeOne(desc), ...);

	if (w.size() != totalSize) {
		throw "VectorPath DSL: stroke collection size mismatch";
	}
	return result;
}

} // namespace detail

// --- Pack API ---

template<typename CoordT, detail::FromSeg Anchor, detail::LineOrCurveSeg... Segs>
[[nodiscard]] consteval auto path(Anchor anchor, Segs... segments)
{
	return detail::packPath<CoordT>(anchor, segments...);
}

template<typename Anchor, detail::LineOrCurveSeg... Segs>
[[nodiscard]] consteval OpenStrokePath<Anchor, Segs...> openPath(Anchor anchor, Segs... segments)
{
	return {anchor, std::tuple{segments...}};
}

template<typename Anchor, detail::LineOrCurveSeg... Segs>
[[nodiscard]] consteval ClosedStrokePath<Anchor, Segs...> closedPath(
	Anchor anchor, Segs... segments)
{
	return {anchor, std::tuple{segments...}};
}

template<typename CoordT, typename... Desc>
[[nodiscard]] consteval auto strokeCollection(Desc... desc)
{
	return detail::packStrokeCollection<CoordT>(desc...);
}

} // namespace VectorPath::dsl

#endif
