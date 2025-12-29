#include "ImGuiTraceViewer.hh"

#include "ImGuiCpp.hh"
#include "ImGuiOpenFile.hh"
#include "ImGuiUtils.hh"
#include "Shortcuts.hh"

#include "Debugger.hh"
#include "EmuDuration.hh"
#include "MSXMotherBoard.hh"

#include "find_closest.h"
#include "timeline.hh"

#include "imgui_stdlib.h"

#include <algorithm>
#include <cassert>

namespace openmsx {

using Traces = std::span<Tracer::Trace*>;
using Events = std::span<const Tracer::Event>;

constexpr float splitterWidth = 7.0f; // should be odd

struct Convertor {
	EmuTime viewStartTime;
	float viewScreenWidth;
	float pixelsPerTick;

	Convertor(EmuTime viewStartTime_, EmuDuration viewDuration, float viewScreenWidth_)
		: viewStartTime(viewStartTime_)
		, viewScreenWidth(viewScreenWidth_)
		, pixelsPerTick(viewScreenWidth_ / float(viewDuration.toUint64()))
	{}

	[[nodiscard]] float timeToX(EmuTime time) const {
		int64_t dt = int64_t(time.toUint64()) - int64_t(viewStartTime.toUint64());
		return float(dt) * pixelsPerTick;
	}

	[[nodiscard]] EmuDuration deltaXtoDuration(float deltaX) const {
		return EmuDuration(uint64_t(deltaX / pixelsPerTick));
	}
	[[nodiscard]] EmuTime xToTime(float x) const {
		return viewStartTime + deltaXtoDuration(x);
	}
};

static auto calcRulerTicks(double begin, double end, double n)
{
	struct Result {
		double first;
		double step;
	};

	auto length = end - begin;
	auto i = length / n;
	auto magnitude = std::pow(10.0, std::floor(std::log10(i)));
	auto mantissa = i / magnitude;
	auto snap = (mantissa >= 5.0) ? 10.0
	          : (mantissa >= 2.0) ?  5.0
	                              :  2.0;
	auto step = snap * magnitude;
	auto first = std::floor(begin / step) * step;
	return Result{.first = first, .step = step};
}

// Generate timeline labels with consistent formatting (fixed or
// scientific notation) for the range [start, stop).
//
// Parameters:
//   from, to:   Range to generate labels for
//   numLabels:  Preferred number of labels (upper bound)
//
// Returns:
//  * Vector of Labels at positions start, start+step, start+2*step, ...
//    (Cached - repeated calls with same parameters return immediately)
//  * step size
std::pair<const std::vector<TimelineFormatter::Label>&, double>
TimelineFormatter::calc(double from, double to, double numLabels, double unitFactor)
{
	// Check if parameters changed - avoid recalculation if same
	Params params{.from = from, .to = to, .numLabels = numLabels, .unitFactor = unitFactor};
	if (params == prevParams) return {labels, step}; // parameters unchanged, return cache
	prevParams = params;

	auto ruler = calcRulerTicks(from * unitFactor, to * unitFactor, numLabels);
	auto start = ruler.first;
	step = ruler.step;
	auto stop = start + step * std::ceil(numLabels);

	// Use a representative value for format decision (avoid zero
	// since it's atypical) Given start < stop requirement,
	// testValue is always non-zero
	auto testValue = (start == 0.0) ? stop : start;
	auto absTestValue = std::abs(testValue);

	// Calculate common subexpressions
	auto logStep = std::log10(step);
	int ceilNegLogStep = -static_cast<int>(std::floor(
		logStep)); // ceil(-logStep) = -floor(logStep)
	int e = static_cast<int>(std::floor(std::log10(absTestValue)));

	// Calculate scientific notation parameters
	// Since step = {1, 2, 5} × 10^n, we can derive precision from
	// the step size and the value's exponent: precision =
	// ceil(exponent - log10(step)) Using identity: ceil(e -
	// logStep) = e + ceil(-logStep) for integer e
	int sciPrecision = e + ceilNegLogStep;
	int sciLen = sciPrecision + 5; // Format: d.dddde±ee = 1 + prec
					// + 'e' + sign + 2 digits

	// Calculate fixed notation parameters
	int decimals = (step < 1.0) ? ceilNegLogStep : 0;
	int intDigits =
		(absTestValue < 1.0)
			? 1
			: e + 1; // e+1 converts exponent to digit count
	int fixedLen = intDigits + decimals;

	// Decide format by comparing approximate string lengths
	// Assumptions: ignore sign (equal in both), ignore decimal
	// point (narrow), assume exponent fits in 2 digits (±00 to ±99,
	// reasonable for timelines)
	bool useScientific = sciLen < fixedLen;

	// Generate all labels in the chosen format
	// Note: Using snprintf for now. Future: switch to std::format
	// (C++20, GCC 13+)
	labels.clear();
	auto value = start;
	char buf[32];
	if (useScientific) {
		while (value < stop) {
			int len = snprintf(buf, sizeof(buf), "%.*e",
						sciPrecision, value);
			labels.push_back(
				{value / unitFactor, std::string(buf, len)});
			value += step;
		}
	} else {
		while (value < stop) {
			int len = snprintf(buf, sizeof(buf), "%.*f",
						decimals, value);
			labels.push_back(
				{value / unitFactor, std::string(buf, len)});
			value += step;
		}
	}
	return {labels, step};
}

ImGuiTraceViewer::ImGuiTraceViewer(ImGuiManager& manager_)
	: ImGuiPart(manager_)
{
}

void ImGuiTraceViewer::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiTraceViewer::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

static void drawText(ImDrawList* drawList, gl::vec2 pos, ImU32 color, std::string_view text)
{
	drawList->AddText(pos, color, text.data(), text.data() + text.size());
}

struct DrawCoarse {
	ImDrawList* drawList;
	float x;
	float y0, y1;
	ImU32 colorNormal;
	ImU32 colorHover;
	bool rowHovered;

