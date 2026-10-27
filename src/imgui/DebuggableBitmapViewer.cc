#include "DebuggableBitmapViewer.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiPalette.hh"
#include "ImGuiUtils.hh"

#include "Debuggable.hh"
#include "Debugger.hh"
#include "MSXMotherBoard.hh"
#include "SymbolManager.hh"
#include "VDP.hh"

#include "enumerate.hh"
#include "one_of.hh"
#include "stl.hh"
#include "strCat.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <optional>
#include <tuple>
#include <utility>

namespace openmsx {

using namespace std::literals;
using BitmapDecoder::ColorMode;
using BitmapDecoder::Field;

// In the order of 'Field'.
static constexpr std::array levelFields = {
	std::pair{"countX", &BitmapDecoder::LevelSettings::countX},
	std::pair{"countY", &BitmapDecoder::LevelSettings::countY},
	std::pair{"stepX",  &BitmapDecoder::LevelSettings::stepX},
	std::pair{"stepY",  &BitmapDecoder::LevelSettings::stepY},
};

struct ColorModeInfo {
	const char* name;
	const char* bitLayout; // of the value, so after applying the endianness
};
// Indexed by 'ColorMode'.
static constexpr std::array colorModeInfos = {
	ColorModeInfo{"Grayscale", ""},
	ColorModeInfo{"Palette",   ""},
	ColorModeInfo{"BRG233",    "bits 1-0: blue, 4-2: red, 7-5: green (as in MSX screen 8)"},
	ColorModeInfo{"BRGX5551",  "bits 4-0: blue, 9-5: red, 14-10: green, 15: ignored"},
	ColorModeInfo{"BRGYs5551", "bits 4-0: blue, 9-5: red, 14-10: green, 15: transparent when set (as in the V9990)"},
	ColorModeInfo{"BGR565",    "bits 4-0: blue, 10-5: green, 15-11: red"},
	ColorModeInfo{"RGB565",    "bits 4-0: red, 10-5: green, 15-11: blue"},
	ColorModeInfo{"RGB888",    "bits 7-0: red, 15-8: green, 23-16: blue"},
	ColorModeInfo{"BGRA8888",  "bits 7-0: blue, 15-8: green, 23-16: red, 31-24: alpha"},
	ColorModeInfo{"RGBA8888",  "bits 7-0: red, 15-8: green, 23-16: blue, 31-24: alpha"},
};
static_assert(colorModeInfos.size() == size_t(ColorMode::RGBA8888) + 1);

[[nodiscard]] static const char* colorModeName(ColorMode mode, int bitsPerItem)
{
	if (mode == ColorMode::GRAYSCALE && bitsPerItem == 1) return "Black & white";
	return colorModeInfos[size_t(mode)].name;
}

[[nodiscard]] static std::string colorModeHelp(ColorMode mode, int bitsPerItem)
{
	switch (mode) {
	case ColorMode::GRAYSCALE:
		if (bitsPerItem == 1) return "0: black, 1: white";
		if (bitsPerItem <= 8) return strCat("0: black, ", (1u << bitsPerItem) - 1, ": white");
		return "Only the 8 most significant bits are used: 0: black, 255: white";
	case ColorMode::PALETTE:
		return bitsPerItem == 2 ? "Index in the first 4 colors of the palette"
		                        : "Index in the 16-color palette";
	default:
		return colorModeInfos[size_t(mode)].bitLayout;
	}
}

[[nodiscard]] static unsigned getValue(const BitmapDecoder::Level& level, Field field)
{
	switch (field) {
		case Field::COUNT_X: return level.countX;
		case Field::COUNT_Y: return level.countY;
		case Field::STEP_X:  return level.stepX;
		case Field::STEP_Y:  return level.stepY;
	}
	UNREACHABLE;
}

// With as many digits as 'maxValue' needs.
[[nodiscard]] static std::string formatHex(unsigned maxValue, unsigned value)
{
	size_t digits = 1;
	for (unsigned n = maxValue >> 4; n != 0; n >>= 4) ++digits;
	return strCat("0x", hex_string<HexCase::upper>(Digits{digits}, value));
}

// A blank count or step field means 'auto'.
[[nodiscard]] static bool isBlank(std::string_view str)
{
	return str.find_first_not_of(" \t") == std::string_view::npos;
}

DebuggableBitmapViewer::DebuggableBitmapViewer(ImGuiManager& manager_, std::string_view debuggableName, size_t index)
	: ImGuiPart(manager_)
	, symbolManager(manager.getReactor().getSymbolManager())
	, debuggableNameSize(debuggableName.size())
	, title(strCat(debuggableName, " bitmap", strCat_if(index, " (", index + 1, ')')))
{
	levels[0].countX = "256";
}

void DebuggableBitmapViewer::setOffset(unsigned offset)
{
	offsetExpr = formatHex(std::max(offset, 0xFFFFu), offset);
}

void DebuggableBitmapViewer::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	for (auto i : xrange(levels.size())) {
		for (const auto& [name, member] : levelFields) {
			buf.appendf("level%d.%s=%s\n", int(i), name, (levels[i].*member).c_str());
		}
	}
}

