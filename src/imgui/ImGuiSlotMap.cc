#include "ImGuiSlotMap.hh"

#include "ImGuiCpp.hh"
#include "ImGuiUtils.hh"

#include "CartridgeSlotManager.hh"
#include "DummyDevice.hh"
#include "MSXCPUInterface.hh"
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXMultiMemDevice.hh"

#include "gl_vec.hh"
#include "strCat.hh"
#include "xrange.hh"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace openmsx {

static constexpr unsigned PAGE_SIZE = 0x4000;

void ImGuiSlotMap::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	buf.appendf("layout=%s\n", vertical ? "vertical" : "horizontal");
}

void ImGuiSlotMap::loadLine(std::string_view name, zstring_view value)
{
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name == "layout") {
		// anything else keeps the default
		if (value == "vertical") {
			vertical = true;
		} else if (value == "horizontal") {
			vertical = false;
		}
	}
}

// The letter of the external cartridge slot at (ps, ss), if any.
// 'ss' must already be normalized: 0 for a non-expanded primary slot.
[[nodiscard]] static std::optional<char> getCartridgeLetter(
	const CartridgeSlotManager& slotManager, int ps, int ss)
{
	for (auto slot : xrange(CartridgeSlotManager::MAX_SLOTS)) {
		if (!slotManager.slotExists(slot)) continue;
		auto [slotPs, slotSs] = slotManager.getPsSs(slot);
		if (slotSs == -1) slotSs = 0; // not expanded
		if ((slotPs == ps) && (slotSs == ss)) {
			return char('A' + slot);
		}
	}
	return {};
}

// The part of one 16kB page that is covered by a single device, as offsets
// relative to the start of that page.
struct Segment {
	unsigned begin;
	unsigned end;
	const MSXDevice* device;
};

// A device that covers a whole page is registered directly, a device that
// covers less than a page is wrapped in an MSXMultiMemDevice, which knows the
// exact address ranges. Result is sorted; gaps are left to the caller.
static void getSegments(std::vector<Segment>& result, MSXCPUInterface& cpuInterface,
                        const MSXDevice* dummyDevice, int ps, int ss, int page)
{
	result.clear();
	const auto* device = cpuInterface.getMSXDevice(ps, ss, page);
	if (!device || (device == dummyDevice)) return; // nothing in this page

	const auto* multi = dynamic_cast<const MSXMultiMemDevice*>(device);
	if (!multi) {
		result.emplace_back(0u, PAGE_SIZE, device);
		return;
	}
	auto pageBegin = PAGE_SIZE * unsigned(page);
	for (const auto& range : multi->getRanges()) {
		auto begin = std::max(range.base, pageBegin);
		auto end = std::min(range.base + range.size, pageBegin + PAGE_SIZE);
		if (begin < end) {
			result.emplace_back(begin - pageBegin, end - pageBegin, range.device);
		}
	}
	std::ranges::sort(result, {}, &Segment::begin);
}

// Mirror of ImGuiUtils' leftClip(): keep the start of the string and replace
// the part that doesn't fit with an ellipsis.
[[nodiscard]] static std::string rightClip(std::string_view s, float maxWidth)
{
	maxWidth -= ImGui::CalcTextSize("..."sv).x;
	if (maxWidth <= 0.0f) return "...";

	// The first length that is too wide, minus one, is the longest prefix that
	// still fits. Length 0 always fits, so that first length is never 0.
	auto lengths = std::views::iota(0uz, s.size() + 1);
	auto it = std::ranges::upper_bound(lengths, maxWidth, {},
		[&](size_t n) { return ImGui::CalcTextSize(s.substr(0, n)).x; });
	auto num = (it == lengths.end()) ? s.size() : (*it - 1);
	return strCat(s.substr(0, num), "...");
}