	void operator()(float x0, float x1) const {
		auto xf0 = std::floor(x0 + x);
		auto xf1 = std::floor(x1 + x + 1.0f);
		drawList->AddRectFilled({xf0, y0}, {xf1, y1}, colorNormal);

		if (rowHovered) {
			auto mouseX = ImGui::GetIO().MousePos.x;
			if (x0 <= mouseX && mouseX < x1) {
				drawList->AddLine({mouseX, y0}, {mouseX, y1}, colorHover, 2.0f);
			}
		}
	}
};

static void drawEventsVoid(
	gl::vec2 topLeft, const Convertor& convertor, EmuTime maxT, Events events, bool rowHovered)
{
	auto* drawList = ImGui::GetWindowDrawList();
	auto colorLine = ImGui::GetColorU32(ImGuiCol_PlotLines);
	auto h = ImGui::GetFrameHeight();
	auto h5 = h * 0.20f;
	auto y0 = topLeft.y;
	auto y1 = y0 + h;
	auto y5 = y0 + h5;

	auto minX = drawList->GetClipRectMin().x;
	auto maxX = drawList->GetClipRectMax().x;
	drawList->AddLine({minX, y1}, {maxX, y1}, colorLine);
	if (events.empty()) return;

	auto highLightTime = [&] {
		if (!rowHovered) return EmuTime::infinity(); // no highlight
		const auto& io = ImGui::GetIO();
		auto mouseX = io.MousePos.x - topLeft.x; // includes scroll offset
		auto [it, dist] = find_closest(
			events, convertor.xToTime(mouseX), {}, &Tracer::Event::time);
		assert(it != events.end());
		return (dist < convertor.deltaXtoDuration(h5)) ? it->time : EmuTime::infinity();
	}();

	auto colorNormal = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
	auto colorHover = ImGui::GetColorU32(ImGuiCol_PlotHistogramHovered);

	auto drawDetailed = [&](float x0, float /*x1*/, const Tracer::Event& event) {
		x0 += topLeft.x;
		std::array<ImVec2, 3> points = {
			gl::vec2{x0 - h5, y5},
			gl::vec2{x0     , y0},
			gl::vec2{x0 + h5, y5},
		};
		bool hover = event.time == highLightTime;
		auto color = hover ? colorHover : colorNormal;
		auto thickness = hover ? 2.0f : 1.0f;
		drawList->AddPolyline(points.data(), narrow<int>(points.size()), color, 0, thickness);
		drawList->AddLine({x0, y0}, {x0, y1}, color, thickness);
	};
	processTimeline(events, maxT, convertor, drawDetailed,
	                DrawCoarse{drawList, topLeft.x, y0, y1, colorNormal, colorHover, rowHovered});
}

static void drawEventsBool(
	gl::vec2 topLeft, const Convertor& convertor, EmuTime maxT, Events events, bool rowHovered)
{
	if (events.empty()) return;

	auto* drawList = ImGui::GetWindowDrawList();
	auto colorNormal = ImGui::GetColorU32(ImGuiCol_PlotLines);
	auto colorHover = ImGui::GetColorU32(ImGuiCol_PlotLinesHovered);
	auto h = ImGui::GetFrameHeight();
	auto y0 = topLeft.y;
	auto y1 = y0 + h;

	const auto& io = ImGui::GetIO();
	auto mouseX = io.MousePos.x;

	auto getVal = [&](const Tracer::Event& e) {
		return e.value.get<uint64_t>() ? y0 : y1;
	};

	auto yp = y1;
	auto drawDetailed = [&](float x0, float x1, const Tracer::Event& event) {
		auto xf0 = topLeft.x + x0;
		auto xf1 = topLeft.x + x1;
		auto y = getVal(event);

		std::array<ImVec2, 3> points = {
			gl::vec2{xf1, y},
			gl::vec2{xf0, y},
			gl::vec2{xf0, yp},
		};
		auto hover = rowHovered && xf0 <= mouseX && mouseX < xf1;
		auto color = hover ? colorHover : colorNormal;
		auto thickness = hover ? 2.0f : 1.0f;
		drawList->AddPolyline(points.data(), (yp == y) ? 2 : 3, color, 0, thickness);

		yp = y;
	};
	processTimeline(events, maxT, convertor, drawDetailed,
	                DrawCoarse{drawList, topLeft.x, y0, y1, colorNormal, colorHover, rowHovered});
}

[[nodiscard]] static std::string_view formatTraceValue(const TraceValue& value, std::span<char, 32> tmpBuf)
{
	return value.visit(overloaded{
		[](std::monostate) {
			return std::string_view{};
		},
		[&](uint64_t i) {
			auto [end, ec] = std::to_chars(tmpBuf.data(), tmpBuf.data() + tmpBuf.size(), i);
			assert(ec == std::errc{}); // should never fail (for sufficiently large buffer)
			return std::string_view(tmpBuf.data(), end - tmpBuf.data());
		},
		[&](double d) {
		#if 0
			// doesn't work yet in MacOS :(
			auto [end, ec] = std::to_chars(tmpBuf.data(), tmpBuf.data() + tmpBuf.size(),
			                               d, std::chars_format::general);
			assert(ec == std::errc{}); // should never fail (for sufficiently large buffer)
			return std::string_view(tmpBuf.data(), end - tmpBuf.data());
		#endif
			int len = snprintf(tmpBuf.data(), tmpBuf.size(), "%.17g", d);
			assert(len > 0 && size_t(len) < tmpBuf.size());
			return std::string_view(tmpBuf.data(), len);
		},
		[](std::string_view s) {
			return s;
		}
	});
}

static void drawEventsValue(
	gl::vec2 topLeft, const Convertor& convertor, EmuTime maxT, Events events, bool rowHovered)
{
	auto* drawList = ImGui::GetWindowDrawList();
	const auto& style = ImGui::GetStyle();
	auto colorNormal = ImGui::GetColorU32(ImGuiCol_PlotLines);
	auto colorHover = ImGui::GetColorU32(ImGuiCol_PlotLinesHovered);
	auto colorText = getColor(imColor::TEXT);
	auto h = ImGui::GetFrameHeight();
	auto h2 = h * 0.50f;
	auto h5 = h * 0.20f;
	auto y0 = topLeft.y;
	auto y1 = y0 + h;
	auto y2 = y0 + h2;

	const auto& io = ImGui::GetIO();
	auto mouseX = io.MousePos.x;

	auto drawDetailed = [&](float x0, float x1, const Tracer::Event& event) {
		auto xf0 = topLeft.x + x0;
		auto xf1 = topLeft.x + x1;
		auto hover = rowHovered && xf0 <= mouseX && mouseX < xf1;
		auto color = hover ? colorHover : colorNormal;
		auto thickness = hover ? 2.0f : 1.0f;

		auto tl = gl::vec2{xf0, y0};
		auto br = gl::vec2{xf1, y1};
		if (auto maxWidth = x1 - x0 - 2.0f * h5; maxWidth > 0.0f) {
			std::array<ImVec2, 6> points = {
				gl::vec2{xf1 - h5, y0},
				gl::vec2{xf0 + h5, y0},
				gl::vec2{xf0,      y2},
				gl::vec2{xf0 + h5, y1},
				gl::vec2{xf1 - h5, y1},
				gl::vec2{xf1,      y2},
			};
			drawList->AddPolyline(points.data(), 6, color, ImDrawFlags_Closed, thickness);
			drawList->PushClipRect(tl, br, true);
			std::array<char, 32> tmpBuf;
			auto valueStr = formatTraceValue(event.value, tmpBuf);
			auto textX = std::max(xf0 + h5, topLeft.x + ImGui::GetScrollX());
			drawText(drawList, gl::vec2{textX, y0 + style.FramePadding.y}, colorText, valueStr);
			drawList->PopClipRect();
		} else {
			drawList->AddRect(tl, br, color, {}, {}, thickness);
		}
	};
	processTimeline(events, maxT, convertor, drawDetailed,
	                DrawCoarse{drawList, topLeft.x, y0, y1, colorNormal, colorHover, rowHovered});
}

static bool menuItemWithShortcut(ImGuiManager& manager, const char* label, Shortcuts::ID id, bool* p_selected = nullptr)
{
	const auto& shortcuts = manager.getShortcuts();
	const auto& shortcut = shortcuts.getShortcut(id);
	if (shortcut.keyChord == ImGuiKey_None) {
		return ImGui::MenuItem(label, nullptr, p_selected);
	} else {
		return ImGui::MenuItem(label, getKeyChordName(shortcut.keyChord).c_str(), p_selected);
	}
}

void ImGuiTraceViewer::drawMenuBar(EmuTime minT, EmuDuration totalT, Tracer& tracer, Traces traces)
{
	im::Menu("File", [&]{
		ImGui::MenuItem("Select probes and traces", nullptr, &showSelect);
		if (ImGui::MenuItem("Export as VCD ...")) {
			manager.openFile->selectNewFile(
				"Export to VCD", "VCD (*.vcd){.vcd}",
				[&](const auto& fn) {
					try {
						tracer.exportVCD(fn);
					} catch (MSXException& e) {
						manager.printError(
							"Couldn't export VCD: ", e.getMessage());
					}
				});
		}
		if (ImGui::MenuItem("Close")) show = false;
	});
	im::Menu("View", [&]{
		if (menuItemWithShortcut(manager, "Zoom to fit", Shortcuts::ID::TRACE_ZOOM_TO_FIT)) {
			zoomToFit(minT, totalT);
		}
		if (menuItemWithShortcut(manager, "Zoom in", Shortcuts::ID::TRACE_ZOOM_IN)) {
			doZoom(1.0 / 1.2, 0.5);
		}
		if (menuItemWithShortcut(manager, "Zoom out", Shortcuts::ID::TRACE_ZOOM_OUT)) {
			doZoom(1.2, 0.5);
		}
		ImGui::Separator();

		ImGui::TextUnformatted("Timeline:");
		im::Indent([&]{
			im::Menu("origin", [&]{
				if (ImGui::RadioButton("MSX boot", !timelineRelative)) timelineRelative = false;
				simpleToolTip("Absolute time since MSX boot");
				if (ImGui::RadioButton("Primary marker", timelineRelative)) timelineRelative = true;
				simpleToolTip("Relative to primary marker");
				// TODO relative to current time
			});
			im::Menu("start", [&]{
				if (ImGui::RadioButton("MSX boot", !timelineStart)) timelineStart = false;
				if (ImGui::RadioButton("First event", timelineStart)) timelineStart = true;
			});
			im::Menu("stop", [&]{
				if (ImGui::RadioButton("Current emulation time", !timelineStop)) timelineStop = false;
				if (ImGui::RadioButton("Last event", timelineStop)) timelineStop = true;
			});
		});
		ImGui::Separator();

		menuItemWithShortcut(manager, "Show menu bar", Shortcuts::ID::TRACE_SHOW_MENU_BAR, &showMenuBar);
	});
	im::Menu("Goto", [&]{
		constexpr auto pos_text = "positive edge = rising edge = transition from low to high";
		constexpr auto neg_text = "negative edge = falling edge = transition from high to low";
		ImGui::TextUnformatted("Move primary marker to");
		simpleToolTip("Place via left-mouse-click on the graph.");
		im::Indent([&]{
			if (menuItemWithShortcut(manager, "Next edge", Shortcuts::ID::TRACE_NEXT_EDGE)) {
				gotoNextEdge(traces, selectedTime1);
			}
			if (menuItemWithShortcut(manager, "Next positive edge", Shortcuts::ID::TRACE_NEXT_POS_EDGE)) {
				gotoNextPosEdge(traces, selectedTime1);
			}
			simpleToolTip(pos_text);
			if (menuItemWithShortcut(manager, "Next negative edge", Shortcuts::ID::TRACE_NEXT_NEG_EDGE)) {
				gotoNextNegEdge(traces, selectedTime1);
			}
			simpleToolTip(neg_text);
			if (menuItemWithShortcut(manager, "Previous edge", Shortcuts::ID::TRACE_PREV_EDGE)) {
				gotoPrevEdge(traces, selectedTime1);
			}
			if (menuItemWithShortcut(manager, "Previous positive edge", Shortcuts::ID::TRACE_PREV_POS_EDGE)) {
				gotoPrevPosEdge(traces, selectedTime1);
			}
			simpleToolTip(pos_text);
			if (menuItemWithShortcut(manager, "Previous negative edge", Shortcuts::ID::TRACE_PREV_NEG_EDGE)) {
				gotoPrevNegEdge(traces, selectedTime1);
			}
			simpleToolTip(neg_text);
		});
		ImGui::Separator();

		ImGui::TextUnformatted("Move secondary marker to");
		simpleToolTip("Place via shift-left-mouse-click on the graph.");
		im::Indent([&]{
			if (menuItemWithShortcut(manager, "Next edge##alt", Shortcuts::ID::TRACE_ALT_NEXT_EDGE)) {
				gotoNextEdge(traces, selectedTime2);
			}
			if (menuItemWithShortcut(manager, "Next positive edge##alt", Shortcuts::ID::TRACE_ALT_NEXT_POS_EDGE)) {
				gotoNextPosEdge(traces, selectedTime2);
			}
			simpleToolTip(pos_text);
			if (menuItemWithShortcut(manager, "Next negative edge##alt", Shortcuts::ID::TRACE_ALT_NEXT_NEG_EDGE)) {
				gotoNextNegEdge(traces, selectedTime2);
			}
			simpleToolTip(neg_text);
			if (menuItemWithShortcut(manager, "Previous edge##alt", Shortcuts::ID::TRACE_ALT_PREV_EDGE)) {
				gotoPrevEdge(traces, selectedTime2);
			}
			if (menuItemWithShortcut(manager, "Previous positive edge##alt", Shortcuts::ID::TRACE_ALT_PREV_POS_EDGE)) {
				gotoPrevPosEdge(traces, selectedTime2);
			}
			simpleToolTip(pos_text);
			if (menuItemWithShortcut(manager, "Previous negative edge##alt", Shortcuts::ID::TRACE_ALT_PREV_NEG_EDGE)) {
				gotoPrevNegEdge(traces, selectedTime2);
			}
			simpleToolTip(neg_text);
		});
	});
	im::Menu("Help", [&]{
		if (ImGui::MenuItem("Overview")) {
			showHelp = true;
			helpSection = HelpSection::OVERVIEW;
		}
		if (ImGui::MenuItem("Probes and Traces")) {
			showHelp = true;
			helpSection = HelpSection::PROBES_AND_TRACES;
		}
		if (ImGui::MenuItem("Graphs")) {
			showHelp = true;
			helpSection = HelpSection::GRAPHS;
		}
		if (ImGui::MenuItem("Navigation")) {
			showHelp = true;
			helpSection = HelpSection::NAVIGATION;
		}
		if (ImGui::MenuItem("Example")) {
			showHelp = true;
			helpSection = HelpSection::EXAMPLE;
		}
	});
}

template<size_t N>
static void thinWhiteLine(ImDrawList* drawList, const std::array<ImVec2, N>& points)
	requires(N == 2)
{
	auto color = getColor(imColor::TEXT);
	drawList->AddLine(points[0], points[1], color, 1.0f);
	drawList->AddPolyline(points.data(), narrow<int>(points.size()), color, 0, 1.0f);
}
template<size_t N>
static void thinWhiteLine(ImDrawList* drawList, const std::array<ImVec2, N>& points)
	requires(N != 2)
{
	auto color = getColor(imColor::TEXT);
	drawList->AddPolyline(points.data(), narrow<int>(points.size()), color, 0, 1.0f);
}
static void arrowLeft(ImDrawList* drawList, gl::vec2 center, float u)
{
	std::array<ImVec2, 3> line1 = {
		center + u * gl::vec2{ 4.0f, -3.0f},
		center + u * gl::vec2{ 1.0f,  0.0f},
		center + u * gl::vec2{ 4.0f,  3.0f},
	};
	thinWhiteLine(drawList, line1);
	std::array<ImVec2, 2> line2 = {
		center + u * gl::vec2{ 1.0f, 0.0f},
		center + u * gl::vec2{10.0f, 0.0f},
	};
	thinWhiteLine(drawList, line2);
}
static void arrowRight(ImDrawList* drawList, gl::vec2 center, float u)
{
	std::array<ImVec2, 3> line1 = {
		center + u * gl::vec2{-4.0f, -3.0f},
		center + u * gl::vec2{-1.0f,  0.0f},
		center + u * gl::vec2{-4.0f,  3.0f},
	};
	thinWhiteLine(drawList, line1);
	std::array<ImVec2, 2> line2 = {
		center + u * gl::vec2{- 1.0f, 0.0f},
		center + u * gl::vec2{-10.0f, 0.0f},
	};
	thinWhiteLine(drawList, line2);
}
static void magnifyingGlass(ImDrawList* drawList, gl::vec2 center, float u)
{
	auto color = getColor(imColor::TEXT);
	drawList->AddCircle(center + u * gl::vec2{-1.0f, -1.0f}, 8.0f, color);
	std::array<ImVec2, 2> line = {
		center + u * gl::vec2{ 4.6f, 4.6f},
		center + u * gl::vec2{ 9.0f, 9.0f},
	};
	thinWhiteLine(drawList, line);
}

static void RenderZoomFit(gl::vec2 center, ImDrawList* drawList)
{
	auto u = ImGui::GetFrameHeightWithSpacing() * (1.0f / 23.0f);
	magnifyingGlass(drawList, center, u);
	std::array<ImVec2, 2> line1 = {
		center + u * gl::vec2{-6.0f, -1.0f},
		center + u * gl::vec2{ 4.0f, -1.0f},
	};
	thinWhiteLine(drawList, line1);
	std::array<ImVec2, 3> line2 = {
		center + u * gl::vec2{-3.0f, -4.0f},
		center + u * gl::vec2{-6.0f, -1.0f},
		center + u * gl::vec2{-3.0f,  2.0f},
	};
	thinWhiteLine(drawList, line2);
	std::array<ImVec2, 3> line3 = {
		center + u * gl::vec2{ 1.0f, -4.0f},
		center + u * gl::vec2{ 4.0f, -1.0f},
		center + u * gl::vec2{ 1.0f,  2.0f},
	};
	thinWhiteLine(drawList, line3);
}
static void RenderZoomOut(gl::vec2 center, ImDrawList* drawList)
{
	auto u = ImGui::GetFrameHeightWithSpacing() * (1.0f / 23.0f);
	magnifyingGlass(drawList, center, u);
	std::array<ImVec2, 2> line1 = {
		center + u * gl::vec2{-6.0f, -1.0f},
		center + u * gl::vec2{ 4.0f, -1.0f},
	};
	thinWhiteLine(drawList, line1);
}
static void RenderZoomIn(gl::vec2 center, ImDrawList* drawList)
{
	auto u = ImGui::GetFrameHeightWithSpacing() * (1.0f / 23.0f);
	magnifyingGlass(drawList, center, u);
	std::array<ImVec2, 2> line1 = {
		center + u * gl::vec2{-6.0f, -1.0f},
		center + u * gl::vec2{ 4.0f, -1.0f},
	};
	thinWhiteLine(drawList, line1);
	std::array<ImVec2, 2> line2 = {
		center + u * gl::vec2{-1.0f, -6.0f},
		center + u * gl::vec2{-1.0f,  4.0f},
	};
	thinWhiteLine(drawList, line2);
}
static void RenderPrevNegEdge(gl::vec2 center, ImDrawList* drawList)
{
	auto u = ImGui::GetFrameHeightWithSpacing() * (1.0f / 23.0f);
	std::array<ImVec2, 4> line1 = {
		center + u * gl::vec2{-9.0f, -9.0f},
		center + u * gl::vec2{-3.0f, -9.0f},
		center + u * gl::vec2{-3.0f,  9.0f},
		center + u * gl::vec2{ 3.0f,  9.0f},
	};
	thinWhiteLine(drawList, line1);
	arrowLeft(drawList, center, u);
}
static void RenderPrevPosEdge(gl::vec2 center, ImDrawList* drawList)
{
	auto u = ImGui::GetFrameHeightWithSpacing() * (1.0f / 23.0f);
	std::array<ImVec2, 4> line1 = {
		center + u * gl::vec2{-9.0f,  9.0f},
		center + u * gl::vec2{-3.0f,  9.0f},
		center + u * gl::vec2{-3.0f, -9.0f},
		center + u * gl::vec2{ 3.0f, -9.0f},
	};
	thinWhiteLine(drawList, line1);
	arrowLeft(drawList, center, u);
}
static void RenderPrevEdge(gl::vec2 center, ImDrawList* drawList)
{
	auto u = ImGui::GetFrameHeightWithSpacing() * (1.0f / 23.0f);
	std::array<ImVec2, 4> line1 = {
		center + u * gl::vec2{-9.0f,  9.0f},
		center + u * gl::vec2{-6.0f,  9.0f},
		center + u * gl::vec2{ 0.0f, -9.0f},
		center + u * gl::vec2{ 4.0f, -9.0f},
	};
	thinWhiteLine(drawList, line1);
	std::array<ImVec2, 4> line2 = {
		center + u * gl::vec2{-9.0f, -9.0f},
		center + u * gl::vec2{-6.0f, -9.0f},
		center + u * gl::vec2{ 0.0f,  9.0f},
		center + u * gl::vec2{ 4.0f,  9.0f},
	};
	thinWhiteLine(drawList, line2);
	arrowLeft(drawList, center, u);
}
static void RenderNextEdge(gl::vec2 center, ImDrawList* drawList)
{
	auto u = ImGui::GetFrameHeightWithSpacing() * (1.0f / 23.0f);
	std::array<ImVec2, 4> line1 = {
		center + u * gl::vec2{ 9.0f,  9.0f},
		center + u * gl::vec2{ 6.0f,  9.0f},
		center + u * gl::vec2{ 0.0f, -9.0f},
		center + u * gl::vec2{-4.0f, -9.0f},
	};
	thinWhiteLine(drawList, line1);
	std::array<ImVec2, 4> line2 = {
		center + u * gl::vec2{ 9.0f, -9.0f},
		center + u * gl::vec2{ 6.0f, -9.0f},
		center + u * gl::vec2{ 0.0f,  9.0f},
		center + u * gl::vec2{-4.0f,  9.0f},
	};
	thinWhiteLine(drawList, line2);
	arrowRight(drawList, center, u);
}
static void RenderNextNegEdge(gl::vec2 center, ImDrawList* drawList)
{
	auto u = ImGui::GetFrameHeightWithSpacing() * (1.0f / 23.0f);
	std::array<ImVec2, 4> line1 = {
		center + u * gl::vec2{ 9.0f,  9.0f},
		center + u * gl::vec2{ 3.0f,  9.0f},
		center + u * gl::vec2{ 3.0f, -9.0f},
		center + u * gl::vec2{-3.0f, -9.0f},
	};
	thinWhiteLine(drawList, line1);
	arrowRight(drawList, center, u);
}
static void RenderNextPosEdge(gl::vec2 center, ImDrawList* drawList)
{
	auto u = ImGui::GetFrameHeightWithSpacing() * (1.0f / 23.0f);
	std::array<ImVec2, 4> line1 = {
		center + u * gl::vec2{ 9.0f, -9.0f},
		center + u * gl::vec2{ 3.0f, -9.0f},
		center + u * gl::vec2{ 3.0f,  9.0f},
		center + u * gl::vec2{-3.0f,  9.0f},
	};
	thinWhiteLine(drawList, line1);
	arrowRight(drawList, center, u);
}

void ImGuiTraceViewer::zoomToFit(EmuTime minT, EmuDuration totalT)
{
	viewStartTime = minT;
	viewDuration = totalT;
}

void ImGuiTraceViewer::doZoom(double factor, double xFract)
{
	auto newDuration = std::max(viewDuration * factor, EmuDuration(100u));
	auto realFactor = newDuration.div(viewDuration);

	if (realFactor > 1.0) {
		auto offset = viewDuration * (xFract * (realFactor - 1.0));
		viewStartTime = viewStartTime.saturateSubtract(offset);
	} else {
		auto offset = viewDuration * (xFract * (1.0 - realFactor));
		viewStartTime += offset;
	}

	viewDuration = newDuration;
}

void ImGuiTraceViewer::scrollTo(EmuTime time)
{
	if (time < viewStartTime) {
		viewStartTime = time.saturateSubtract(viewDuration * 0.2);
	} else if (time >= viewStartTime + viewDuration) {
		viewStartTime = time.saturateSubtract(viewDuration * 0.8);
	}
}

[[nodiscard]] static const Tracer::Trace* getTrace(Traces traces, int selectedRow)
{
	if (selectedRow < 0 || selectedRow >= int(traces.size())) return nullptr;
	return traces[selectedRow];
}

[[nodiscard]] double ImGuiTraceViewer::getUnitConversionFactor() const
{
	switch (units) {
	default: // shouldn't happen
	case       SECONDS: return 1.0;
	case MILLI_SECONDS: return 1'000.0;
	case MICRO_SECONDS: return 1'000'000.0;
	case        CYCLES: return 3'579'545.0;
	}
}

[[nodiscard]] static const Tracer::Event* findStrictlySmaller(const Tracer::Trace& trace, EmuTime time)
{
	auto it = std::ranges::lower_bound(trace.events, time, {}, &Tracer::Event::time);
	if (it == trace.events.begin()) return nullptr;
	return std::to_address(it - 1);
}

[[nodiscard]] static const Tracer::Event* findStrictlyBigger(const Tracer::Trace& trace, EmuTime time)
{
	auto it = std::ranges::upper_bound(trace.events, time, {}, &Tracer::Event::time);
	if (it == trace.events.end()) return nullptr;
	return std::to_address(it);
}

void ImGuiTraceViewer::gotoPrevNegEdge(Traces traces, EmuTime& selectedTime)
{
	if (const auto* trace = getTrace(traces, selectedRow)) {
		auto time = selectedTime;
		while (true) {
			const auto* event = findStrictlySmaller(*trace, time);
			if (!event) return;
			time = event->time;
			if (!trace->isBool() || event->value.get_u64() == 0) break;
		}
		selectedTime = time;
		scrollTo(time);
	}
}

void ImGuiTraceViewer::gotoPrevPosEdge(Traces traces, EmuTime& selectedTime)
{
	if (const auto* trace = getTrace(traces, selectedRow)) {
		auto time = selectedTime;
		while (true) {
			const auto* event = findStrictlySmaller(*trace, time);
			if (!event) return;
			time = event->time;
			if (!trace->isBool() || event->value.get_u64() != 0) break;
		}
		selectedTime = time;
		scrollTo(time);
	}
}

void ImGuiTraceViewer::gotoPrevEdge(Traces traces, EmuTime& selectedTime)
{
	if (const auto* trace = getTrace(traces, selectedRow)) {
		if (const auto* event = findStrictlySmaller(*trace, selectedTime)) {
			selectedTime = event->time;
			scrollTo(event->time);
		}
	}
}

void ImGuiTraceViewer::gotoNextEdge(Traces traces, EmuTime& selectedTime)
{
	if (const auto* trace = getTrace(traces, selectedRow)) {
		if (const auto* event = findStrictlyBigger(*trace, selectedTime)) {
			selectedTime = event->time;
			scrollTo(event->time);
		}
	}
}

void ImGuiTraceViewer::gotoNextNegEdge(Traces traces, EmuTime& selectedTime)
{
	if (const auto* trace = getTrace(traces, selectedRow)) {
		auto time = selectedTime;
		while (true) {
			const auto* event = findStrictlyBigger(*trace, time);
			if (!event) return;
			time = event->time;
			if (!trace->isBool() || event->value.get_u64() == 0) break;
		}
		selectedTime = time;
		scrollTo(time);
	}
}

void ImGuiTraceViewer::gotoNextPosEdge(Traces traces, EmuTime& selectedTime)
{
	if (const auto* trace = getTrace(traces, selectedRow)) {
		auto time = selectedTime;
		while (true) {
			const auto* event = findStrictlyBigger(*trace, time);
			if (!event) return;
			time = event->time;
			if (!trace->isBool() || event->value.get_u64() != 0) break;
		}
		selectedTime = time;
		scrollTo(time);
	}
}

static void toolTipWithShortcut(ImGuiManager& manager, const char* text, Shortcuts::ID id)
{
	simpleToolTip([&] -> std::string {
		const auto& shortcuts = manager.getShortcuts();
		const auto& shortcut = shortcuts.getShortcut(id);
		if (shortcut.keyChord == ImGuiKey_None) return std::string(text);
		return strCat(text, " (", getKeyChordName(shortcut.keyChord), ')');
	});
}

[[nodiscard]] static std::string formatDouble(double value)
{
	std::array<char, 32> buffer; // 24 should also be sufficient, small safety margin
#if 0
	// doesn't work yet in MacOS :(
	auto [end, ec] = std::to_chars(
		buffer.data(), buffer.data() + buffer.size(),
		value, std::chars_format::general, 11);
	assert(ec == std::errc{}); // should never fail if we pass a sufficiently large buffer
	return std::string(buffer.data(), end - buffer.data());
#endif
	int len = snprintf(buffer.data(), buffer.size(), "%.17g", value);
	assert(len > 0 && size_t(len) < buffer.size());
	return std::string(buffer.data(), len);
}

void ImGuiTraceViewer::drawToolBar(EmuTime minT, EmuDuration totalT, float viewScreenWidth, Traces traces)
{
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
		if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
			auto& io = ImGui::GetIO();
			if (io.MouseWheel != 0.0f) {
				auto mx = io.MousePos.x - (ImGui::GetCursorScreenPos().x + col0_width + splitterWidth); // mouse x inside graph window
				auto xFract = mx / viewScreenWidth;
				doZoom(std::pow(1.2, double(-io.MouseWheel)), double(xFract));
				io.MouseWheel = 0.0f; // consume event
			}
		}
	}

	using enum Shortcuts::ID;
	const auto& shortcuts = manager.getShortcuts();
	if (shortcuts.checkShortcut(TRACE_SHOW_MENU_BAR)) {
		showMenuBar = !showMenuBar;
	}
	const auto& style = ImGui::GetStyle();
	gl::vec2 size{ImGui::GetFrameHeightWithSpacing()};
	if (ButtonWithCustomRendering("##ZoomFit", size, false, RenderZoomFit) ||
	    shortcuts.checkShortcut(TRACE_ZOOM_TO_FIT)) {
		zoomToFit(minT, totalT);
	}
	toolTipWithShortcut(manager, "Zoom to fit", TRACE_ZOOM_TO_FIT);
	ImGui::SameLine();

	if (ButtonWithCustomRendering("##ZoomOut", size, false, RenderZoomOut) ||
	    shortcuts.checkShortcut(TRACE_ZOOM_OUT)) {
		doZoom(1.2, 0.5);
	}
	toolTipWithShortcut(manager, "Zoom out", TRACE_ZOOM_OUT);
	ImGui::SameLine();

	if (ButtonWithCustomRendering("##ZoomIn", size, false, RenderZoomIn) ||
	    shortcuts.checkShortcut(TRACE_ZOOM_IN)) {
		doZoom(1.0 / 1.2, 0.5);
	}
	toolTipWithShortcut(manager, "Zoom in", TRACE_ZOOM_IN);
	ImGui::SameLine(0.0f, style.ItemSpacing.x * 2.0f);

	bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
	auto gotoStuff = [&](const char* label, const char* tooltip,
	                     Shortcuts::ID ID, Shortcuts::ID altID,
	                     auto renderFunc, auto action) {
		if (ButtonWithCustomRendering(label, size, false, renderFunc)) {
			std::invoke(action, this, traces, shift ? selectedTime2 : selectedTime1);
		}
		toolTipWithShortcut(manager, tooltip, shift ? altID : ID);
		if (shortcuts.checkShortcut(ID)) {
			std::invoke(action, this, traces, selectedTime1);
		}
		if (shortcuts.checkShortcut(altID)) {
			std::invoke(action, this, traces, selectedTime2);
		}
	};
	gotoStuff("##PrevNegEdge", "Goto previous negative edge",
		TRACE_PREV_NEG_EDGE, TRACE_ALT_PREV_NEG_EDGE,
		RenderPrevNegEdge, &ImGuiTraceViewer::gotoPrevNegEdge);
	ImGui::SameLine();
	gotoStuff("##PrevPosEdge", "Goto previous positive edge",
		TRACE_PREV_POS_EDGE, TRACE_ALT_PREV_POS_EDGE,
		RenderPrevPosEdge, &ImGuiTraceViewer::gotoPrevPosEdge);
	ImGui::SameLine();
	gotoStuff("##PrevEdge", "Goto previous edge (positive or negative)",
		TRACE_PREV_EDGE, TRACE_ALT_PREV_EDGE,
		RenderPrevEdge, &ImGuiTraceViewer::gotoPrevEdge);
	ImGui::SameLine();
	gotoStuff("##NextEdge", "Goto next edge (positive or negative)",
		TRACE_NEXT_EDGE, TRACE_ALT_NEXT_EDGE,
		RenderNextEdge, &ImGuiTraceViewer::gotoNextEdge);
	ImGui::SameLine();
	gotoStuff("##NextPosEdge", "Goto next positive edge",
		TRACE_NEXT_POS_EDGE, TRACE_ALT_NEXT_POS_EDGE,
		RenderNextPosEdge, &ImGuiTraceViewer::gotoNextPosEdge);
	ImGui::SameLine();
	gotoStuff("##NextNegEdge", "Goto next negative edge",
		TRACE_NEXT_NEG_EDGE, TRACE_ALT_NEXT_NEG_EDGE,
		RenderNextNegEdge, &ImGuiTraceViewer::gotoNextNegEdge);
	HelpMarker("Place a primary (or secondary) marker by clicking (or shift-click) on the graph below.\n"
	           "Advance the markers to the previous/next (positive or negative) edge via the buttons on the left, "
	           "(normal-click for the primary marker, shift-click for the secondary marker).\n"
	           "On the right the position and distance between the markers is shown. "
	           "The units can be selected via the dropdown list in the top-right corner.");
	ImGui::SameLine(0.0f, style.ItemSpacing.x * 2.0f);

	auto unitFactor = getUnitConversionFactor();
	if (!timelineRelative) {
		ImGui::TextUnformatted("prim."sv);
		simpleToolTip("Primary marker (green). Place via left-mouse-click on the graph.");
		ImGui::SameLine();
		auto leftStr = formatDouble(selectedTime1.toDouble() * unitFactor);
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
		if (ImGui::InputText("##left", &leftStr)) {
			if (auto d = StringOp::stringTo<double>(leftStr)) {
				selectedTime1 = EmuTime::fromDouble(*d / unitFactor);
			}
		}
		ImGui::SameLine();
		ImGui::TextUnformatted("sec."sv);
		simpleToolTip("Secondary marker (red). Place via shift-left-mouse-click on the graph.");
		ImGui::SameLine();
		auto rightStr = formatDouble(selectedTime2.toDouble() * unitFactor);
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
		if (ImGui::InputText("##right", &rightStr)) {
			if (auto d = StringOp::stringTo<double>(rightStr)) {
				selectedTime2 = EmuTime::fromDouble(*d / unitFactor);
			}
		}
		ImGui::SameLine();
	}
	ImGui::TextUnformatted("dist."sv);
	simpleToolTip("Distance between primary and secondary markers.");
	ImGui::SameLine();
	im::Disabled(true, [&]{
		auto distStr = formatDouble((selectedTime2.toDouble() - selectedTime1.toDouble()) * unitFactor);
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
		ImGui::InputText("##dist", &distStr, ImGuiInputTextFlags_ReadOnly);
	});
	ImGui::SameLine();

	ImGui::TextUnformatted("units"sv);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
	std::array shortUnitNames = {
		"sec", "ms", "\u00b5s", "cycles",
	};
	std::array longUnitNames = {
		"sec (seconds)", "ms (milliseconds)", "\u00b5s (microseconds)", "cycles (Z80 @3.57MHz)",
	};
	im::Combo("##units", shortUnitNames[units], [&]{
		for (int i = 0; i < Units::NUM; ++i) {
			if (ImGui::Selectable(longUnitNames[i])) {
				units = i;
			}
		}
	});
}

