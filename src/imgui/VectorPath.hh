#ifndef VECTOR_PATH_HH
#define VECTOR_PATH_HH

// Compact vector-path format for Dear ImGui drawing.
//
// Paths are stored as constexpr byte arrays. See VectorPathDsl.hh for the
// compile-time packer DSL.
//
// Coordinates are stored as native-endian values of type CoordT (typically
// int8_t, int16_t, or float). The CoordinateTransform converts CoordT to
// vec2 for Dear ImGui.
//
// ----------------------------------------------------------------------------
// Single path
// ----------------------------------------------------------------------------
//
//   [segment_count: u8]              N segments after the anchor (N >= 1)
//   [anchor_x, anchor_y: CoordT]
//   [flags_0: u8]                    segment-type bits for segments 0..7
//   [segment 0 payload]
//   ...
//   [segment 7 payload]
//   [flags_1: u8]                    segment-type bits for segments 8..15
//   [segment 8 payload]
//   ...
//
// Before segment i (i = 0 .. N-1), when i % 8 == 0, read one flags byte.
// Bit (i % 8) of that byte: 0 = line, 1 = cubic Bézier.
// Unused bits in the last flags byte are ignored.
//
// Segment payloads (native-endian CoordT pairs):
//   line:  x, y                               (2 * sizeof(CoordT))
//   cubic: cx1, cy1, cx2, cy2, ex, ey         (6 * sizeof(CoordT))
//
// Cubic segments use the same semantics as SVG "C" and ImGui
// PathBezierCubicCurveTo (anchor or previous point is the implicit start).
//
// ----------------------------------------------------------------------------
// Collection (stroked paths only)
// ----------------------------------------------------------------------------
//
//   [path_count: u8]                 M paths (M >= 1)
//   [flags_0: u8]                    open/closed bits for paths 0..7
//   [path 0 blob]                    self-describing single path
//   [path 1 blob]
//   ...
//   [flags_1: u8]                    when M > 8, before path 8
//   [path 8 blob]
//   ...
//
// Before path i (i = 0 .. M-1), when i % 8 == 0, read one flags byte.
// Bit (i % 8): 0 = open stroke, 1 = closed stroke (ImDrawFlags_Closed).
// Each path blob uses the single-path layout above.
//
// ----------------------------------------------------------------------------
// Draw semantics (Dear ImGui)
// ----------------------------------------------------------------------------
//
//   drawPathStroke:        PathStroke on a single path (ImDrawFlags, default open)
//   drawPathStrokes:       PathStroke per path; open/closed from collection flags
//   drawPathFilled:        PathFillConcave (implicitly closes the contour)
//   drawPathFilledStroked: PathFillConcave, then PathStroke with Closed
//
// Fill functions take a single path blob (no open/closed bit in the data).
// Colors and stroke thickness are runtime parameters, not stored in the blob.

#include "video/gl_mat.hh"
#include "video/gl_vec.hh"

#include <imgui.h>

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

namespace VectorPath {

// Sequential reader over a byte span. Each get<T>() advances the read position
// and asserts that sizeof(T) bytes are available.
class ByteReader {
public:
	explicit ByteReader(std::span<const uint8_t> data_)
		: data(data_) {}

	template<typename T>
	[[nodiscard]] T get()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		assert(pos + sizeof(T) <= data.size());
		T v;
		std::memcpy(&v, data.data() + pos, sizeof(T));
		pos += sizeof(T);
		return v;
	}

	[[nodiscard]] bool empty() const { return pos >= data.size(); }

private:
	std::span<const uint8_t> data;
	size_t pos = 0;
};

template<typename CoordT>
concept WireCoordinate = std::is_trivially_copyable_v<CoordT>;

// --- Coordinate transforms (template parameter to draw functions) ---

struct IdentityTransform {
	template<typename CoordT>
	[[nodiscard]] static gl::vec2 operator()(CoordT x, CoordT y)
	{
		return {float(x), float(y)};
	}
};

struct ScaleTransform {
	float scale;

	template<typename CoordT>
	[[nodiscard]] gl::vec2 operator()(CoordT x, CoordT y) const
	{
		return operator()(gl::vec2{float(x), float(y)});
	}
	[[nodiscard]] gl::vec2 operator()(gl::vec2 c) const { return c * scale; }
};

struct TranslateTransform {
	gl::vec2 translate;

	template<typename CoordT>
	[[nodiscard]] gl::vec2 operator()(CoordT x, CoordT y) const
	{
		return gl::vec2{float(x), float(y)} + translate;
	}
};

struct ScaleTranslateTransform {
	float scale;
	gl::vec2 translate;

