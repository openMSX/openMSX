#include "ImGuiSlotMap.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "CartridgeSlotManager.hh"
#include "DummyDevice.hh"
#include "MSXCPUInterface.hh"
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXMultiMemDevice.hh"
#include "TclObject.hh"

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
static constexpr unsigned ADDRESS_SPACE = 4 * PAGE_SIZE;

void ImGuiSlotMap::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiSlotMap::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
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

// A continuous address range in one slot, covered by a single device.
// 'device' is nullptr for the parts where nothing is mapped.
struct Block {
	unsigned begin;
	unsigned end;
	const MSXDevice* device;
};

// Split the address space of one slot into blocks. MSXCPUInterface stores a
// device per 16kB page, and wraps devices that cover less than a page in an
// MSXMultiMemDevice, which knows the exact address ranges. Recombining both
// gives the real extent of every device, so that nothing here has to care
// about page boundaries.
static void getBlocks(std::vector<Block>& result, std::vector<Block>& scratch,
                      MSXCPUInterface& cpuInterface, const MSXDevice* dummyDevice,
                      int ps, int ss)
{
	scratch.clear();
	for (auto page : xrange(4)) {
		auto pageBegin = PAGE_SIZE * unsigned(page);
		const auto* device = cpuInterface.getMSXDevice(ps, ss, page);
		if (!device || (device == dummyDevice)) continue;
		if (const auto* multi = dynamic_cast<const MSXMultiMemDevice*>(device)) {
			for (const auto& range : multi->getRanges()) {
				auto begin = std::max(range.base, pageBegin);
				auto end = std::min(range.base + range.size, pageBegin + PAGE_SIZE);
				if (begin < end) scratch.emplace_back(begin, end, range.device);
			}
		} else {
			scratch.emplace_back(pageBegin, pageBegin + PAGE_SIZE, device);
		}
	}
	std::ranges::sort(scratch, {}, &Block::begin);

	// Glue the pieces of a device that spans page boundaries back together,
	// and fill what's left with 'empty' blocks.
	result.clear();
	unsigned pos = 0;
	for (const auto& block : scratch) {
		if (!result.empty() && (result.back().device == block.device) &&
		    (result.back().end == block.begin)) {
			result.back().end = block.end;
		} else {
			if (pos < block.begin) result.emplace_back(pos, block.begin, nullptr);
			result.emplace_back(block.begin, block.end, block.device);
		}
		pos = block.end;
	}
	if (pos < ADDRESS_SPACE) result.emplace_back(pos, ADDRESS_SPACE, nullptr);
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

// Draw 'text' centered in the rectangle, wrapped over as many lines as fit.
// Falls back to a single clipped line, and to nothing at all when even that
// doesn't fit - the tooltip is what makes those cells readable.
static void drawText(ImDrawList* drawList, std::string_view text,
                     gl::vec2 min, gl::vec2 max, ImU32 color)
{
	const auto& style = ImGui::GetStyle();
	auto width = (max.x - min.x) - 2.0f * style.CellPadding.x;
	auto height = (max.y - min.y) - 2.0f * style.CellPadding.y;
	auto lineHeight = ImGui::GetTextLineHeight();
	if ((height < lineHeight) || (width < ImGui::CalcTextSize("..."sv).x)) return;

	auto draw = [&](std::string_view line, float y) {
		auto lineWidth = ImGui::CalcTextSize(line).x;
		drawList->AddText(gl::vec2(min.x + 0.5f * ((max.x - min.x) - lineWidth), y),
		                  color, line.data(), line.data() + line.size());
	};

	// Wrap at word boundaries. ImGui breaks mid-word when a word is wider than
	// the cell, which reads as garbage in a diagram, so treat that as 'doesn't
	// fit' and fall back to clipping.
	auto* font = ImGui::GetFont();
	auto fontSize = ImGui::GetFontSize();
	std::array<std::string_view, 16> lines;
	size_t numLines = 0;
	bool wrapped = true;
	const char* pos = text.data();
	const char* end = pos + text.size();
	while (pos < end) {
		if (numLines == lines.size()) { wrapped = false; break; }
		const char* stop = font->CalcWordWrapPosition(fontSize, pos, end, width);
		if ((stop == pos) || ((stop != end) && (*stop != ' '))) { wrapped = false; break; }
		lines[numLines++] = std::string_view(pos, size_t(stop - pos));
		pos = stop;
		while ((pos < end) && (*pos == ' ')) ++pos; // eat the wrap point
	}
	if (wrapped && (numLines != 0) && (float(numLines) * lineHeight <= height)) {
		auto y = min.y + 0.5f * ((max.y - min.y) - float(numLines) * lineHeight);
		for (auto line : std::span(lines).first(numLines)) {
			draw(line, y);
			y += lineHeight;
		}
	} else {
		draw(rightClip(text, width), min.y + 0.5f * ((max.y - min.y) - lineHeight));
	}
}

namespace {
// What the mouse is over, so that only one tooltip is shown even where blocks
// overlap after being enlarged to a hoverable size.
struct HoveredBlock {
	Block block{0, 0, nullptr};
	float height = 0.0f;
	bool valid = false;
};

// A block that is too small to see, enlarged and drawn on top of its
// neighbours instead of to scale.
struct TinyBlock {
	gl::vec2 min;
	gl::vec2 max;
	ImU32 color;
};

struct DrawContext {
	ImGuiManager& manager;
	MSXCPUInterface& cpuInterface;
	const CartridgeSlotManager& slotManager;
	const MSXDevice* dummyDevice;
	ImDrawList* drawList;
	ImU32 emptyColor;
	ImU32 occupiedColor;
	ImU32 externalColor;
	ImU32 outlineColor;
	ImU32 textColor;
	ImU32 dimTextColor;
	gl::vec2 mouse;
	HoveredBlock hovered;
	std::vector<std::pair<gl::vec2, gl::vec2>> outlines;
	std::vector<TinyBlock> tinyBlocks;
	std::vector<Block> blocks;
	std::vector<Block> scratch;
};
}

// Below this a block can't be seen, let alone hovered. A single byte is a
// hundredth of a pixel on a 64kB axis, so those are drawn out of scale.
static constexpr float MIN_BLOCK_HEIGHT = 4.0f;

// One slot, drawn as a continuous 0x0000-0xFFFF axis with the highest address
// on top, so a device is a rectangle at its real position and size.
static void drawSlot(DrawContext& ctx, int ps, int ss, bool expanded,
                     float x0, float x1, float headerTop, float top, float bottom)
{
	auto cartridge = getCartridgeLetter(ctx.slotManager, ps, ss);
	auto labelHeight = 0.5f * (top - headerTop);
	drawText(ctx.drawList, expanded ? strCat(ps, '-', ss) : strCat(ps),
	         gl::vec2(x0, headerTop), gl::vec2(x1, headerTop + labelHeight), ctx.textColor);
	if (cartridge) {
		drawText(ctx.drawList, tmpStrCat("Slot ", *cartridge),
		         gl::vec2(x0, headerTop + labelHeight), gl::vec2(x1, top), ctx.dimTextColor);
	}

	auto height = bottom - top;
	auto addressToY = [&](unsigned address) {
		return bottom - height * (float(address) * (1.0f / float(ADDRESS_SPACE)));
	};

	getBlocks(ctx.blocks, ctx.scratch, ctx.cpuInterface, ctx.dummyDevice, ps, ss);
	// Blocks come in increasing address order, so downwards on screen. Keeps
	// enlarged blocks from covering each other.
	auto lowestEnlarged = bottom;
	for (const auto& block : ctx.blocks) {
		auto blockTop = addressToY(block.end);
		auto blockBottom = addressToY(block.begin);
		auto blockHeight = blockBottom - blockTop;
		auto color = !block.device ? ctx.emptyColor
		                           : (cartridge ? ctx.externalColor : ctx.occupiedColor);

		if (auto grow = 0.5f * (MIN_BLOCK_HEIGHT - blockHeight); grow > 0.0f) {
			// Too small to draw to scale, so draw it out of scale: enlarge it
			// around its real address, stack it on top of an earlier enlarged
			// block if they'd overlap, and paint it over its big neighbours
			// later on.
			blockTop -= grow;
			blockBottom += grow;
			if (blockBottom > lowestEnlarged) {
				blockTop -= blockBottom - lowestEnlarged;
				blockBottom = lowestEnlarged;
			}
			lowestEnlarged = blockTop;
			ctx.tinyBlocks.emplace_back(gl::vec2(x0, blockTop), gl::vec2(x1, blockBottom),
			                            color);
		} else {
			gl::vec2 min{x0, blockTop};
			gl::vec2 max{x1, blockBottom};
			ctx.drawList->AddRectFilled(min, max, color);
			ctx.outlines.emplace_back(min, max);
			drawText(ctx.drawList, block.device ? std::string_view(block.device->getName())
			                                    : "empty"sv,
			         min, max, ctx.textColor);
		}

		// Hit-test what is actually drawn. The smallest block wins, so an
		// enlarged one stays reachable between its much larger neighbours.
		if ((x0 <= ctx.mouse.x) && (ctx.mouse.x < x1) &&
		    (blockTop <= ctx.mouse.y) && (ctx.mouse.y < blockBottom) &&
		    (!ctx.hovered.valid || (blockHeight < ctx.hovered.height))) {
			ctx.hovered = HoveredBlock{block, blockHeight, true};
		}
	}
}

static void drawDeviceToolTip(DrawContext& ctx)
{
	const auto& [block, height_, valid_] = ctx.hovered;
	im::Tooltip([&]{
		im::TextWrapPos(ImGui::GetFontSize() * 35.0f, [&]{
			if (block.device) {
				ImGui::TextUnformatted(block.device->getName());
				ImGui::Separator();
			}
			auto size = block.end - block.begin;
			ImGui::StrCat("address: 0x", hex_string<4, HexCase::upper>(block.begin),
			              " - 0x", hex_string<4, HexCase::upper>(block.end - 1));
			ImGui::StrCat("size: ", (size < 1024) ? strCat(size, " bytes")
			                                      : strCat(size / 1024, "kB"));
			if (!block.device) return;

			const auto& name = block.device->getName();
			// Whatever the device wants to tell about itself, e.g. the mapper
			// type of a ROM or the file it was loaded from.
			if (auto info = ctx.manager.execute(makeTclList("machine_info", "device", name))) {
				for (size_t i = 0; (i + 1) < info->size(); i += 2) {
					auto key = info->getListIndexUnchecked(i).getString();
					auto value = info->getListIndexUnchecked(i + 1).getString();
					if (value.empty()) continue;
					ImGui::StrCat(key, ": ", value);
				}
			}
			// Devices with a debuggable of the same name (memory mappers, RAM)
			// can also tell how much memory they actually have.
			if (auto total = ctx.manager.execute(makeTclList("debug", "size", name))) {
				if (auto bytes = total->getOptionalInt(); bytes && (*bytes > 0)) {
					ImGui::StrCat("total memory: ", *bytes / 1024, "kB");
				}
			}
		});
	});
}

static void drawSlotMap(DrawContext& ctx)
{
	const auto& style = ImGui::GetStyle();
	auto lineHeight = ImGui::GetTextLineHeight();
	auto rowHeight = lineHeight + 2.0f * style.CellPadding.y;
	auto headerHeight = 2.0f * rowHeight; // slot number, external slot name
	auto gapX = 2.0f * style.ItemSpacing.x;
	auto gapY = style.ItemSpacing.y;
	// Wide enough for every label on the address axis. Letters and digits don't
	// have the same width, so check both kinds.
	auto labelWidth = 2.0f * style.CellPadding.x +
		std::max({ImGui::CalcTextSize("0000"sv).x,
		          ImGui::CalcTextSize("C000"sv).x,
		          ImGui::CalcTextSize("FFFF"sv).x});
	// A primary slot always gets the same width, expanded or not, so machines
	// with a different slot layout still look alike.
	auto minSlotWidth = 4.0f * 3.0f * lineHeight;
	auto minMapHeight = 10.0f * rowHeight;

	auto avail = gl::vec2(ImGui::GetContentRegionAvail());
	// The labels of the lowest and highest address stick out half a line.
	auto overhang = 0.5f * rowHeight;

	// Reflow: put 4, 2 or 1 primary slots next to each other, whichever fits.
	auto widthFor = [&](int perRow) {
		return labelWidth + float(perRow) * minSlotWidth + float(perRow - 1) * gapX;
	};
	int perRow = 4;
	if (widthFor(perRow) > avail.x) perRow = 2;
	if ((perRow == 2) && (widthFor(perRow) > avail.x)) perRow = 1;
	int numRows = 4 / perRow;

	auto slotWidth = std::max(minSlotWidth,
		(avail.x - labelWidth - float(perRow - 1) * gapX) / float(perRow));
	auto mapHeight = std::max(minMapHeight,
		(avail.y - overhang - float(numRows) * headerHeight - float(numRows - 1) * gapY) /
		float(numRows));

	auto origin = gl::vec2(ImGui::GetCursorScreenPos());
	gl::vec2 size{labelWidth + float(perRow) * slotWidth + float(perRow - 1) * gapX,
	              float(numRows) * (headerHeight + mapHeight) + float(numRows - 1) * gapY +
	              overhang};
	ImGui::Dummy(size); // reserve the space, the map itself is positioned absolutely

	for (auto row : xrange(numRows)) {
		auto headerTop = origin.y + float(row) * (headerHeight + mapHeight + gapY);
		auto top = headerTop + headerHeight;
		auto bottom = top + mapHeight;
		auto addressToY = [&](unsigned address) {
			return bottom - mapHeight * (float(address) * (1.0f / float(ADDRESS_SPACE)));
		};

		// Address axis: one label per page boundary, plus the very top, each
		// centered on the position it marks.
		auto drawTick = [&](unsigned address, std::string_view label) {
			auto y = addressToY(address);
			drawText(ctx.drawList, label,
			         gl::vec2(origin.x, y - 0.5f * rowHeight),
			         gl::vec2(origin.x + labelWidth, y + 0.5f * rowHeight),
			         ctx.textColor);
		};
		for (auto page : xrange(4)) {
			auto address = PAGE_SIZE * unsigned(page);
			drawTick(address, tmpStrCat(hex_string<4, HexCase::upper>(address)));
		}
		drawTick(ADDRESS_SPACE, "FFFF"sv);

		auto x = origin.x + labelWidth;
		for (auto ps : xrange(row * perRow, std::min((row + 1) * perRow, 4))) {
			if (ctx.cpuInterface.isExpanded(ps)) {
				auto width = 0.25f * slotWidth;
				for (auto ss : xrange(4)) {
					auto x0 = x + float(ss) * width;
					drawSlot(ctx, ps, ss, true, x0, x0 + width, headerTop, top, bottom);
				}
			} else {
				drawSlot(ctx, ps, 0, false, x, x + slotWidth, headerTop, top, bottom);
			}
			x += slotWidth + gapX;
		}
	}

	// Outlines in one go, after all the fills, so that the fill of one block
	// can't paint over the outline of the block next to it. This is also what
	// keeps a device that is too small to see as a rectangle visible as a line.
	for (const auto& [min, max] : ctx.outlines) {
		ctx.drawList->AddRect(min, max, ctx.outlineColor);
	}
	ctx.outlines.clear();

	// Devices too small to draw to scale go last, so that they end up visible
	// on top of the (much bigger) devices around them.
	for (const auto& [min, max, color] : ctx.tinyBlocks) {
		ctx.drawList->AddRectFilled(min, max, color);
		ctx.drawList->AddRect(min, max, ctx.outlineColor);
	}
	ctx.tinyBlocks.clear();
}

void ImGuiSlotMap::paint(MSXMotherBoard* motherBoard)
{
	if (!show) return;
	if (!motherBoard) return;

	auto& cpuInterface = motherBoard->getCPUInterface();
	DrawContext ctx{
		.manager = manager,
		.cpuInterface = cpuInterface,
		.slotManager = motherBoard->getSlotManager(),
		.dummyDevice = &cpuInterface.getDummyDevice(),
		.drawList = nullptr, // only valid inside the window
		.emptyColor = ImGui::GetColorU32(getColor(imColor::GRAY), 0.4f),
		.occupiedColor = ImGui::GetColorU32(ImGuiCol_Header),
		.externalColor = getColor(imColor::YELLOW_BG),
		.outlineColor = ImGui::GetColorU32(ImGuiCol_Text),
		.textColor = ImGui::GetColorU32(ImGuiCol_Text),
		.dimTextColor = ImGui::GetColorU32(ImGuiCol_TextDisabled),
		.mouse = gl::vec2(ImGui::GetIO().MousePos),
	};
	ctx.outlines.reserve(32);

	auto fontSize = ImGui::GetFontSize();
	ImGui::SetNextWindowSize(ImVec2(64.0f * fontSize, 26.0f * fontSize), ImGuiCond_FirstUseEver);
	im::Window("Slot map", &show, [&]{
		ctx.drawList = ImGui::GetWindowDrawList();
		drawSlotMap(ctx);

		if (ctx.hovered.valid && ImGui::IsWindowHovered()) {
			drawDeviceToolTip(ctx);
		}
	});
}

} // namespace openmsx