void ImGuiTraceViewer::drawNames(Traces traces, float rulerHeight, float rowHeight, int mouseRow, Debugger& debugger)
{
	int windowFlags = ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoScrollWithMouse
			| ImGuiWindowFlags_AlwaysHorizontalScrollbar;
	im::Child("##col0", gl::vec2{col0_width, 0.0f}, 0, windowFlags, [&] {
		const auto& style = ImGui::GetStyle();
		const auto& io = ImGui::GetIO();

		auto pos0 = ImGui::GetCursorPos();
		if (ImGui::Button("Select ...")) showSelect = true;
		ImGui::SetCursorPos(pos0);

		ImGui::Dummy({10.0f, rulerHeight});
		auto cPos = ImGui::GetCursorPos();
		ImGui::SetScrollY(scrollY);

		auto* drawList = ImGui::GetWindowDrawList();
		gl::vec2 clipMin = drawList->GetClipRectMin() + gl::vec2{0.0f, rulerHeight};
		gl::vec2 clipMax = drawList->GetClipRectMax();
		bool hovered = (clipMin.x <= io.MousePos.x) && (io.MousePos.x < clipMax.x) &&
		               (clipMin.y <= io.MousePos.y) && (io.MousePos.y < clipMax.y);

		std::optional<int> dragUp;
		std::optional<int> dragDown;
		drawList->PushClipRect(clipMin, clipMax);
		auto colorText = getColor(imColor::TEXT);
		im::ListClipper(traces.size(), -1, rowHeight, [&](int row) {
			auto tl = clipMin + gl::vec2{0.0f, float(row) * rowHeight};
			bool rowHovered = hovered && (row == mouseRow);
			if (rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				selectedRow = row;
			}
			if (auto color = rowHovered           ? std::optional<ImU32>(ImGui::GetColorU32(ImGuiCol_HeaderHovered))
			               : (row == selectedRow) ? std::optional<ImU32>(ImGui::GetColorU32(ImGuiCol_Header))
			                                      : std::optional<ImU32>{}) {
				drawList->AddRectFilled(tl, tl + gl::vec2{col0_width, rowHeight}, *color);
			}
			const auto* trace = traces[row];
			const auto& name = trace->name;
			drawText(drawList, tl + gl::vec2{0.0f, style.FramePadding.y + style.ItemSpacing.y},
				 colorText, traces[row]->name);

			ImGui::SetCursorPos(cPos + gl::vec2{0.0f, float(row) * rowHeight});
			ImGui::InvisibleButton(name.c_str(), gl::vec2{col0_width, rowHeight});
			if (ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
				auto dy = ImGui::GetMouseDragDelta(0).y;
				auto dist = rowHeight * 0.5f;
				if (dy > dist) {
					dragDown = row;
				} else if (dy < -dist) {
					dragUp = row;
				}
			}
			if (!trace->isUserTrace()) { // a probe
				simpleToolTip([&]{
					if (auto probe = debugger.findProbe(name)) {
						return probe->getDescription();
					} else {
						return ""sv;
					}
				});
			}
		});
		drawList->PopClipRect();

		auto doSwap = [&](int idx1, int idx2) {
			assert(idx1 < idx2);
			auto name1 = traces[idx1]->name;
			auto name2 = traces[idx2]->name;
			auto it1 = std::ranges::find(selectedTraces, name1, &SelectedTrace::name);
			auto it2 = std::ranges::find(it1 + 1, selectedTraces.end(), name2, &SelectedTrace::name);
			if (it1 != selectedTraces.end() && it2 != selectedTraces.end()) {
				std::iter_swap(it1, it2);
			}
		};
		if (dragUp && *dragUp >= 1) {
			doSwap(*dragUp - 1, *dragUp);
			if (selectedRow == *dragUp) --selectedRow;
		} else if (dragDown && *dragDown < int(traces.size() - 1)) {
			doSwap(*dragDown, *dragDown + 1);
			if (selectedRow == *dragDown) ++selectedRow;
		}
	});
}