void DebuggableBitmapViewer::loadLine(std::string_view name, zstring_view value)
{
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else {
		for (auto i : xrange(levels.size())) {
			for (const auto& [fieldName, member] : levelFields) {
				if (name == tmpStrCat("level", i, '.', fieldName)) {
					levels[i].*member = std::string(value);
				}
			}
		}
	}
}

BitmapDecoder::Settings DebuggableBitmapViewer::getSettings() const
{
	BitmapDecoder::Settings result{
		.bitsPerItem = unsigned(bitsPerItem),
		.colorMode = ColorMode(colorMode),
		.bitOrder = BitmapDecoder::Order(bitOrder),
		.byteOrder = BitmapDecoder::Order(byteOrder),
		.levels = {},
	};
	for (auto i : xrange(numLevels())) {
		result.levels.push_back(levels[i]);
	}
	return result;
}

void DebuggableBitmapViewer::setTiling(int newTiling, const Resolved& resolved)
{
	auto oldLevels = numLevels();
	tiling = newTiling;
	auto newLevels = numLevels();
	// Inactive levels keep their settings, so that they come back when going up again.
	if (newLevels <= oldLevels) return;

	// Only the outermost height can be auto: freeze the old outermost one,
	// and use 1 for an auto height of a reactivated level. A level that was
	// never used becomes 1 x 1 (1 x auto when outermost), so the image stays
	// the same.
	auto& outer = levels[oldLevels - 1];
	if (resolved && isBlank(outer.countY)) {
		outer.countY = strCat(resolved->levels[oldLevels - 1].countY);
	}
	for (auto i : xrange(oldLevels, newLevels)) {
		if (isBlank(levels[i].countX)) levels[i].countX = "1";
	}
	for (auto i : xrange(newLevels - 1)) {
		if (isBlank(levels[i].countY)) levels[i].countY = "1";
	}
}

void DebuggableBitmapViewer::validateColorMode()
{
	auto modes = BitmapDecoder::validColorModes(unsigned(bitsPerItem));
	if (!contains(modes, ColorMode(colorMode))) {
		colorMode = int(modes.front());
	}
}

void DebuggableBitmapViewer::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	auto fontSize = ImGui::GetFontSize();
	ImGui::SetNextWindowSize(gl::vec2{52, 45} * fontSize, ImGuiCond_FirstUseEver);
	im::Window(title.c_str(), &show, [&]{
		auto* debuggable = motherBoard->getDebugger().findDebuggable(getDebuggableName());
		if (!debuggable) {
			ImGui::StrCat("Debuggable '", getDebuggableName(), "' is not available.");
			return;
		}
		unsigned memSize = debuggable->getSize();

		auto& interp = manager.getInterpreter();
		auto parsedOffset = parseAddressExpr(offsetExpr, symbolManager, interp);
		std::string offsetError;
		if (!parsedOffset) {
			offsetError = std::move(parsedOffset.error());
		} else if (*parsedOffset >= memSize) {
			offsetError = strCat("Must be less than the size of the debuggable: ", formatHex(memSize, memSize));
		}
		unsigned offset = offsetError.empty() ? *parsedOffset : 0;

		validateColorMode();
		auto evaluate = [&](std::string_view str) { return parseAddressExpr(str, symbolManager, interp); };
		auto resolved = BitmapDecoder::resolve(getSettings(), memSize - offset, evaluate);
		bool valid = offsetError.empty() && resolved;

		im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			drawLayout(memSize, offset, offsetError);
			ImGui::Separator();

			int flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_BordersInnerV;
			im::Table("##settings", 2, flags, [&]{
				if (ImGui::TableNextColumn()) {
					ImGui::SetNextItemWidth(6.0f * fontSize);
					if (int newTiling = tiling; ImGui::Combo("Tiling", &newTiling, "No tiling\0" "1 level\0" "2 levels\0")) {
						setTiling(newTiling, resolved);
					}
					simpleToolTip("No tiling: one rectangle of pixels.\n"
						"1 level: pixels form tiles, tiles form a sheet.\n"
						"2 levels: pixels form tiles, tiles form groups, groups form a sheet.");
					im::Indent([&]{
						drawStepTable(resolved);
					});
				}
				if (ImGui::TableNextColumn()) {
					drawView();
				}
			});
			if (valid) {
				drawSummary(memSize, offset, *resolved);
			} else {
				ImGui::NewLine();
			}
			ImGui::Separator();
		});

		if (valid) {
			const auto* vdp = dynamic_cast<const VDP*>(motherBoard->findDevice("VDP"));
			drawImage(*debuggable, memSize, offset, *resolved, manager.palette->getPalette(vdp));
		} else {
			drawErrors(offsetError, resolved);
		}
	});
}