	template<typename CoordT>
	[[nodiscard]] gl::vec2 operator()(CoordT x, CoordT y) const
	{
		return gl::vec2{float(x), float(y)} * scale + translate;
	}
};

struct AffineTransform {
	gl::mat2 linear;
	gl::vec2 translate;

	template<typename CoordT>
	[[nodiscard]] gl::vec2 operator()(CoordT x, CoordT y) const
	{
		return linear * gl::vec2{float(x), float(y)} + translate;
	}
};

// Build an affine transform: scale, then rotate (radians), then translate.
[[nodiscard]] inline AffineTransform makeAffineTransform(
	float angleRadians, float scale, gl::vec2 translate)
{
	float s = std::sin(angleRadians);
	float c = std::cos(angleRadians);
	auto linear = gl::mat2{gl::vec2{ c, s},
	                       gl::vec2(-s, c)} * scale;
	return {linear, translate};
}

template<typename T>
concept CoordinateTransform = requires(const T& t, int x, int y) {
	{ t(x, y) } -> std::convertible_to<gl::vec2>;
};

static_assert(CoordinateTransform<IdentityTransform>);
static_assert(CoordinateTransform<ScaleTransform>);
static_assert(CoordinateTransform<TranslateTransform>);
static_assert(CoordinateTransform<AffineTransform>);

namespace detail {

template<typename CoordT, CoordinateTransform TF>
void buildPath(ImDrawList* drawList, ByteReader& reader, TF transform)
{
	auto segmentCount = reader.get<uint8_t>();
	assert(segmentCount >= 1);

	auto getXY = [&] {
		auto x = reader.get<CoordT>();
		auto y = reader.get<CoordT>();
		return transform(x, y);
	};

	drawList->PathClear();
	drawList->PathLineTo(getXY());

	uint8_t flags = 0;
	for (uint8_t i = 0; i < segmentCount; ++i) {
		if ((i % 8) == 0) {
			flags = reader.get<uint8_t>();
		}

		if ((flags >> (i % 8)) & 1) {
			auto c1 = getXY();
			auto c2 = getXY();
			auto e = getXY();
			drawList->PathBezierCubicCurveTo(c1, c2, e);
		} else {
			drawList->PathLineTo(getXY());
		}
	}
}

} // namespace detail

// --- Draw API ---

template<typename CoordT, CoordinateTransform TF>
void drawPathStroke(ImDrawList* drawList, std::span<const uint8_t> path,
                    TF transform, ImU32 color, float thickness,
                    ImDrawFlags flags = ImDrawFlags_None)
{
	ByteReader reader(path);
	detail::buildPath<CoordT>(drawList, reader, transform);
	drawList->PathStroke(color, thickness, flags);
	assert(reader.empty());
}

template<typename CoordT, CoordinateTransform TF>
void drawPathStrokes(ImDrawList* drawList, std::span<const uint8_t> collection,
                     TF transform, ImU32 color, float thickness)
{
	ByteReader reader(collection);
	auto pathCount = reader.get<uint8_t>();

	uint8_t strokeFlags = 0;
	for (uint8_t i = 0; i < pathCount; ++i) {
		if ((i % 8) == 0) {
			strokeFlags = reader.get<uint8_t>();
		}

		bool closed = (strokeFlags >> (i % 8)) & 1;
		detail::buildPath<CoordT>(drawList, reader, transform);
		drawList->PathStroke(color, thickness,
		                     closed ? ImDrawFlags_Closed : ImDrawFlags_None);
	}
	assert(reader.empty());
}

template<typename CoordT, CoordinateTransform TF>
void drawPathFilled(ImDrawList* drawList, std::span<const uint8_t> path,
                    TF transform, ImU32 fillColor)
{
	ByteReader reader(path);
	detail::buildPath<CoordT>(drawList, reader, transform);
	drawList->PathFillConcave(fillColor);
	assert(reader.empty());
}

template<typename CoordT, CoordinateTransform TF>
void drawPathFilledStroked(ImDrawList* drawList, std::span<const uint8_t> path,
                           TF transform, ImU32 fillColor,
                           ImU32 outlineColor, float outlineThickness)
{
	{
		ByteReader reader(path);
		detail::buildPath<CoordT>(drawList, reader, transform);
		drawList->PathFillConcave(fillColor);
		assert(reader.empty());
	}
	{
		ByteReader reader(path);
		detail::buildPath<CoordT>(drawList, reader, transform);
		drawList->PathStroke(outlineColor, outlineThickness, ImDrawFlags_Closed);
		assert(reader.empty());
	}
}

} // namespace VectorPath

#endif