// Try to draw 'text' as a block of centered lines, wrapped at word boundaries,
// in the rectangle [min, max). Returns false when it doesn't fit; a tall cell
// (a device covering several pages) can hold a lot more than a single line.
[[nodiscard]] static bool drawWrapped(std::string_view text, gl::vec2 min, gl::vec2 max)
{
	const auto& style = ImGui::GetStyle();
	auto width = (max.x - min.x) - 2.0f * style.CellPadding.x;
	auto height = (max.y - min.y) - 2.0f * style.CellPadding.y;
	auto lineHeight = ImGui::GetTextLineHeight();
	if ((width <= 0.0f) || (height < lineHeight)) return false;

	auto* font = ImGui::GetFont();
	auto fontSize = ImGui::GetFontSize();
	std::array<std::string_view, 8> lines; // a cell is at most 8 lines high
	size_t numLines = 0;

	const char* pos = text.data();
	const char* end = pos + text.size();
	while (pos < end) {
		if (numLines == lines.size()) return false;
		const char* stop = font->CalcWordWrapPosition(fontSize, pos, end, width);
		// ImGui breaks mid-word when a word is wider than the cell; that reads
		// as garbage in a diagram, so rather fall back to a clipped single line.
		if ((stop == pos) || ((stop != end) && (*stop != ' '))) return false;
		lines[numLines++] = std::string_view(pos, size_t(stop - pos));
		pos = stop;
		while ((pos < end) && (*pos == ' ')) ++pos; // eat the wrap point
	}
	if ((numLines == 0) || (float(numLines) * lineHeight > height)) return false;

	auto y = min.y + 0.5f * ((max.y - min.y) - float(numLines) * lineHeight);
	for (auto line : std::span(lines).first(numLines)) {
		auto lineWidth = ImGui::CalcTextSize(line).x;
		ImGui::SetCursorScreenPos(gl::vec2(min.x + 0.5f * ((max.x - min.x) - lineWidth), y));
		ImGui::TextUnformatted(line);
		y += lineHeight;
	}
	return true;
}

// Make a whole cell hoverable, so a short label can still explain itself.
static void cellToolTip(const char* id, gl::vec2 min, gl::vec2 max, std::string_view text)
{
	ImGui::SetCursorScreenPos(min);
	ImGui::InvisibleButton(id, max - min);
	simpleToolTip(text);
}

static void drawCentered(std::string_view text, gl::vec2 min, gl::vec2 max)
{
	if (drawWrapped(text, min, max)) return;

	// Doesn't fit, not even wrapped: clip to one line, full text on hover.
	const auto& style = ImGui::GetStyle();
	auto avail = (max.x - min.x) - 2.0f * style.CellPadding.x;
	auto y = min.y + 0.5f * ((max.y - min.y) - ImGui::GetTextLineHeight());
	ImGui::SetCursorScreenPos(gl::vec2(min.x + style.CellPadding.x, y));
	ImGui::TextUnformatted(rightClip(text, avail));
	simpleToolTip(text);
}

// Everything the grid needs. Cells are filled and outlined with the draw list
// rather than with a table, because a device can cover several pages (one
// merged cell) or just a part of one page.
namespace {
struct DrawContext {
	MSXCPUInterface& cpuInterface;
	const CartridgeSlotManager& slotManager;
	const MSXDevice* dummyDevice;
	ImDrawList* drawList;
	ImU32 emptyColor;
	ImU32 occupiedColor;
	ImU32 externalColor;
	ImU32 outlineColor;
	float rowHeight; // one line of text plus padding
	std::vector<std::pair<gl::vec2, gl::vec2>> outlines;
	std::vector<Segment> segments; // reused for every cell
};
}

static void drawCell(DrawContext& ctx, gl::vec2 min, gl::vec2 max,
                     const MSXDevice* device, bool cartridge)
{
	ctx.drawList->AddRectFilled(min, max,
		!device ? ctx.emptyColor : (cartridge ? ctx.externalColor : ctx.occupiedColor));
	ctx.outlines.emplace_back(min, max);
	drawCentered(device ? std::string_view(device->getName()) : "empty"sv, min, max);
}

// Outlines are drawn in one go, after all the fills, so that the fill of one
// cell can't paint over the outline of the cell next to it.
static void flushOutlines(DrawContext& ctx)
{
	for (const auto& [min, max] : ctx.outlines) {
		ctx.drawList->AddRect(min, max, ctx.outlineColor);
	}
	ctx.outlines.clear();
}

// One page that holds more than one device. Addresses run to the right in the
// horizontal layout and upwards in the vertical one, so that's also how the
// cell is split.
static void drawSplitPage(DrawContext& ctx, int ps, int ss, int page, bool cartridge,
                          gl::vec2 min, gl::vec2 max, bool vertical)
{
	auto drawSegment = [&](unsigned begin, unsigned end, const MSXDevice* device) {
		gl::vec2 segMin = min;
		gl::vec2 segMax = max;
		if (vertical) {
			auto scale = (max.y - min.y) * (1.0f / float(PAGE_SIZE));
			segMin.y = max.y - scale * float(end);
			segMax.y = max.y - scale * float(begin);
		} else {
			auto scale = (max.x - min.x) * (1.0f / float(PAGE_SIZE));
			segMin.x = min.x + scale * float(begin);
			segMax.x = min.x + scale * float(end);
		}
		drawCell(ctx, segMin, segMax, device, cartridge);
	};

	getSegments(ctx.segments, ctx.cpuInterface, ctx.dummyDevice, ps, ss, page);
	unsigned pos = 0;
	for (const auto& segment : ctx.segments) {
		if (pos < segment.begin) drawSegment(pos, segment.begin, nullptr);
		drawSegment(segment.begin, segment.end, segment.device);
		pos = segment.end;
	}
	if (pos < PAGE_SIZE) drawSegment(pos, PAGE_SIZE, nullptr);
}