void DebuggableBitmapViewer::drawErrors(std::string_view offsetError, const Resolved& resolved)
{
	im::TextWrapPos([&]{
		if (!offsetError.empty()) {
			ImGui::TextUnformatted(offsetError);
		}
		if (resolved) return;
		std::span errors = resolved.error();
		for (auto [i, error] : enumerate(errors)) {
			if (!error.empty() && !contains(errors.first(i), error)) {
				ImGui::TextUnformatted(error);
			}
		}
	});
}

void DebuggableBitmapViewer::drawLayout(unsigned memSize, unsigned offset, std::string_view offsetError)
{
	auto fontSize = ImGui::GetFontSize();

	im::StyleColor(!offsetError.empty(), ImGuiCol_Text, getColor(imColor::ERROR), [&]{
		ImGui::SetNextItemWidth(12.0f * fontSize);
		ImGui::InputText("Offset", &offsetExpr);
		simpleToolTip([&]{
			return offsetError.empty() ? formatHex(memSize - 1, offset) : std::string(offsetError);
		});
	});
	HelpMarker("Offsets can be entered as:\n"
		"  Decimal or hexadecimal values (e.g. 0x1234)\n"
		"  A calculation like 0x1234 + 7*22\n"
		"  The name of a label (e.g. CHPUT)\n"
		"  A Tcl expression (e.g. [reg hl] to follow the content of register HL)\n"
		"\n"
		"The offset is re-evaluated every frame.");
	ImGui::Separator();

	auto bitsName = [](int bits) { return strCat(bits, bits == 1 ? " bit" : " bits"); };
	ImGui::SetNextItemWidth(6.0f * fontSize);
	im::Combo("Bits per item", bitsName(bitsPerItem).c_str(), [&]{
		for (int bits : bitsPerItemChoices) {
			if (ImGui::Selectable(bitsName(bits).c_str(), bits == bitsPerItem)) {
				bitsPerItem = bits;
				validateColorMode();
			}
		}
	});
	simpleToolTip("The size of one pixel in the data. Changing it keeps custom steps in pixels.");
	if (bitsPerItem < 8) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(7.0f * fontSize);
		ImGui::Combo("Bit order", &bitOrder, "MSB first\0LSB first\0");
		simpleToolTip("Where in a byte the first pixel is: in the most or in the least significant bits.");
	} else if (bitsPerItem > 8) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(7.0f * fontSize);
		ImGui::Combo("Endianness", &byteOrder, "Big endian\0Little endian\0");
		simpleToolTip("Big endian: the first byte is the most significant one.\n"
		              "Little endian (Z80 data): the first byte is the least significant one.");
	}

	im::Indent([&]{
		ImGui::SetNextItemWidth(8.0f * fontSize);
		im::Combo("Color mode", colorModeName(ColorMode(colorMode), bitsPerItem), [&]{
			for (auto mode : BitmapDecoder::validColorModes(unsigned(bitsPerItem))) {
				if (ImGui::Selectable(colorModeName(mode, bitsPerItem), int(mode) == colorMode)) {
					colorMode = int(mode);
				}
				simpleToolTip([&]{ return colorModeHelp(mode, bitsPerItem); });
			}
		});
		simpleToolTip([&]{ return colorModeHelp(ColorMode(colorMode), bitsPerItem); });

		if (ColorMode(colorMode) == ColorMode::PALETTE) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(6.0f * fontSize);
			ImGui::Combo("Palette", &manager.palette->whichPalette, "VDP\000Custom\000Fixed\000");
			simpleToolTip("Shared with the other viewers");

			ImGui::SameLine();
			if (ImGui::Button("Editor")) {
				manager.palette->window.raise();
			}
			simpleToolTip("Open palette editor");
		}
	});
}

