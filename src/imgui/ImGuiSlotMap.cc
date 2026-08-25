#include "ImGuiSlotMap.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiMedia.hh"
#include "ImGuiUtils.hh"

#include "CartridgeSlotManager.hh"
#include "Debuggable.hh"
#include "Debugger.hh"
#include "DummyDevice.hh"
#include "MSXCPUInterface.hh"
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXMultiMemDevice.hh"
#include "TclObject.hh"

#include "gl_vec.hh"
#include "static_vector.hh"
#include "strCat.hh"
#include "xrange.hh"

#include <imgui.h>

#include <algorithm>
#include <optional>
#include <ranges>
#include <string_view>
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

// A continuous address range in one slot, covered by a single device.
// 'device' is nullptr for the parts where nothing is mapped.
struct Block {
	unsigned begin;
	unsigned end; // exclusive
	const MSXDevice* device;
};

// Draw 'text' centered in the rectangle, wrapped over as many lines as fit.
// The last line ends in an ellipsis if the whole text doesn't fit, and nothing
// is drawn at all when not even an ellipsis fits - the tooltip is what makes
// those cells readable.
static void drawText(ImDrawList* drawList, std::string_view text,
                     gl::vec2 min, gl::vec2 max, ImU32 color)
{
	const auto& style = ImGui::GetStyle();
	auto [width, height] = (max - min) - 2.0f * gl::vec2(style.CellPadding);
	auto lineHeight = ImGui::GetTextLineHeight();
	if ((height < lineHeight) || (width < ImGui::CalcTextSize("..."sv).x)) return;

	auto draw = [&](std::string_view line, float y) {
		auto lineWidth = ImGui::CalcTextSize(line).x;
		drawList->AddText(gl::vec2(min.x + 0.5f * ((max.x - min.x) - lineWidth), y),
		                  color, line.data(), line.data() + line.size());
	};

	// Wrap at word boundaries for as long as that works, then put what is left
	// on the last line, clipped. Wrapping stops for one of two reasons: there
	// is no room for another line, or the next word doesn't fit on a line of
	// its own. In the latter case CalcWordWrapPosition() would break inside the
	// word, which reads as garbage in a diagram. Both cases end the same way,
	// so a name is either fully wrapped or ends in an ellipsis.
	auto* font = ImGui::GetFont();
	auto fontSize = ImGui::GetFontSize();
	static_vector<std::string_view, 16> lines;
	auto maxLines = std::min(size_t(height / lineHeight), lines.max_size());
	std::string_view rest;
	const char* pos = text.data();
	const char* end = pos + text.size();
	while (pos < end) {
		auto remainder = std::string_view(pos, size_t(end - pos));
		if ((lines.size() + 1) == maxLines) { rest = remainder; break; }
		const char* stop = font->CalcWordWrapPosition(fontSize, pos, end, width);
		if ((stop == pos) ||                     // not even one word fits
		    ((stop != end) && (*stop != ' '))) { // would break inside a word
			rest = remainder;
			break;
		}
		lines.push_back(std::string_view(pos, size_t(stop - pos)));
		pos = stop;
		while ((pos < end) && (*pos == ' ')) ++pos; // eat the wrap point
	}

	auto numLines = lines.size() + (rest.empty() ? 0 : 1);
	auto y = min.y + 0.5f * ((max.y - min.y) - float(numLines) * lineHeight);
	for (auto line : lines) {
		draw(line, y);
		y += lineHeight;
	}
	// rightClip() adds the ellipsis, and returns 'rest' unchanged when it fits.
	if (!rest.empty()) draw(ImGui::rightClip(rest, width), y);
}

