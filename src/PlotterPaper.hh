#ifndef PLOTTER_PAPER_HH
#define PLOTTER_PAPER_HH

#include "GLUtil.hh"
#include "MemBuffer.hh"
#include "gl_vec.hh"

#include <algorithm>
#include <cassert>
#include <span>
#include <utility>

namespace openmsx {

using GrayPixel = uint8_t;
using RgbPixel = gl::vecN<3, uint8_t>;

template<typename T> class Canvas
{
public:
	Canvas(gl::ivec2 size, T init_value = T{})
		: pixels(size_t(size.x) * size.y)
		, sz(size)
	{
		std::ranges::fill(pixels, init_value);
	}

	[[nodiscard]] gl::ivec2 size() const { return sz; }

	// Access entire scanline (reduces address calculation overhead)
	[[nodiscard]] std::span<T> getLine(int y) {
		assert(y >= 0 && y < sz.y);
		return std::span<T>(pixels.data() + y * sz.x, sz.x);
	}
	[[nodiscard]] std::span<const T> getLine(int y) const {
		assert(y >= 0 && y < sz.y);
		return std::span<const T>(pixels.data() + y * sz.x, sz.x);
	}

private:
	MemBuffer<T> pixels;
	gl::ivec2 sz;
};


class PlotterPaper
{
public:
	explicit PlotterPaper(gl::ivec2 size);

	[[nodiscard]] gl::ivec2 size() const { return paper.size(); }
	[[nodiscard]] bool empty() const { return !anythingPlotted; }

	void draw_dot(gl::vec2 pos, float radius, gl::vec3 color);
	void draw_motion(gl::vec2 from, gl::vec2 to, float radius, gl::vec3 color);

	[[nodiscard]] Canvas<RgbPixel> getRGB() const;
	gl::Texture& updateTexture(gl::ivec2 targetSize);

private:
	Canvas<gl::vec3> paper;

	gl::Texture texture{gl::Null{}};
	gl::ivec2 texSize;

	bool damage = true; // TODO track damage bounding box
	bool anythingPlotted = false; // Used to avoid saving blank file when nothing was plotted
};

} // namespace openmsx

#endif