void DebuggableBitmapViewer::drawStepTable(const Resolved& resolved)
{
	static constexpr std::array levelNames = {"Pixel", "Tile", "Group"};
	static constexpr std::array unitNames = {"pixel", "tile", "group"};

	auto fontSize = ImGui::GetFontSize();

	auto num = numLevels();

	auto drawField = [&](size_t level, Field field) {
		auto index = BitmapDecoder::fieldIndex(level, field);
		std::string_view error = resolved ? std::string_view{} : std::string_view{resolved.error()[index]};
		bool isCount = field == one_of(Field::COUNT_X, Field::COUNT_Y);
		bool outerHeight = field == Field::COUNT_Y && level == num - 1;
		auto hint = (isCount && !outerHeight) ? std::string{}
		          : resolved ? strCat("auto: ", getValue(resolved->levels[level], field))
		          : std::string("auto");
		const auto& [name, member] = levelFields[size_t(field)];
		im::StyleColor(!error.empty(), ImGuiCol_Text, getColor(imColor::ERROR), [&]{
			ImGui::SetNextItemWidth((isCount ? 5.0f : 7.0f) * fontSize);
			ImGui::InputTextWithHint(tmpStrCat("##", name).c_str(), hint.c_str(), &(levels[level].*member));
		});
		simpleToolTip([&]{
			if (!error.empty()) return std::string(error);
			std::string_view unit = unitNames[level];
			auto help = [&]{
				switch (field) {
					case Field::COUNT_X: return strCat("Number of ", unit, "s per row.");
					case Field::COUNT_Y: return strCat("Number of rows of ", unit, "s.",
					                                   outerHeight ? "\nEmpty = as many rows as fit." : "");
					case Field::STEP_X:  return strCat("Distance from one ", unit, " to the next one on its right, in pixels.\n"
					                                   "Empty = auto.");
					case Field::STEP_Y:  return strCat("Distance from one ", unit, " to the next one below it, in pixels.\n"
					                                   "Empty = auto.");
				}
				UNREACHABLE;
			}();
			return strCat(help, "\nSame syntax as the offset, e.g. 16 * 8.");
		});
	};

	auto drawUnit = [&](size_t level, Field field) {
		ImGui::AlignTextToFramePadding();
		if (!resolved) {
			ImGui::TextUnformatted("px"sv);
			return;
		}
		auto step = getValue(resolved->levels[level], field);
		auto bits = uint64_t(step) * unsigned(bitsPerItem);
		if (auto bytes = bits / 8; (bits % 8 == 0) && (bytes != step)) {
			ImGui::StrCat("px (= ", bytes, bytes == 1 ? " byte)" : " bytes)");
		} else {
			ImGui::TextUnformatted("px"sv);
		}
	};

	im::Table("##steps", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX, [&]{
		for (auto level : xrange(num)) {
			for (bool vertical : {false, true}) {
				im::ID(int(2 * level + vertical), [&]{
					if (ImGui::TableNextColumn() && num > 1 && !vertical) {
						ImGui::AlignTextToFramePadding();
						ImGui::TextUnformatted(levelNames[level]);
					}
					if (ImGui::TableNextColumn()) {
						ImGui::AlignTextToFramePadding();
						ImGui::TextUnformatted(vertical ? "Y" : "X");
					}
					if (ImGui::TableNextColumn()) {
						drawField(level, vertical ? Field::COUNT_Y : Field::COUNT_X);
					}
					if (ImGui::TableNextColumn()) {
						ImGui::AlignTextToFramePadding();
						ImGui::TextUnformatted(" step");
					}
					if (ImGui::TableNextColumn()) {
						drawField(level, vertical ? Field::STEP_Y : Field::STEP_X);
					}
					if (ImGui::TableNextColumn()) {
						drawUnit(level, vertical ? Field::STEP_Y : Field::STEP_X);
					}
				});
			}
		}
	});
}