void ImGuiTraceViewer::drawSplitter(float width)
{
	const auto& io = ImGui::GetIO();

	ImGui::SameLine(0.0f, 0.0f);

	auto pos = ImGui::GetCursorScreenPos();
	auto avail = ImGui::GetContentRegionAvail();
	ImGui::InvisibleButton("##splitter", ImVec2(width, -1));
	bool hovered = ImGui::IsItemHovered();
	bool active = ImGui::IsItemActive();
	if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		col0_width = std::clamp(col0_width + io.MouseDelta.x, 50.0f, ImGui::GetContentRegionAvail().x - 50.0f);
	}
	if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

	auto x_center = pos.x + (width - 1.0f) * 0.5f;
	auto y0 = pos.y;
	auto y1 = y0 + avail.y;

	auto col_sep = ImGui::GetColorU32(ImGuiCol_Separator);
	auto col_sep_active = ImGui::GetColorU32(ImGuiCol_SeparatorActive);
	auto col_hover = ImGui::GetColorU32(ImGuiCol_SeparatorHovered);
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(x_center, y0), ImVec2(x_center, y1), active ? col_sep_active : (hovered ? col_hover : col_sep), 1.0f);

	ImGui::SameLine(0.0f, 0.0f);
}

void ImGuiTraceViewer::drawRuler(gl::vec2 size, const Convertor& convertor, EmuTime from, EmuTime to)
{
	auto labelSize = ImGui::CalcTextSize("0123456789"sv);
	auto numLabels = size.x / labelSize.x;
	auto origin = timelineRelative ? selectedTime1.toDouble() : 0.0;
	auto unitFactor = getUnitConversionFactor();
	auto [labels, step] = timelineFormatter.calc(
		from.toDouble() - origin, to.toDouble() - origin, double(numLabels), unitFactor);
	auto minorStep = step * 0.1 / unitFactor;

	auto* drawList = ImGui::GetWindowDrawList();
	auto colorText = getColor(imColor::TEXT);
	auto colorTick = ImGui::GetColorU32(ImGuiCol_Separator);
	auto y0 = size.y * 0.67f;
	auto y1 = size.y * 0.85f;
	auto y2 = size.y;
	gl::vec2 clipMin = drawList->GetClipRectMin();
	gl::vec2 clipMax = drawList->GetClipRectMax();
	drawList->AddLine({clipMin.x, clipMin.y + y0}, {clipMax.x, clipMin.y + y0}, colorTick);

	for (const auto& l : labels) {
		const auto& text = l.text;
		auto value = l.value + origin;
		auto textSize = ImGui::CalcTextSize(text);
		auto centerX = convertor.timeToX(EmuTime::fromDouble(value));
		drawText(drawList, clipMin + gl::vec2{centerX - textSize.x * 0.5f, scrollY}, colorText, text);
		for (auto i : xrange(10)) {
			auto x = convertor.timeToX(EmuTime::fromDouble(value + double(i) * minorStep));
			drawList->AddLine(clipMin + gl::vec2{x, y0}, clipMin + gl::vec2{x, (i % 5) == 0 ? y2 : y1}, colorTick);
		}
	}
}