// The grid, in either orientation. Horizontal has slots as rows and pages as
// columns; vertical is the transpose, with the highest address on top, like
// the memory maps in the MSX wiki and in most manuals.
//
// Both are laid out by hand instead of with a table, because a device covering
// several pages is drawn as one merged cell.
static void drawGrid(DrawContext& ctx, bool vertical)
{
	const auto& style = ImGui::GetStyle();
	auto rowHeight = ctx.rowHeight;
	// Gap between the 4 primary slots, along the slot axis.
	auto gap = style.ItemSpacing.x;
	// Widest label of the strip on the left.
	auto labelWidth = ImGui::CalcTextSize(vertical ? "Page"sv : "0-0 (cart A)"sv).x
	                + 2.0f * style.CellPadding.x;
	// One row with the title of each strip, one with the numbers.
	auto headerHeight = 2.0f * rowHeight;

	auto origin = gl::vec2(ImGui::GetCursorScreenPos());
	auto avail = ImGui::GetContentRegionAvail().x;

	// A non-expanded slot gets the same space as 4 sub-slots, so that the
	// layout is the same for every machine.
	float slotExtent = 4.0f * rowHeight; // one primary slot, along the slot axis
	float pageExtent = 0.0f;             // one page, along the page axis
	if (vertical) {
		slotExtent = std::max(slotExtent, 0.25f * (avail - labelWidth - 3.0f * gap));
		pageExtent = 2.0f * rowHeight; // room to split a page and still fit text
	} else {
		pageExtent = std::max(6.0f * rowHeight, 0.25f * (avail - labelWidth));
	}
	gl::vec2 size = vertical
		? gl::vec2(labelWidth + 4.0f * slotExtent + 3.0f * gap, headerHeight + 4.0f * pageExtent)
		: gl::vec2(labelWidth + 4.0f * pageExtent, headerHeight + 4.0f * slotExtent + 3.0f * gap);
	ImGui::Dummy(size); // reserve the space, the grid itself is positioned absolutely

	auto gridLeft = origin.x + labelWidth;
	auto gridTop = origin.y + headerHeight;

	// Position along the page axis of the strip covering pages 'lo'..'hi'.
	auto pagePos = [&](int page) {
		return vertical ? gridTop + float(3 - page) * pageExtent
		                : gridLeft + float(page) * pageExtent;
	};
	auto pageSpan = [&](int lo, int hi) {
		return vertical ? std::pair{pagePos(hi), pagePos(lo) + pageExtent}
		                : std::pair{pagePos(lo), pagePos(hi) + pageExtent};
	};
	auto makeRect = [&](float slotPos, float slotSize, std::pair<float, float> page) {
		return vertical
			? std::pair{gl::vec2(slotPos, page.first), gl::vec2(slotPos + slotSize, page.second)}
			: std::pair{gl::vec2(page.first, slotPos), gl::vec2(page.second, slotPos + slotSize)};
	};

	// Title of the strip along the top, and the one for the strip on the left.
	drawCentered(vertical ? "Slot"sv : "Page"sv,
	             gl::vec2(gridLeft, origin.y),
	             gl::vec2(origin.x + size.x, origin.y + rowHeight));
	drawCentered(vertical ? "Page"sv : "Slot"sv,
	             gl::vec2(origin.x, origin.y + rowHeight),
	             gl::vec2(gridLeft, gridTop));

	// Page numbers, on the left when vertical, on top when horizontal. The
	// address range is a tooltip in both.
	for (auto page : xrange(4)) {
		auto [min, max] = vertical
			? std::pair{gl::vec2(origin.x, pagePos(page)),
			            gl::vec2(gridLeft, pagePos(page) + pageExtent)}
			: std::pair{gl::vec2(pagePos(page), origin.y + rowHeight),
			            gl::vec2(pagePos(page) + pageExtent, gridTop)};
		drawCentered(strCat(page), min, max);
		auto begin = PAGE_SIZE * unsigned(page);
		cellToolTip(tmpStrCat("##page", page).c_str(), min, max,
		            tmpStrCat(hex_string<4, HexCase::upper>(begin), '-',
		                      hex_string<4, HexCase::upper>(begin + PAGE_SIZE - 1)));
	}

	for (auto ps : xrange(4)) {
		bool expanded = ctx.cpuInterface.isExpanded(ps);
		int numSlots = expanded ? 4 : 1;
		auto slotSize = slotExtent / float(numSlots);
		auto blockPos = (vertical ? gridLeft : gridTop) + float(ps) * (slotExtent + gap);

		for (auto sub : xrange(numSlots)) {
			int ss = expanded ? sub : 0;
			auto slotPos = blockPos + float(sub) * slotSize;
			auto cartridge = getCartridgeLetter(ctx.slotManager, ps, ss);

			auto label = expanded ? strCat(ps, '-', ss) : strCat(ps);
			if (cartridge) strAppend(label, " (cart ", *cartridge, ')');
			auto [labelMin, labelMax] = vertical
				? std::pair{gl::vec2(slotPos, origin.y + rowHeight),
				            gl::vec2(slotPos + slotSize, gridTop)}
				: std::pair{gl::vec2(origin.x, slotPos),
				            gl::vec2(gridLeft, slotPos + slotSize)};
			drawCentered(label, labelMin, labelMax);

			// Pages covered by a single device can be merged with the next
			// one; pages holding more than one device are drawn on their own.
			std::array<const MSXDevice*, 4> device = {};
			std::array<bool, 4> split = {};
			for (auto page : xrange(4)) {
				getSegments(ctx.segments, ctx.cpuInterface, ctx.dummyDevice, ps, ss, page);
				if (ctx.segments.empty()) {
					device[page] = nullptr; // whole page empty
				} else if ((ctx.segments.size() == 1) &&
				           (ctx.segments[0].begin == 0) &&
				           (ctx.segments[0].end == PAGE_SIZE)) {
					device[page] = ctx.segments[0].device;
				} else {
					split[page] = true;
				}
			}
			int page = 0;
			while (page < 4) {
				if (split[page]) {
					auto [min, max] = makeRect(slotPos, slotSize, pageSpan(page, page));
					drawSplitPage(ctx, ps, ss, page, bool(cartridge), min, max, vertical);
					++page;
					continue;
				}
				int last = page;
				while ((last + 1 < 4) && !split[last + 1] &&
				       (device[last + 1] == device[page])) {
					++last;
				}
				auto [min, max] = makeRect(slotPos, slotSize, pageSpan(page, last));
				drawCell(ctx, min, max, device[page], bool(cartridge));
				page = last + 1;
			}
		}
	}
	flushOutlines(ctx);
	ImGui::SetCursorScreenPos(origin + gl::vec2(0.0f, size.y + style.ItemSpacing.y));
}