void DebuggableBitmapViewer::drawSummary(unsigned memSize, unsigned offset, const Layout& layout)
{
	auto size = layout.imageSize();
	auto bytes = layout.dataSize();
	auto available = memSize - offset;
	bool missing = bytes > available;
	auto text = missing
		? strCat(size.x, 'x', size.y, " px, needs ", bytes, " bytes, ", available, " available")
		: strCat(size.x, 'x', size.y, " px, ", bytes, " bytes, ",
		         formatHex(memSize - 1, offset), " - ", formatHex(memSize - 1, offset + bytes - 1));

	// An auto height stops at MAX_IMAGE_SIZE, even when more rows would fit in the data.
	const auto& outer = layout.levels.back();
	auto innerHeight = unsigned(size.y) / outer.countY;
	bool heightLimited = isBlank(levels[layout.levels.size() - 1].countY)
	                  && (unsigned(size.y) + innerHeight > BitmapDecoder::MAX_IMAGE_SIZE)
	                  && (uint64_t(bytes) * 8 + uint64_t(outer.stepY) * unsigned(bitsPerItem) <= uint64_t(available) * 8);
	if (heightLimited) {
		auto extra = tmpStrCat(", height limited to ", BitmapDecoder::MAX_IMAGE_SIZE);
		if (ImGui::CalcTextSize(text.c_str()).x + ImGui::CalcTextSize(extra.c_str()).x <= ImGui::GetContentRegionAvail().x) {
			text += extra;
		}
	}
	im::StyleColor(missing, ImGuiCol_Text, getColor(imColor::WARNING), [&]{
		ImGui::TextUnformatted(text);
	});
}