static EmuTime snapToEvent(float mouseX, auto convertor, std::span<const Tracer::Event> events)
{
	auto time = convertor.xToTime(mouseX);
	auto [it, dist] = find_closest(events, time, {}, &Tracer::Event::time);
	if (it == events.end()) return time;
	auto h5 = ImGui::GetFrameHeight() * 0.20f;
	return (dist < convertor.deltaXtoDuration(h5)) ? it->time : time;
}

void ImGuiTraceViewer::drawGraphs(Traces traces, float rulerHeight, float rowHeight, int mouseRow, const Convertor& convertor,
                                  EmuTime minT, EmuTime maxT, EmuTime now)
{
	if        (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat)) {
		viewStartTime = viewStartTime.saturateSubtract(convertor.deltaXtoDuration(0.5f * convertor.viewScreenWidth));
	} else if (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_RightArrow, ImGuiInputFlags_Repeat)) {
		viewStartTime += convertor.deltaXtoDuration(0.5f * convertor.viewScreenWidth);
	} else if (ImGui::Shortcut(ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat)) {
		viewStartTime = viewStartTime.saturateSubtract(convertor.deltaXtoDuration(12.0f));
	} else if (ImGui::Shortcut(ImGuiKey_RightArrow, ImGuiInputFlags_Repeat)) {
		viewStartTime += convertor.deltaXtoDuration(12.0f);
	}

	auto totalT = maxT - minT;
	auto total1000 = totalT * 1e-3;
	auto [scrollWidth, scrollPos] = [&]{
		// if we make the scroll bar range TOO wide, ImGui has precision issues
		if (total1000 < viewDuration) {
			auto scrollSlackDuration = (viewDuration < totalT) ? (totalT - viewDuration) : EmuDuration::zero();
			auto width = float(scrollSlackDuration.div(viewDuration)) + 1.0f;
			auto pos = float((viewStartTime - minT).div(viewDuration));
			return std::pair(width, pos);
		} else {
			auto width = 1000.0f;
			auto pos = float((viewStartTime - minT).div(total1000));
			return std::pair(width, pos);
		}
	}();
	float fakeScrollWidth = scrollWidth * convertor.viewScreenWidth;
	float fakeScrollPos = scrollPos * convertor.viewScreenWidth;
	ImGui::SetNextWindowContentSize({fakeScrollWidth, rulerHeight + rowHeight * float(traces.size())});
	ImGui::SetNextWindowScroll({fakeScrollPos, scrollY});

	int windowFlags2 = ImGuiWindowFlags_HorizontalScrollbar
	                 | ImGuiWindowFlags_AlwaysHorizontalScrollbar;
	im::Child("Events", {}, {}, windowFlags2, [&] {
		scrollY = ImGui::GetScrollY();
		auto scrollX = ImGui::GetScrollX();
		if (std::abs(scrollX - fakeScrollPos) > 0.5f) {
			// user changed scroll position
			auto dur = std::max(total1000, viewDuration);
			viewStartTime = minT + (dur * double(scrollX / convertor.viewScreenWidth));
		}

		const auto& style = ImGui::GetStyle();
		const auto& io = ImGui::GetIO();

		auto t0 = convertor.xToTime(0.0f);
		auto t1 = convertor.xToTime(convertor.viewScreenWidth);

		drawRuler({convertor.viewScreenWidth, rulerHeight}, convertor, t0, t1);
		ImGui::Dummy({10.0f, rulerHeight});

		auto* drawList = ImGui::GetWindowDrawList();
		gl::vec2 clipMin = drawList->GetClipRectMin() + gl::vec2{0.0f, rulerHeight};
		gl::vec2 clipMax = drawList->GetClipRectMax();
		bool hovered = (clipMin.x <= io.MousePos.x) && (io.MousePos.x < clipMax.x) &&
		               (clipMin.y <= io.MousePos.y) && (io.MousePos.y < clipMax.y) &&
		               ImGui::IsWindowHovered();
		auto mouseX = io.MousePos.x - clipMin.x;

		drawList->PushClipRect(clipMin, clipMax);
		im::ListClipperID(traces.size(), rowHeight, [&](int row) {
			const auto& trace = *traces[row];
			auto it0 = std::ranges::lower_bound(trace.events, viewStartTime, {}, &Tracer::Event::time);
			if (it0 != trace.events.begin()) --it0;
			auto graphEndTime = viewStartTime + viewDuration;
			auto it1 = std::ranges::upper_bound(trace.events, graphEndTime, {}, &Tracer::Event::time);
			std::span<const Tracer::Event> visibleEvents(it0, it1);

			bool rowHovered = hovered && (row == mouseRow);
			if (rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) {
					selectedTime2 = snapToEvent(mouseX, convertor, visibleEvents);
				} else {
					selectedRow = row;
					selectedTime1 = snapToEvent(mouseX, convertor, visibleEvents);
				}
			}
			auto tl = clipMin + gl::vec2{0.0f, float(row) * rowHeight};
			if (auto color = rowHovered           ? std::optional<ImU32>(ImGui::GetColorU32(ImGuiCol_HeaderHovered))
			               : (row == selectedRow) ? std::optional<ImU32>(ImGui::GetColorU32(ImGuiCol_Header))
			                                      : std::optional<ImU32>{}) {
				drawList->AddRectFilled(tl, {clipMax.x, tl.y + rowHeight}, *color);
			}
			tl.y += style.ItemSpacing.y;
			switch (trace.getFormat()) {
			case Tracer::Trace::Format::MONOSTATE:
				drawEventsVoid(tl, convertor, maxT, visibleEvents, rowHovered);
				break;
			case Tracer::Trace::Format::BOOL:
				drawEventsBool(tl, convertor, maxT, visibleEvents, rowHovered);
				break;
			default:
				drawEventsValue(tl, convertor, maxT, visibleEvents, rowHovered);
			}
		});
		auto drawLine = [&](EmuTime t, ImU32 color) {
			if (t != EmuTime::infinity()) {
				auto x = float(convertor.timeToX(t) + clipMin.x);
				drawList->AddLine({x, clipMin.y}, {x, clipMax.y}, color);
			}
		};
		drawLine(selectedTime1, IM_COL32(0, 255, 0, 255)); // green
		drawLine(selectedTime2, IM_COL32(255, 0, 0, 255)); // red

		if (now < maxT) {
			auto x = convertor.timeToX(now) + clipMin.x;
			drawList->AddRectFilled({x, clipMin.y}, clipMax, IM_COL32(0, 0, 0, 128));
		}

		drawList->PopClipRect();
	});
}