namespace {
// What the mouse is over, so that only one tooltip is shown even where blocks
// overlap after being enlarged to a hoverable size.
struct HoveredBlock {
	Block block{0, 0, nullptr};
	std::optional<unsigned> cartridgeSlot; // when it's in an external slot
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

// Everything that only depends on the font and the style, so the same for the
// whole frame.
struct Metrics {
	float rowHeight;
	float headerHeight;
	float labelWidth;
	float minSlotWidth;
	float minMapHeight;
	gl::vec2 gap;
	float overhang;
};

struct DrawContext {
	ImGuiManager& manager;
	MSXCPUInterface& cpuInterface;
	const CartridgeSlotManager& slotManager;
	Debugger& debugger;
	const MSXDevice* dummyDevice;
	Metrics metrics;
	ImDrawList* drawList;
	ImU32 emptyColor;
	ImU32 occupiedColor;
	ImU32 externalColor;
	ImU32 outlineColor;
	ImU32 textColor;
	ImU32 dimTextColor;
	gl::vec2 mouse;
	HoveredBlock hovered;
	std::vector<std::pair<gl::vec2, gl::vec2>> outlines;   // one per primary slot
	std::vector<std::pair<gl::vec2, gl::vec2>> separators; // between the blocks
	std::vector<TinyBlock> tinyBlocks;
	std::vector<Block> blocks;
	std::vector<Block> scratch;
};
}

// Split the address space of one slot into blocks, in 'ctx.blocks'.
// MSXCPUInterface stores a device per 16kB page, and wraps devices that cover
// less than a page in an MSXMultiMemDevice, which knows the exact address
// ranges. Recombining both gives the real extent of every device, so that
// nothing here has to care about page boundaries.
// Note: 'ctx.blocks' and 'ctx.scratch' live in the DrawContext instead of being
//       local variables, so that the (up to) 16 calls in one frame share a
//       single buffer.
static void getBlocks(DrawContext& ctx, int ps, int ss)
{
	auto& scratch = ctx.scratch;
	scratch.clear();
	for (auto page : xrange(4)) {
		auto pageBegin = PAGE_SIZE * unsigned(page);
		const auto* device = ctx.cpuInterface.getMSXDevice(ps, ss, page);
		if (device == ctx.dummyDevice) continue;
		if (const auto* multi = dynamic_cast<const MSXMultiMemDevice*>(device)) {
			// Only these sub-ranges can be out of order: the pages themselves
			// are visited in increasing order, so sorting per page is enough.
			auto first = scratch.size();
			for (const auto& range : multi->getRanges()) {
				auto begin = std::max(range.base, pageBegin);
				auto end = std::min(range.base + range.size, pageBegin + PAGE_SIZE);
				if (begin < end) scratch.emplace_back(begin, end, range.device);
			}
			std::ranges::sort(scratch.begin() + first, scratch.end(), {}, &Block::begin);
		} else {
			scratch.emplace_back(pageBegin, pageBegin + PAGE_SIZE, device);
		}
	}

	// Glue the pieces of a device that spans page boundaries back together,
	// and fill what's left with 'empty' blocks.
	auto& result = ctx.blocks;
	result.clear();
	unsigned pos = 0;
	for (const auto& block : scratch) {
		if (!result.empty() && (result.back().device == block.device) &&
		    (result.back().end == block.begin)) {
			result.back().end = block.end;
		} else {
			if (pos < block.begin) result.emplace_back(pos, block.begin, nullptr);
			result.push_back(block);
		}
		pos = block.end;
	}
	if (pos < ADDRESS_SPACE) result.emplace_back(pos, ADDRESS_SPACE, nullptr);
}

static Metrics getMetrics()
{
	const auto& style = ImGui::GetStyle();
	auto lineHeight = ImGui::GetTextLineHeight();
	auto rowHeight = lineHeight + 2.0f * style.CellPadding.y;
	return {
		.rowHeight = rowHeight,
		.headerHeight = 2.0f * rowHeight, // external slot name, slot number
		// Wide enough for every label on the address axis. Letters and digits
		// don't have the same width, so check both kinds.
		.labelWidth = 2.0f * style.CellPadding.x +
		              std::max({ImGui::CalcTextSize("0000"sv).x,
		                        ImGui::CalcTextSize("C000"sv).x,
		                        ImGui::CalcTextSize("FFFF"sv).x}),
		// A primary slot always gets the same width, expanded or not, so
		// machines with a different slot layout still look alike.
		.minSlotWidth = 4.0f * 3.0f * lineHeight,
		.minMapHeight = 10.0f * rowHeight,
		.gap = 2.0f * gl::vec2(style.ItemSpacing),
		// The labels of the lowest and highest address stick out half a line.
		.overhang = 0.5f * rowHeight,
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
	// The slot number goes directly above the map, there are four times as many
	// of those as external slot names.
	auto cartridge = ctx.slotManager.findSlot(ps, ss, true);
	auto labelHeight = 0.5f * (top - headerTop);
	if (cartridge) {
		drawText(ctx.drawList, tmpStrCat("Slot ", char('A' + *cartridge)),
		         gl::vec2(x0, headerTop), gl::vec2(x1, headerTop + labelHeight),
		         ctx.dimTextColor);
	}
	drawText(ctx.drawList, expanded ? strCat(ps, '-', ss) : strCat(ps),
	         gl::vec2(x0, headerTop + labelHeight), gl::vec2(x1, top), ctx.textColor);

	auto height = bottom - top;
	auto addressToY = [&](unsigned address) {
		return bottom - height * (float(address) * (1.0f / float(ADDRESS_SPACE)));
	};

	getBlocks(ctx, ps, ss);
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
			// Only the line towards the next block: the outer edges belong to
			// the outline of the slot as a whole. Drawing a rectangle per block
			// would draw every shared edge twice. See the note on 'separators'
			// for why this stops one pixel short of x1.
			if (block.end != ADDRESS_SPACE) {
				ctx.separators.emplace_back(min, gl::vec2(x1 - 1.0f, blockTop));
			}
			drawText(ctx.drawList, block.device ? std::string_view(block.device->getName())
			                                    : "empty"sv,
			         min, max, ctx.textColor);
		}

		// Hit-test what is actually drawn. The smallest block wins, so an
		// enlarged one stays reachable between its much larger neighbours.
		if ((x0 <= ctx.mouse.x) && (ctx.mouse.x < x1) &&
		    (blockTop <= ctx.mouse.y) && (ctx.mouse.y < blockBottom) &&
		    (!ctx.hovered.valid || (blockHeight < ctx.hovered.height))) {
			ctx.hovered = HoveredBlock{block, cartridge, blockHeight, true};
		}
	}
}