void DebuggableBitmapViewer::drawView()
{
	auto fontSize = ImGui::GetFontSize();
	ImGui::SetNextItemWidth(4.0f * fontSize);
	ImGui::Combo("Zoom", &zoom, "1x\0002x\0003x\0004x\0005x\0006x\0007x\0008x\000");
	ImGui::Separator();

	auto gridRow = [&](const char* label, bool& enabled, gl::vec4& color, int* size) {
		if (ImGui::TableNextColumn()) {
			ImGui::Checkbox(label, &enabled);
		}
		if (ImGui::TableNextColumn()) {
			im::Disabled(!enabled, [&]{
				ImGui::ColorEdit4(tmpStrCat(label, " color").c_str(), color.data(),
					ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
				if (!size) return;
				ImGui::SameLine();
				ImGui::SetNextItemWidth(6.0f * fontSize);
				if (ImGui::InputInt(tmpStrCat("Size##", label).c_str(), size)) {
					*size = std::clamp(*size, 0, MAX_GUTTER_SIZE);
				}
				simpleToolTip("0: a 1-pixel line on the top row and left column of each cell\n"
					"1 or more: a gap of that many screen pixels between the cells");
			});
		}
	};
	im::Table("##grid", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX, [&]{
		im::Disabled(zoom == 0, [&]{
			gridRow("Pixel grid", pixelGrid, pixelGridColor, nullptr);
		});
		if (tiling >= 1) {
			gridRow("Tile grid", tileGrid, tileGridColor, &tileGridSize);
		}
		if (tiling >= 2) {
			gridRow("Group grid", groupGrid, groupGridColor, &groupGridSize);
			if (tileGrid && groupGrid) {
				groupGridSize = std::max(groupGridSize, tileGridSize);
			}
		}
		if (ImGui::TableNextColumn()) {
			ImGui::Separator();
		}
		if (ImGui::TableNextColumn()) {
			ImGui::Separator();
		}

		if (ImGui::TableNextColumn()) {
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Checkerboard"sv);
			simpleToolTip("Background, visible through transparent pixels and pixels outside the debuggable.");
		}
		if (ImGui::TableNextColumn()) {
			auto checkerBoardColorFlags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha;
			ImGui::ColorEdit4("checkerboard color1", checkerBoardColor1.data(), checkerBoardColorFlags);
			ImGui::SameLine();
			ImGui::ColorEdit4("checkerboard color2", checkerBoardColor2.data(), checkerBoardColorFlags);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(6.0f * fontSize);
			if (ImGui::InputInt("Size", &checkerBoardSize)) {
				checkerBoardSize = std::clamp(checkerBoardSize, 0, MAX_CHECKERBOARD_SIZE);
			}
			simpleToolTip("0 = no checkerboard");
		}
	});
}

void DebuggableBitmapViewer::drawImage(Debuggable& debuggable, unsigned memSize, unsigned offset, const Layout& layout,
                                       std::span<const uint32_t, 16> palette)
{
	dataBuf.resize(std::min(layout.dataSize(), memSize - offset));
	debuggable.readBlock(offset, dataBuf);
	auto imageSize = layout.imageSize();
	pixelBuf.resize(size_t(imageSize.x) * size_t(imageSize.y));
	BitmapDecoder::decode(layout, palette, dataBuf, pixelBuf);

	if (!imageTex) {
		imageTex.emplace(false, false); // no interpolation, no wrapping
	}
	imageTex->bind();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageSize.x, imageSize.y, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, pixelBuf.data());

	int z = zoom + 1;
	bool showPixelGrid = pixelGrid && (z > 1);
	if (showPixelGrid) {
		auto color = ImGui::ColorConvertFloat4ToU32(pixelGridColor);
		std::array<uint32_t, 8 * 8> gridPixels;
		for (auto y : xrange(z)) {
			for (auto x : xrange(z)) {
				gridPixels[y * z + x] = (x == 0 || y == 0) ? color : 0;
			}
		}
		if (!pixelGridTex) {
			pixelGridTex.emplace(false, true); // no interpolation, with wrapping
		}
		pixelGridTex->bind();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, z, z, 0,
		             GL_RGBA, GL_UNSIGNED_BYTE, gridPixels.data());
	}
	if (checkerBoardSize) {
		auto col1 = ImGui::ColorConvertFloat4ToU32(checkerBoardColor1);
		auto col2 = ImGui::ColorConvertFloat4ToU32(checkerBoardColor2);
		std::array<uint32_t, 2 * 2> checkerPixels = {col1, col2, col2, col1};
		if (!checkerTex) {
			checkerTex.emplace(false, true); // no interpolation, with wrapping
		}
		checkerTex->bind();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0,
		             GL_RGBA, GL_UNSIGNED_BYTE, checkerPixels.data());
	}

	// The enabled tile and group grids, innermost first.
	struct Grid {
		gl::ivec2 cell; // in pixels
		ImU32 color;
		int size; // 0: overlay line, otherwise gutter
	};
	static_vector<Grid, BitmapDecoder::MAX_LEVELS - 1> grids;
	gl::ivec2 cell{1, 1};
	for (auto i : xrange(size_t(1), layout.levels.size())) {
		const auto& inner = layout.levels[i - 1];
		cell = cell * gl::ivec2(int(inner.countX), int(inner.countY));
		auto [enabled, color, size] = (i == 1) ? std::tuple{tileGrid, tileGridColor, tileGridSize}
		                                       : std::tuple{groupGrid, groupGridColor, groupGridSize};
		if (enabled) grids.emplace_back(cell, ImGui::ColorConvertFloat4ToU32(color), size);
	}
	auto hasGutter = [](const Grid& g) { return g.size != 0; };
	// Without gutters the image is a single quad, otherwise one quad per cell of this grid.
	const Grid* blockGrid = nullptr;
	if (auto it = std::ranges::find_if(grids, hasGutter); it != grids.end()) {
		auto numBlocks = (imageSize.x / it->cell.x) * (imageSize.y / it->cell.y);
		if (numBlocks > MAX_GUTTER_TILES) {
			for (auto& g : grids) g.size = 0;
			im::StyleColor(ImGuiCol_Text, getColor(imColor::WARNING), [&]{
				ImGui::StrCat("More than ", MAX_GUTTER_TILES, " tiles: gutters are drawn as lines.");
			});
		} else {
			blockGrid = &*it;
		}
	}
	// Where several grids have a boundary, only the outermost one is drawn.
	// At the image edge (p = 0) a gutter draws nothing, so it doesn't count.
	auto gridAt = [&](int p, int axis) -> const Grid* {
		for (auto i = grids.size(); i-- > 0;) {
			const auto& g = grids[i];
			if ((p % g.cell[axis] == 0) && (p != 0 || !hasGutter(g))) return &g;
		}
		return nullptr;
	};
	auto forEachBoundary = [&](const Grid& g, int axis, int first, std::invocable<int> auto action) {
		for (int p = first; p < imageSize[axis]; p += g.cell[axis]) {
			if (gridAt(p, axis) == &g) action(p);
		}
	};
	// Per axis: the screen offset of each pixel (zoomed, gutters included),
	// followed by the total size.
	auto makeEdges = [&](int axis) {
		int n = imageSize[axis];
		std::vector<int> edges(size_t(n) + 1);
		for (int p = 1; p <= n; ++p) {
			const auto* g = (p < n) ? gridAt(p, axis) : nullptr;
			edges[p] = edges[p - 1] + z + (g ? g->size : 0);
		}
		return edges;
	};
	auto edgesX = makeEdges(0);
	auto edgesY = makeEdges(1);

	auto imgSize = gl::vec2(imageSize);
	auto totalSize = gl::vec2(float(edgesX.back()), float(edgesY.back()));
	auto availSize = gl::vec2(ImGui::GetContentRegionAvail()) - gl::vec2(0.0f, ImGui::GetTextLineHeightWithSpacing());
	auto reqSize = totalSize + gl::vec2(ImGui::GetStyle().ScrollbarSize);
	auto childSize = max(min(availSize, reqSize), gl::vec2(4.0f * ImGui::GetFrameHeight()));
	gl::vec2 scrnPos;
	im::Child("##image", childSize, 0, ImGuiWindowFlags_HorizontalScrollbar, [&]{
		scrnPos = ImGui::GetCursorScreenPos();
		auto* drawList = ImGui::GetWindowDrawList();
		if (!blockGrid) {
			auto pos = ImGui::GetCursorPos();
			if (checkerBoardSize) {
				ImGui::Image(checkerTex->getImGui(), totalSize, {}, totalSize / (4.0f * float(checkerBoardSize)));
				ImGui::SetCursorPos(pos);
			}
			ImGui::Image(imageTex->getImGui(), totalSize);
			if (showPixelGrid) {
				ImGui::SetCursorPos(pos);
				ImGui::Image(pixelGridTex->getImGui(), totalSize, {}, imgSize);
			}
		} else {
			if (checkerBoardSize) {
				drawList->AddImage(checkerTex->getImGui(), scrnPos, scrnPos + totalSize,
				                   {}, totalSize / (4.0f * float(checkerBoardSize)));
			}
			// inner grids first, so that the outer ones are drawn on top
			for (const auto& g : grids) {
				if (!hasGutter(g)) continue;
				auto gap = float(g.size);
				forEachBoundary(g, 0, g.cell.x, [&](int p) {
					auto x = scrnPos.x + float(edgesX[p]);
					drawList->AddRectFilled({x - gap, scrnPos.y}, {x, scrnPos.y + totalSize.y}, g.color);
				});
				forEachBoundary(g, 1, g.cell.y, [&](int p) {
					auto y = scrnPos.y + float(edgesY[p]);
					drawList->AddRectFilled({scrnPos.x, y - gap}, {scrnPos.x + totalSize.x, y}, g.color);
				});
			}
			auto block = blockGrid->cell;
			gl::vec2 clipMin = drawList->GetClipRectMin();
			gl::vec2 clipMax = drawList->GetClipRectMax();
			for (int y = 0; y < imageSize.y; y += block.y) {
				auto y0 = scrnPos.y + float(edgesY[y]);
				auto y1 = scrnPos.y + float(edgesY[y + block.y - 1] + z);
				if ((y1 < clipMin.y) || (y0 > clipMax.y)) continue;
				for (int x = 0; x < imageSize.x; x += block.x) {
					auto x0 = scrnPos.x + float(edgesX[x]);
					auto x1 = scrnPos.x + float(edgesX[x + block.x - 1] + z);
					if ((x1 < clipMin.x) || (x0 > clipMax.x)) continue;
					gl::vec2 p0{x0, y0};
					gl::vec2 p1{x1, y1};
					auto uv0 = gl::vec2(float(x), float(y)) / imgSize;
					auto uv1 = gl::vec2(float(x + block.x), float(y + block.y)) / imgSize;
					drawList->AddImage(imageTex->getImGui(), p0, p1, uv0, uv1);
					if (showPixelGrid) {
						drawList->AddImage(pixelGridTex->getImGui(), p0, p1, {}, gl::vec2(block));
					}
				}
			}
			ImGui::Dummy(totalSize);
		}
		for (const auto& g : grids) {
			if (hasGutter(g)) continue;
			forEachBoundary(g, 0, 0, [&](int p) {
				auto x = scrnPos.x + float(edgesX[p]);
				drawList->AddRectFilled({x, scrnPos.y}, {x + 1.0f, scrnPos.y + totalSize.y}, g.color);
			});
			forEachBoundary(g, 1, 0, [&](int p) {
				auto y = scrnPos.y + float(edgesY[p]);
				drawList->AddRectFilled({scrnPos.x, y}, {scrnPos.x + totalSize.x, y + 1.0f}, g.color);
			});
		}
	});
	if (ImGui::IsItemHovered()) {
		// The pixel at screen offset 'm', or nothing over a gutter.
		auto toPixel = [&](const std::vector<int>& edges, float m) -> std::optional<int> {
			auto starts = std::span(edges).first(edges.size() - 1);
			auto mi = int(std::floor(m));
			auto it = std::ranges::upper_bound(starts, mi);
			if (it == starts.begin()) return {};
			auto p = int(it - starts.begin()) - 1;
			if (mi >= edges[p] + z) return {};
			return p;
		};
		auto mouse = gl::vec2(ImGui::GetIO().MousePos) - scrnPos;
		if (auto px = toPixel(edgesX, mouse.x)) {
			if (auto py = toPixel(edgesY, mouse.y)) {
				drawHover(memSize, offset, layout, {*px, *py});
			}
		}
	}
}