void ImGuiTraceViewer::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	ImGui::SetNextWindowSize(gl::vec2{68, 29} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	int flags = showMenuBar ? ImGuiWindowFlags_MenuBar : 0;
	im::Window("Probe/Trace Viewer", &show, flags, [&] {
		paintMain(*motherBoard);
	});

	if (showSelect) {
		ImGui::SetNextWindowSize(gl::vec2{18, 27} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
		im::Window("Select probes and traces", &showSelect, [&] {
			paintSelect(*motherBoard);
		});
	}
	if (showHelp) {
		ImGui::SetNextWindowSize(gl::vec2{50, 38} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
		im::Window("Trace Viewer Help", &showHelp, [&] {
			paintHelp();
		});
	}
}

void ImGuiTraceViewer::paintMain(MSXMotherBoard& motherBoard)
{
	auto& debugger = motherBoard.getDebugger();
	auto& tracer = debugger.getTracer();
	const auto& allTraces = tracer.getTraces();

	for (const auto& trace : allTraces) {
		bool isProbe = !trace->isUserTrace();
		if (!isProbe || allUserTraces) {
			// TODO this is O(N^2), ok?
			if (!contains(selectedTraces, trace->name, &SelectedTrace::name)) {
				selectedTraces.emplace_back(trace->name, isProbe);
			}
		}
	}
	std::vector<Tracer::Trace*> traces;
	traces.reserve(selectedTraces.size());
	for (const auto& sel : selectedTraces) {
		if (allUserTraces || sel.selected) {
			if (auto it = std::ranges::find(allTraces, sel.name, &Tracer::Trace::name);
			it != allTraces.end()) {
				traces.push_back(it->get());
			}
		}
	}

	if (ImGui::Shortcut(ImGuiKey_UpArrow, ImGuiInputFlags_Repeat)) {
		if (selectedRow > 0) --selectedRow;
	} else if (ImGui::Shortcut(ImGuiKey_DownArrow, ImGuiInputFlags_Repeat)) {
		if (selectedRow < int(traces.size() - 1)) ++selectedRow;
	}

	EmuTime minT = EmuTime::infinity();
	EmuTime maxT = EmuTime::zero();
	if (timelineStart || timelineStop) {
		for (const auto& trace : traces) {
			if (trace->events.empty()) continue;
			minT = std::min(minT, trace->events.front().time);
			maxT = std::max(maxT, trace->events.back().time);
		}
		if (minT > maxT) minT = maxT;
	}

	static constexpr auto margin = EmuDuration::sec(0.02);
	auto now = motherBoard.getCurrentTime();
	minT = timelineStart ? minT : EmuTime::zero();
	minT = std::min(minT, viewStartTime);
	maxT = timelineStop ? maxT : now;
	maxT = std::max(maxT, viewStartTime + viewDuration);
	maxT += margin; // add margin at end
	if (maxT < minT) maxT = minT + margin;
	auto totalT = maxT - minT;

	auto viewScreenWidth = ImGui::GetContentRegionAvail().x - col0_width - splitterWidth;

	if (showMenuBar) {
		im::MenuBar([&]{
			drawMenuBar(minT, totalT, tracer, traces);
		});
	}
	drawToolBar(minT, totalT, viewScreenWidth, traces);
	ImGui::Separator();

	const auto& io = ImGui::GetIO();
	const auto& style = ImGui::GetStyle();
	float rulerHeight = ImGui::GetTextLineHeight() * 1.5f;
	auto rowHeight = ImGui::GetFrameHeight() + 2.0f * style.ItemSpacing.y;
	auto mouseY = io.MousePos.y - ImGui::GetCursorScreenPos().y + scrollY - rulerHeight;
	auto mouseRow = static_cast<int>(mouseY / rowHeight);
	if (mouseRow < 0 || mouseRow >= int(traces.size())) mouseRow = -1;

	drawNames(traces, rulerHeight, rowHeight, mouseRow, debugger);
	drawSplitter(splitterWidth);
	Convertor convertor{viewStartTime, viewDuration, viewScreenWidth};
	drawGraphs(traces, rulerHeight, rowHeight, mouseRow, convertor, minT, maxT, now);
}

void ImGuiTraceViewer::paintSelect(MSXMotherBoard& motherBoard)
{
	auto& debugger = motherBoard.getDebugger();
	auto& tracer = debugger.getTracer();
	const auto& traces = tracer.getTraces();

	const auto& style = ImGui::GetStyle();
	auto rightSpace = ImGui::CalcTextSize("(?)").x + style.ItemSpacing.x;
	auto boxHeight = ImGui::GetFrameHeightWithSpacing() * 4.7f;

	ImGui::SeparatorText("Probes");
	im::Child("##probes", {-rightSpace, boxHeight}, ImGuiChildFlags_Borders, [&]{
		auto& allProbes = debugger.getProbes();
		im::ListClipper(allProbes.size(), [&](int i) {
			const auto& probe = *allProbes[i];
			const auto& name = probe.getName();
			bool selected = contains(traces, name, &Tracer::Trace::name);
			if (ImGui::Checkbox(name.c_str(), &selected)) {
				if (selected) {
					tracer.selectProbe(debugger, name);
					if (!contains(selectedTraces, name, &SelectedTrace::name)) {
						selectedTraces.emplace_back(name, true);
					}
				} else {
					tracer.unselectProbe(debugger, name);
					if (auto it = std::ranges::find(selectedTraces, name, &SelectedTrace::name);
						it != selectedTraces.end()) {
						selectedTraces.erase(it);
					}
				}
			}
			simpleToolTip(probe.getDescription());
		});
	});
	HelpMarker(
		"Probes are provided by openMSX (builtin). "
		"The available probes depend on the selected MSX machine and extensions.\n"
		"Some probes generate A LOT of data, therefor there's no option to automatically enable all.");

	ImGui::SeparatorText("User traces");
	if (ImGui::RadioButton("Auto select all", allUserTraces)) {
		allUserTraces = true;
	}
	HelpMarker("Automatically select all user defined traces. Including when more traces are created later.");
	if (ImGui::RadioButton("Manual select", !allUserTraces)) {
		allUserTraces = false;
	}
	im::DisabledIndent(allUserTraces, [&]{
		im::Child("##traces", {-rightSpace, boxHeight}, ImGuiChildFlags_Borders, [&]{
			const auto& allTraces = tracer.getTraces();
			std::vector<zstring_view> userTraceNames;
			userTraceNames.reserve(allTraces.size());
			for (const auto& trace : traces) {
				if (trace->isUserTrace()) {
					userTraceNames.emplace_back(trace->name);
				}
			}
			im::ListClipper(userTraceNames.size(), [&](int i) {
				const auto& name = userTraceNames[i];
				auto it1 = std::ranges::find(selectedTraces, name, &SelectedTrace::name);
				bool selected = it1 != selectedTraces.end() && it1->selected;
				if (ImGui::Checkbox(name.c_str(), &selected)) {
					if (selected) {
						if (auto it = std::ranges::find(selectedTraces, name, &SelectedTrace::name);
							it != selectedTraces.end()) {
							it->selected = true;
						} else {
							selectedTraces.emplace_back(std::string(name), true);
						}
					} else {
						if (auto it = std::ranges::find(selectedTraces, name, &SelectedTrace::name);
							it != selectedTraces.end()) {
							it->selected = false;
						}
					}
				}
				// TODO tooltip if we extend 'debug trace add' with '-description'
			});
		});
		HelpMarker(
			"Traces are created by the user, via the 'debug trace add' command. "
			"Typically trace data is added via breakpoints or watchpoints.\n"
			"For example in the breakpoint editor set a breakpoint on your function, "
			"and then in the action column (possibly unhide via right-click the column header) "
			"enter the command 'debug trace add myFunction [reg A]'. "
			"This will add a trace point whenever your function is called, "
			"and it will log the value of register A");
	});
}

static inline void RenderParagraph(std::string_view text)
{
	im::TextWrapPos([&]{
		ImGui::TextUnformatted(text);
	});
}

static inline void RenderBullet(std::string_view text)
{
	ImGui::Bullet();
	ImGui::SameLine();
	RenderParagraph(text);
}

static inline void TextUnformattedUnderlined(std::string_view text)
{
	ImGui::TextUnformatted(text);
	auto size = ImGui::CalcTextSize(text);
	auto p = ImGui::GetItemRectMin();
	auto start = gl::vec2{p.x, p.y + size.y + 2.0f};
	auto end = gl::vec2{p.x + size.x, start.y};
	auto color = getColor(imColor::TEXT);
	ImGui::GetWindowDrawList()->AddLine(start, end, color);
	ImGui::NewLine();
}

void ImGuiTraceViewer::paintHelp()
{
	if (helpSection == HelpSection::OVERVIEW) {
		ImGui::SetScrollHereY(0.0f);
	}
	TextUnformattedUnderlined("Overview");
	RenderParagraph(
		"The 'Probe/Trace Viewer' is an openMSX debugging tool to visualize events "
		"(and optionally associated data) on a timeline. This can help answer "
		"questions like:");
	RenderBullet(
		"How much time passed between the VDP generating a VBLANK IRQ and the Z80 responding?");
	RenderBullet(
		"When is the VDP busy executing commands? Can I change my code to increase "
		"utilization (less idle time)?");
	RenderBullet(
		"When is my function called and what parameters were passed?");

	RenderParagraph(
		"First, you need to tell openMSX what information to collect. This can be in the "
		"form of builtin openMSX 'probes' or user-defined 'traces'. See the section "
		"'Probes and traces' for more details. In some cases collecting this information "
		"can be expensive. That's why openMSX does not collect everything by default "
		"(which would also clutter the view).");
	RenderParagraph(
		"Next, run your test case. During this process you'll see the graphs being "
		"created. Typically, after some interesting data has been collected you'll "
		"want to pause emulation and examine the graph in detail. See the 'Graphs' "
		"section.");
	RenderParagraph(
		"The main purpose of this tool is to analyze the collected data. Often this "
		"involves cross-correlating multiple data sources. For example, how many "
		"cycles are there between entering 'myFuncA' and starting the next VDP "
		"command? When was the last IRQ before 'myFuncB'? See the 'Navigation' "
		"section.");
	RenderParagraph(
		"If this is still a bit abstract, you can read the 'Example' section for a "
		"more concrete example.");
	ImGui::NewLine();
	ImGui::NewLine();

	if (helpSection == HelpSection::PROBES_AND_TRACES) {
		ImGui::SetScrollHereY(0.0f);
	}
	TextUnformattedUnderlined("Probes and Traces");
	RenderParagraph(
		"As mentioned in the 'Overview' section, the first step is to select the "
		"information sources. This is done via the 'Select probes and traces' window. "
		"That window can be opened via 'menu bar > File > Select probes and traces', "
		"or by clicking the 'Select...' button in the left column.");
	RenderParagraph(
		"This window has two main parts: 'Probes' and 'User traces'. The 'Probes' part "
		"lists builtin openMSX probes with checkboxes to enable collection. The 'User "
		"traces' part offers 'Auto select all' or 'Manual select'. 'Manual select' shows "
		"currently defined user traces with checkboxes. 'Auto select all' selects all "
		"user traces, including ones not yet created (and thus not yet visible in "
		"the list).");
	RenderParagraph(
		"Both probes and traces generate timestamped events. Some events only record a "
		"timestamp (for example 'z80.accept.IRQ'). Other events carry extra information: "
		"'VDP.commandExecuting' also carries a boolean indicating whether a command "
		"started or stopped. User traces may carry arbitrary additional data (boolean, "
		"integer, float, string), for example function parameters.");
	RenderParagraph(
		"Probes are builtin low-level events provided by openMSX; traces are "
		"user-defined and typically project-specific. Because some builtin probes "
		"generate many events or are expensive to collect, enable only those you "
		"need; traces are usually smaller and tailored to your debugging needs.");
	RenderParagraph(
		"openMSX currently provides generic probes for IRQs and the VDP command "
		"engine, providing low-level information not otherwise exposed via "
		"debuggables. Future openMSX versions may add more builtin probes.");
	RenderParagraph(
		"Traces are user-defined and often attached to breakpoints or watchpoints. "
		"Create a trace data point with the Tcl command:");
	im::Font(manager.fontMono, [&]{ im::Indent([&]{ RenderParagraph(
		"debug trace add <trace-name> [<value>]"); }); });
	RenderParagraph(
		"Execute 'help debug trace' and 'help debug trace add' in the console for "
		"details. Here's a full example, attach to a breakpoint and log register A as data:");
	im::Font(manager.fontMono, [&]{ im::Indent([&]{ RenderParagraph(
		"debug breakpoint create -address 0x1234 -command {debug trace add "
		"myFunction [reg A]}"); }); });
	RenderParagraph(
		"This logs a data point for the trace named 'myFunction' whenever the breakpoint at address 0x1234 "
		"triggers. Using 'debug trace add ...' as the breakpoint '-command' records the trace "
		"instead of stopping emulation. The '[reg A]' expression logs the current value "
		"of the Z80 register A.");
	RenderParagraph(
		"You can create the breakpoint via the above Tcl command or via the GUI "
		"(Debugger > Breakpoints > Editor). Enter the action in the 'Action' column "
		"(unhide that column from the table header context menu if needed).");
	ImGui::NewLine();
	ImGui::NewLine();

	if (helpSection == HelpSection::GRAPHS) {
		ImGui::SetScrollHereY(0.0f);
	}
	TextUnformattedUnderlined("Graphs");
	RenderParagraph(
		"Once probes/traces are selected and data has been collected (by running "
		"a test case), the results appear in the main area of the Probe/Trace "
		"Viewer.");
	RenderParagraph(
		"The left column lists probe/trace names, the right area shows the "
		"timeline and the recorded data.");
	RenderParagraph(
		"There are three data types, each rendered differently:");
	RenderBullet(
		"Event-only: timestamped events without extra data, shown as small up-arrows.");
	RenderBullet(
		"Boolean: timestamped 0/1 transitions, drawn as a square wave (edges at transitions).");
	RenderBullet(
		"Value traces: events carrying integers, floats, or strings; shown as "
		"elongated hexagons that span until the next value, with the value text "
		"inside.");

	RenderParagraph(
		"By default the graph order follows the selection order, but drag names in the "
		"left column to reorder.");
	RenderParagraph(
		"If the collected information does not fit on screen, zoom or scroll to inspect it:");
	RenderBullet(
		"Zoom: via 'View > Zoom', toolbar icons, or shortcuts (default: Ctrl-=, Ctrl--, Ctrl-0).");
	RenderBullet(
		"Scroll: via bottom scrollbar or Left/Right cursor keys (use Shift + Left/Right for faster movement).");

	RenderParagraph("Markers:");
	RenderBullet("Primary marker (green): left-click near an edge to place or snap it.");
	RenderBullet("Secondary marker (red): shift-left-click near an edge to place or snap it.");

	RenderParagraph(
		"Near the top are two text fields showing the marker positions, edit "
		"them to move the markers. A third read-only field shows the distance "
		"between the markers. Use the units dropdown to choose seconds or Z80 "
		"clock cycles.");
	RenderParagraph(
		"Use 'View > Timeline > start/stop' to show the full run (from emulation start to current time) "
		"or only the interval containing collected events. Showing only the "
		"interval with events is useful when debugging a short section of a "
		"long-running program.");
	RenderParagraph(
		"Use 'View > Timeline > Origin' to place the origin at start of emulation (this gives "
		"stable time points) or at the primary marker (this gives shorter time values).");
	ImGui::NewLine();
	ImGui::NewLine();

	if (helpSection == HelpSection::NAVIGATION) {
		ImGui::SetScrollHereY(0.0f);
	}
	TextUnformattedUnderlined("Navigation");
	RenderParagraph(
		"An important feature of the 'Probe/Trace Viewer' is measuring and relating "
		"events across traces (for example, measuring distances or the ordering "
		"between events). The UI offers the following helpers.");
	RenderParagraph(
		"(Shift-)left-click near an edge snaps the primary (or secondary) "
		"marker to that edge, allowing precise distance measurements between "
		"events.");
	RenderParagraph("Navigation functions (six total):");
	RenderBullet("Go to next/previous (any) edge");
	RenderBullet("Go to next/previous positive (=rising) edge");
	RenderBullet("Go to next/previous negative (=falling) edge");

	RenderParagraph(
		"Positive/negative are only relevant for boolean traces: a positive edge is "
		"a transition from low (0) to high (1); a negative edge is the opposite. "
		"For example, in VDP.commandExecuting a positive edge marks when a "
		"command starts and the negative edge when it finishes.");
	RenderParagraph(
		"These functions are available as six toolbar icons. Left-click an icon to "
		"move the primary marker; shift-left-click to move the secondary marker. "
		"Keyboard shortcuts are available (default: Ctrl+A,S,D,F,G,H, six "
		"consecutive keys, same order as the toolbar icons). Ctrl alone moves "
		"the primary marker; Ctrl+Shift moves the secondary marker.");
	RenderParagraph(
		"You can relate events across graphs using the navigation functions. For "
		"example, locate a rising edge in 'VDP.IRQvertical' (when the VDP asserts the "
		"VBLANK IRQ). Select the 'z80.acceptIRQ' row without moving the marker (click "
		"the row name or use the up/down cursor keys), then go to the next event to "
		"see when the Z80 accepted the interrupt (the Z80 will jump to the ISR, "
		"typically at address 0x0038). Selecting 'VDP.IRQvertical' again and advancing to "
		"the next edge shows when the ISR cleared the VDP IRQ.");
	ImGui::NewLine();
	ImGui::NewLine();

	if (helpSection == HelpSection::EXAMPLE) {
		ImGui::SetScrollHereY(0.0f);
	}
	TextUnformattedUnderlined("Example");
	RenderParagraph(
		"Hopefully the other help sections have made the purpose of the 'Probe/Trace "
		"Viewer' clear. But if it's still too abstract then here's an example. To "
		"avoid dependencies on external MSX software, we'll analyze the MSX-BASIC "
		"startup sequence.");
	RenderParagraph(
		"First we need to do some preparation:");
	RenderBullet(
		"Start a Philips NMS 8255 machine (other machines work too but may need slight adjustments).");
	RenderBullet(
		"Pause emulation quickly (before or during the boot logo) so you have "
		"time to set up probes and traces.");
	RenderParagraph(
		"Open the openMSX console (press F10 or 'Tools > Show console') and enter:");
	im::Font(manager.fontMono, [&]{ im::Indent([&]{ RenderParagraph(
		"debug breakpoint create -address 0x0919 -command {debug trace add chput "
		"[format \"%c (%d)\" [reg a] [reg a]]}"); }); });
	RenderParagraph(
		"Alternatively, create the breakpoint via 'Debugger > Breakpoints > Editor': "
		"click 'Add', enter '0x0919' in the 'Address' column, and put the 'debug trace "
		"add ...' command in the 'Action' column (unhide that column if needed). 0x0919 "
		"is the CHPUT BIOS function entry used by MSX-BASIC (MSX-BASIC calls this "
		"address directly rather than the stable entry point at 0x00A2). The 'debug "
		"trace add' command records a data point for the user-defined 'chput' trace; "
		"the 'format' expression prints register 'A' both as a character and as a "
		"decimal integer.");
	RenderParagraph(
		"Open the Probe/Trace Viewer (Debugger > Probe/Trace viewer) and click "
		"'Select...' in the left column (or use 'File > Select probes and traces').");
	RenderParagraph(
		"In the selector enable 'VDP.IRQvertical' and 'z80.acceptIRQ' (and any other "
		"probes you're interested in). In the 'User traces' part, enable 'Auto select all' so your "
		"'chput' trace will appear once it is recorded.");
	RenderParagraph(
		"Now resume emulation and wait for MSX-BASIC to finish booting, then pause again.");
	RenderParagraph("Inspecting the data:");
	RenderBullet(
		"Use Zoom to fit ('toolbar > magnifier' or 'Ctrl-0') for an overview.");
	RenderBullet(
		"Zoom into the dense region using the toolbar or by hovering the mouse "
		"over the region and using Ctrl + mouse wheel.");
	RenderBullet(
		"At sufficient zoom you'll see individual printed characters and their values (e.g. M (77)).");
	RenderBullet(
		"Place the primary marker at the start of the 'M' event (left-click near "
		"the edge); place the secondary marker at the end of the 'M' event "
		"(shift-left-click).");
	RenderParagraph(
		"The dist field in the top bar shows the time between markers. In this "
		"case printing 'M' took 0.00043 seconds; switching units to Z80 cycles shows "
		"1553 cycles; CHPUT is relatively slow.");
	RenderParagraph(
		"At 9.92 seconds printing 'o' took much longer (about 8458 cycles). The "
		"z80.acceptIRQ row shows an interrupt at 9.921 seconds; the ISR consumed "
		"roughly 6900 cycles, explaining the longer duration.");
	RenderParagraph(
		"This simple example demonstrates how visualizing and correlating traces can "
		"reveal timing and interaction details between subsystems.");

	helpSection = {};
}

} // namespace openmsx