static void drawDeviceToolTip(DrawContext& ctx)
{
	const auto& block = ctx.hovered.block;
	im::Tooltip([&]{
		im::TextWrapPos(ImGui::GetFontSize() * 35.0f, [&]{
			bool header = false;
			// For an external slot, what is plugged into it. That's not the
			// same thing as the device below, which is a part of it, though
			// for a simple extension the two names are the same.
			if (auto slot = ctx.hovered.cartridgeSlot) {
				auto content = ctx.manager.media->displayNameForSlotContent(
					ctx.slotManager, *slot);
				if (block.device && (content == block.device->getName())) {
					ImGui::StrCat("Slot ", char('A' + *slot));
				} else {
					ImGui::StrCat("Slot ", char('A' + *slot), ": ", content);
				}
				header = true;
			}
			if (block.device) {
				ImGui::TextUnformatted(block.device->getName());
				header = true;
			}
			if (header) ImGui::Separator();
			// Below 1kB the size in kB would round down to 0.
			auto printSize = [](std::string_view label, unsigned bytes) {
				auto unit = (bytes == 1) ? " byte"sv : " bytes"sv;
				ImGui::StrCat(label, ": ", (bytes < 1024) ? strCat(bytes, unit)
				                                          : strCat(bytes / 1024, "kB"));
			};
			ImGui::StrCat("address: 0x", hex_string<4, HexCase::upper>(block.begin),
			              " - 0x", hex_string<4, HexCase::upper>(block.end - 1));
			printSize("size", block.end - block.begin);
			if (!block.device) return;

			// Whatever the device wants to tell about itself, e.g. the mapper
			// type of a ROM or the file it was loaded from. Ask the device
			// directly instead of going through the 'machine_info device'
			// command: we already have the MSXDevice here.
			TclObject info;
			block.device->getDeviceInfo(info);
			for (size_t i = 0; (i + 1) < info.size(); i += 2) {
				auto key = info.getListIndexUnchecked(i).getString();
				auto value = info.getListIndexUnchecked(i + 1).getString();
				if (value.empty()) continue;
				ImGui::StrCat(key, ": ", value);
			}
			// Devices with a debuggable of the same name (memory mappers, RAM)
			// can also tell how much memory they actually have.
			if (const auto* debuggable = ctx.debugger.findDebuggable(block.device->getName())) {
				if (auto bytes = debuggable->getSize(); bytes != 0) {
					printSize("total memory", bytes);
				}
			}
		});
	});
}

