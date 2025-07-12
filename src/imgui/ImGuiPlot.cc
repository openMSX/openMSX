#include "ImGuiPlot.hh"

#include "imgui.h"
#include "imgui_internal.h" // for ImLerp

#include "narrow.hh"

#include <algorithm>
#include <span>

namespace openmsx {

// Simplified/optimized version of ImGui::PlotLines()
void plotLinesRaw(std::span<const float> values, float scale_min, float scale_max,
                  gl::vec2 top_left, gl::vec2 bottom_right, uint32_t color)
{
	int num_values = narrow<int>(values.size());
	if (num_values < 2) return;
	int num_items = num_values - 1;

	int inner_width = std::max(1, static_cast<int>(bottom_right.x - top_left.x));
	int res_w = std::min(inner_width, num_items);

	float t_step = 1.0f / (float)res_w;
	float scale_range = scale_max - scale_min;
	float inv_scale = (scale_range != 0.0f) ? (1.0f / scale_range) : 0.0f;

	auto valueToY = [&](float v) {
		return 1.0f - std::clamp((v - scale_min) * inv_scale, 0.0f, 1.0f);
	};
	float t = 0.0f;
	auto tp0 = gl::vec2(t, valueToY(values[0]));
	auto pos0 = ImLerp(top_left, bottom_right, tp0);

	auto* drawList = ImGui::GetWindowDrawList();
	for (int n = 0; n < res_w; n++) {
		auto idx = static_cast<int>(t * float(num_items) + 0.5f);
		assert(0 <= idx && idx < num_values);
		float v = values[(idx + 1) % num_values];

		t += t_step;
		auto tp1 = gl::vec2(t, valueToY(v));
		auto pos1 = ImLerp(top_left, bottom_right, tp1);
		drawList->AddLine(pos0, pos1, color);
		pos0 = pos1;
	}
}

void plotLines(std::span<const float> values, float scale_min, float scale_max, gl::vec2 outer_size)
{
	const auto& style = ImGui::GetStyle();

	gl::vec2 pos = ImGui::GetCursorScreenPos();
	auto outer_tl = pos;              // top-left
	auto outer_br = pos + outer_size; // bottom-right
	auto inner_tl = outer_tl + gl::vec2(style.FramePadding);
	auto inner_br = outer_br - gl::vec2(style.FramePadding);

	ImGui::RenderFrame(outer_tl, outer_br, ImGui::GetColorU32(ImGuiCol_FrameBg),
	                   true, style.FrameRounding);

	auto color = ImGui::GetColorU32(ImGuiCol_PlotLines);
	plotLinesRaw(values, scale_min, scale_max, inner_tl, inner_br, color);
}

// Simplified/optimized version of ImGui::PlotHistogram()
void plotHistogram(std::span<const float> values, float scale_min, float scale_max, gl::vec2 outer_size)
{
	const auto& style = ImGui::GetStyle();

	gl::vec2 pos = ImGui::GetCursorScreenPos();
	auto outer_tl = pos;              // top-left
	auto outer_br = pos + outer_size; // bottom-right
	auto inner_tl = outer_tl + gl::vec2(style.FramePadding);
	auto inner_br = outer_br - gl::vec2(style.FramePadding);

	ImGui::RenderFrame(outer_tl, outer_br, ImGui::GetColorU32(ImGuiCol_FrameBg),
	                   true, style.FrameRounding);
	if (values.empty()) return;

	int num_values = narrow<int>(values.size());
	int inner_width = std::max(1, static_cast<int>(inner_br.x - inner_tl.x));
	int res_w = std::min(inner_width, num_values);

	float t_step = 1.0f / static_cast<float>(res_w);
	float scale_range = scale_max - scale_min;
	float inv_scale = (scale_range != 0.0f) ? (1.0f / scale_range) : 0.0f;

	auto valueToY = [&](float v) {
		return 1.0f - std::clamp((v - scale_min) * inv_scale, 0.0f, 1.0f);
	};
	float zero_line = (scale_min * scale_max < 0.0f)
	                ? (1 + scale_min * inv_scale)
	                : (scale_min < 0.0f ? 0.0f : 1.0f);

	auto color = ImGui::GetColorU32(ImGuiCol_PlotHistogram);

	float t0 = 0.0f;
	auto* drawList = ImGui::GetWindowDrawList();
	drawList->PrimReserve(6 * res_w, 4 * res_w);

	for (int n = 0; n < res_w; n++) {
		auto idx = static_cast<int>(t0 * float(num_values) + 0.5f);
		assert(0 <= idx && idx < num_values);
		float y0 = valueToY(values[idx]);
		float t1 = t0 + t_step;

		auto pos0 = ImLerp(inner_tl, inner_br, gl::vec2(t0, y0));
		auto pos1 = ImLerp(inner_tl, inner_br, gl::vec2(t1, zero_line));
		drawList->PrimRect(pos0, pos1, color);

		t0 = t1;
	}
}

} // namespace openmsx
