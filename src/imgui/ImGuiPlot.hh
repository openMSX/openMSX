#ifndef IMGUIPLOT_HH
#define IMGUIPLOT_HH

#include <span>

#include "gl_vec.hh"

namespace openmsx {

void plotLines(std::span<const float> values, float scale_min, float scale_max, gl::vec2 outer_size);
void plotLinesRaw(std::span<const float> values, float scale_min, float scale_max,
                  gl::vec2 top_left, gl::vec2 bottom_right, uint32_t color);
void plotHistogram(std::span<const float> values, float scale_min, float scale_max, gl::vec2 outer_size);

} // namespace openmsx

#endif