static void drawSlotMap(DrawContext& ctx)
{
	auto [rowHeight, headerHeight, labelWidth, minSlotWidth, minMapHeight, gap, overhang] =
		ctx.metrics;
	auto avail = gl::vec2(ImGui::GetContentRegionAvail());

	// Reflow: put 4, 2 or 1 primary slots next to each other, whichever fits.
	auto widthFor = [&](int perRow) {
		return labelWidth + float(perRow) * minSlotWidth + float(perRow - 1) * gap.x;
	};
	int perRow = 4;
	if (widthFor(perRow) > avail.x) perRow = 2;
	if ((perRow == 2) && (widthFor(perRow) > avail.x)) perRow = 1;
	int numRows = 4 / perRow;

	auto slotWidth = std::max(minSlotWidth,
		(avail.x - labelWidth - float(perRow - 1) * gap.x) / float(perRow));
	auto mapHeight = std::max(minMapHeight,
		(avail.y - overhang - float(numRows) * headerHeight - float(numRows - 1) * gap.y) /
		float(numRows));

	auto origin = gl::vec2(ImGui::GetCursorScreenPos());
	gl::vec2 size{labelWidth + float(perRow) * slotWidth + float(perRow - 1) * gap.x,
	              float(numRows) * (headerHeight + mapHeight) + float(numRows - 1) * gap.y +
	              overhang};
	ImGui::Dummy(size); // reserve the space, the map itself is positioned absolutely

	for (auto row : xrange(numRows)) {
		auto headerTop = origin.y + float(row) * (headerHeight + mapHeight + gap.y);
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
					// Between the sub-slots only, the outer edges are part
					// of the outline of the primary slot.
					if (ss != 0) {
						ctx.separators.emplace_back(gl::vec2(x0, top),
						                            gl::vec2(x0, bottom - 1.0f));
					}
				}
			} else {
				drawSlot(ctx, ps, 0, false, x, x + slotWidth, headerTop, top, bottom);
			}
			ctx.outlines.emplace_back(gl::vec2(x, top), gl::vec2(x + slotWidth, bottom));
			x += slotWidth + gap.x;
		}
	}

	// Outlines in one go, after all the fills, so that the fill of one block
	// can't paint over the outline of the block next to it. This is also what
	// keeps a device that is too small to see as a rectangle visible as a line.
	// Each edge is drawn exactly once, otherwise the shared ones would come out
	// twice as thick as the rest.
	// Note: AddRect() treats 'max' as exclusive (it strokes half a pixel inside
	//       it, like AddRectFilled() fills up to just before it), while both end
	//       points of AddLine() are inclusive. So a separator has to stop one
	//       pixel short to end where the outline around it is, see drawSlot().
	for (const auto& [p1, p2] : ctx.separators) {
		ctx.drawList->AddLine(p1, p2, ctx.outlineColor);
	}
	ctx.separators.clear();
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

// Big enough for a 2x2 layout: room for three primary slots next to each other,
// while the reflow needs room for four before it puts them in a single row.
static gl::vec2 defaultWindowSize(const Metrics& m)
{
	const auto& style = ImGui::GetStyle();
	gl::vec2 content{m.labelWidth + 3.0f * (m.minSlotWidth + m.gap.x),
	                 2.0f * (m.headerHeight + m.minMapHeight) + m.gap.y + m.overhang};
	return content + 2.0f * gl::vec2(style.WindowPadding) +
	       gl::vec2(0.0f, ImGui::GetFrameHeight()); // title bar
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
		.debugger = motherBoard->getDebugger(),
		.dummyDevice = &cpuInterface.getDummyDevice(),
		.metrics = getMetrics(),
		.drawList = nullptr, // only valid inside the window
		.emptyColor = ImGui::GetColorU32(getColor(imColor::GRAY), 0.4f),
		.occupiedColor = ImGui::GetColorU32(ImGuiCol_Header),
		.externalColor = getColor(imColor::YELLOW_BG),
		.outlineColor = ImGui::GetColorU32(ImGuiCol_Text),
		.textColor = ImGui::GetColorU32(ImGuiCol_Text),
		.dimTextColor = ImGui::GetColorU32(ImGuiCol_TextDisabled),
		.mouse = gl::vec2(ImGui::GetIO().MousePos),
		// Default initialization is what we want for the rest, but spell it out
		// to keep gcc's -Wmissing-field-initializers quiet.
		.hovered = {},
		.outlines = {},
		.separators = {},
		.tinyBlocks = {},
		.blocks = {},
		.scratch = {},
	};
	ctx.outlines.reserve(4);
	ctx.separators.reserve(64);

	ImGui::SetNextWindowSize(defaultWindowSize(ctx.metrics), ImGuiCond_FirstUseEver);
	im::Window("Slot map", &show, [&]{
		ctx.drawList = ImGui::GetWindowDrawList();
		drawSlotMap(ctx);

		if (ctx.hovered.valid && ImGui::IsWindowHovered()) {
			drawDeviceToolTip(ctx);
		}
	});
}

} // namespace openmsx