void DebuggableBitmapViewer::drawHover(unsigned memSize, unsigned offset, const Layout& layout, gl::ivec2 pos)
{
	static constexpr std::array levelNames = {"pixel", "tile", "group"};
	im::ScopedFont sf(manager.fontMono);

	std::string text = strCat("x=", pos.x, " y=", pos.y);
	auto x = unsigned(pos.x);
	auto y = unsigned(pos.y);
	uint32_t item = 0;
	for (auto [i, level] : enumerate(layout.levels)) {
		auto ix = x % level.countX; x /= level.countX;
		auto iy = y % level.countY; y /= level.countY;
		item += ix * level.stepX + iy * level.stepY;
		if (layout.levels.size() > 1) {
			strAppend(text, (i == 0 ? "  " : " "), levelNames[i], "=(", ix, ',', iy, ')');
		}
	}

	auto bits = BitmapDecoder::bitsPerItem(layout.format);
	auto bitAddr = item * bits;
	auto byteAddr = bitAddr / 8;
	strAppend(text, "  addr=", formatHex(memSize - 1, offset + byteAddr));
	unsigned shift = 0;
	if (bits < 8) {
		auto p = bitAddr % 8;
		shift = (layout.order == BitmapDecoder::Order::MSB_FIRST) ? (8 - p - bits) : p;
		if (bits == 1) {
			strAppend(text, " bit ", shift);
		} else {
			strAppend(text, " bits ", shift + bits - 1, '-', shift);
		}
	}

	if (uint64_t(bitAddr) + bits > uint64_t(dataBuf.size()) * 8) {
		strAppend(text, "  outside debuggable");
	} else {
		uint32_t value = 0;
		if (bits < 8) {
			value = (dataBuf[byteAddr] >> shift) & ((1u << bits) - 1);
		} else {
			for (auto i : xrange(bits / 8)) {
				uint32_t b = dataBuf[byteAddr + i];
				value = (layout.order == BitmapDecoder::Order::MSB_FIRST) ? ((value << 8) | b)
				                                                       : (value | (b << (8 * i)));
			}
		}
		auto color = pixelBuf[size_t(pos.y) * size_t(layout.imageSize().x) + size_t(pos.x)];
		auto alpha = color >> 24;
		strAppend(text, "  value=0x", hex_string<HexCase::upper>(Digits{(bits + 3) / 4}, value),
		          "  #", hex_string<2, HexCase::upper>((color >> 0) & 0xff),
		          hex_string<2, HexCase::upper>((color >> 8) & 0xff),
		          hex_string<2, HexCase::upper>((color >> 16) & 0xff));
		if (alpha != 0xff) strAppend(text, hex_string<2, HexCase::upper>(alpha));
	}
	ImGui::TextUnformatted(text);
}

} // namespace openmsx