static void drawLegend(const char* text, ImU32 color)
{
	auto size = ImGui::GetFontSize();
	ImGui::ColorButton(text, ImGui::ColorConvertU32ToFloat4(color),
	                   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
	                   {size, size});
	ImGui::SameLine();
	ImGui::TextDisabledUnformatted(text);
}

void ImGuiSlotMap::paint(MSXMotherBoard* motherBoard)
{
	if (!show) return;
	if (!motherBoard) return;

	auto& cpuInterface = motherBoard->getCPUInterface();
	const auto& style = ImGui::GetStyle();
	DrawContext ctx{
		.cpuInterface = cpuInterface,
		.slotManager = motherBoard->getSlotManager(),
		.dummyDevice = &cpuInterface.getDummyDevice(),
		.drawList = nullptr, // only valid inside the window
		.emptyColor = ImGui::GetColorU32(getColor(imColor::GRAY), 0.4f),
		.occupiedColor = ImGui::GetColorU32(ImGuiCol_Header),
		.externalColor = getColor(imColor::YELLOW_BG),
		.outlineColor = ImGui::GetColorU32(ImGuiCol_Text),
		.rowHeight = ImGui::GetTextLineHeight() + 2.0f * style.CellPadding.y,
	};
	ctx.outlines.reserve(16);

	auto fontSize = ImGui::GetFontSize();
	if (fitToContent) {
		// The two layouts have a very different shape, so after switching,
		// resize the window to whatever the new one needs (0 == auto-fit).
		fitToContent = false;
		ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
	} else {
		// Only the width is elastic, the height follows from the layout, so
		// let it auto-fit rather than guessing a value that's wrong for one
		// of the two layouts.
		ImGui::SetNextWindowSize(ImVec2(64.0f * fontSize, 0.0f), ImGuiCond_FirstUseEver);
	}
	im::Window("Slot map", &show, [&]{
		ctx.drawList = ImGui::GetWindowDrawList();
		drawGrid(ctx, vertical);

		int index = vertical ? 0 : 1;
		ImGui::SetNextItemWidth(ImGui::CalcTextSize("Horizontal"sv).x +
		                        ImGui::GetFrameHeight() + 2.0f * style.FramePadding.x);
		if (ImGui::Combo("Layout", &index, "Vertical\0Horizontal\0")) {
			vertical = (index == 0);
			fitToContent = true;
		}
		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();
		drawLegend("empty", ctx.emptyColor);
		ImGui::SameLine();
		drawLegend("device", ctx.occupiedColor);
		ImGui::SameLine();
		drawLegend("external cartridge", ctx.externalColor);
	});
}

} // namespace openmsx
