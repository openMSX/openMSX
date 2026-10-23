#ifndef DEBUGGABLE_BITMAP_VIEWER_HH
#define DEBUGGABLE_BITMAP_VIEWER_HH

#include "BitmapDecoderSettings.hh"
#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "gl_vec.hh"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

class Debuggable;
class SymbolManager;

// Shows (part of) any debuggable as a configurable bitmap.
class DebuggableBitmapViewer final : public ImGuiPart
{
public:
	DebuggableBitmapViewer(ImGuiManager& manager_, std::string_view debuggableName, size_t index);
	[[nodiscard]] std::string_view getDebuggableName() const { return {title.data(), debuggableNameSize}; }

	void setOffset(unsigned offset);

	// ImGuiPart
	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = true;

private:
	using Layout = BitmapDecoder::Layout;
	using FieldErrors = BitmapDecoder::FieldErrors;
	using LevelSettings = BitmapDecoder::LevelSettings;

	using Resolved = std::expected<Layout, FieldErrors>;

	[[nodiscard]] BitmapDecoder::Settings getSettings() const;
	[[nodiscard]] size_t numLevels() const { return size_t(tiling) + 1; }
	void setTiling(int newTiling, const Resolved& resolved);
	void validateColorMode();

	void drawLayout(unsigned memSize, unsigned offset, std::string_view offsetError);
	void drawStepTable(const Resolved& resolved);
	void drawSummary(unsigned memSize, unsigned offset, const Layout& layout);
	void drawView();
	void drawErrors(std::string_view offsetError, const Resolved& resolved);
	void drawImage(Debuggable& debuggable, unsigned memSize, unsigned offset, const Layout& layout,
	               std::span<const uint32_t, 16> palette);
	void drawHover(unsigned memSize, unsigned offset, const Layout& layout, gl::ivec2 pos);

private:
	SymbolManager& symbolManager;
	size_t debuggableNameSize;
	std::string title; // debuggableName + " bitmap" + suffix

	std::string offsetExpr = "0";
	int bitsPerItem = 8;
	int colorMode = int(BitmapDecoder::ColorMode::BRG233);
	int bitOrder = int(BitmapDecoder::Order::MSB_FIRST);
	int byteOrder = int(BitmapDecoder::Order::LSB_FIRST);
	int tiling = 0; // number of tiling levels, 0 to MAX_LEVELS - 1
	std::array<LevelSettings, BitmapDecoder::MAX_LEVELS> levels; // also the inactive ones (beyond 'tiling')

	int zoom = 1; // 0->1x, 1->2x, ..., 7->8x
	bool pixelGrid = true;
	gl::vec4 pixelGridColor{0.0f, 0.0f, 0.0f, 0.5f}; // RGBA
	gl::vec4 checkerBoardColor1{0.2f, 0.2f, 0.2f, 0.8f}; // RGBA
	gl::vec4 checkerBoardColor2{0.4f, 0.4f, 0.4f, 0.8f}; // RGBA
	int checkerBoardSize = 4; // 0 -> no checkerboard
	// Size 0: an overlay line; otherwise a gutter of that many screen pixels.
	// When both grids are enabled, the group size is at least the tile size.
	bool tileGrid = true;
	gl::vec4 tileGridColor{0.0f, 0.6f, 1.0f, 0.8f}; // RGBA
	int tileGridSize = 1;
	bool groupGrid = true;
	gl::vec4 groupGridColor{1.0f, 0.5f, 0.0f, 0.8f}; // RGBA
	int groupGridSize = 2;

	static constexpr int MAX_CHECKERBOARD_SIZE = 255;
	static constexpr int MAX_GUTTER_SIZE = 8;
	static constexpr int MAX_GUTTER_TILES = 16384; // beyond this, gutters are drawn as lines
	static constexpr auto bitsPerItemChoices = {1, 2, 4, 8, 16, 24, 32}; // same as BitmapDecoder::validBitsPerItem()
	static constexpr auto persistentElements = std::tuple{
		PersistentElement    {"show",               &DebuggableBitmapViewer::show},
		PersistentElement    {"offset",             &DebuggableBitmapViewer::offsetExpr},
		PersistentElementEnum{"bitsPerItem",        &DebuggableBitmapViewer::bitsPerItem, bitsPerItemChoices},
		PersistentElementMax {"colorMode",          &DebuggableBitmapViewer::colorMode, int(BitmapDecoder::ColorMode::RGBA8888) + 1},
		PersistentElementMax {"bitOrder",           &DebuggableBitmapViewer::bitOrder, 2},
		PersistentElementMax {"byteOrder",          &DebuggableBitmapViewer::byteOrder, 2},
		PersistentElementMax {"tiling",             &DebuggableBitmapViewer::tiling, int(BitmapDecoder::MAX_LEVELS)},
		PersistentElementMax {"zoom",               &DebuggableBitmapViewer::zoom, 8},
		PersistentElement    {"pixelGrid",          &DebuggableBitmapViewer::pixelGrid},
		PersistentElement    {"pixelGridColor",     &DebuggableBitmapViewer::pixelGridColor},
		PersistentElement    {"checkerBoardColor1", &DebuggableBitmapViewer::checkerBoardColor1},
		PersistentElement    {"checkerBoardColor2", &DebuggableBitmapViewer::checkerBoardColor2},
		PersistentElementMax {"checkerBoardSize",   &DebuggableBitmapViewer::checkerBoardSize, MAX_CHECKERBOARD_SIZE + 1},
		PersistentElement    {"tileGrid",           &DebuggableBitmapViewer::tileGrid},
		PersistentElement    {"tileGridColor",      &DebuggableBitmapViewer::tileGridColor},
		PersistentElementMax {"tileGridSize",       &DebuggableBitmapViewer::tileGridSize, MAX_GUTTER_SIZE + 1},
		PersistentElement    {"groupGrid",          &DebuggableBitmapViewer::groupGrid},
		PersistentElement    {"groupGridColor",     &DebuggableBitmapViewer::groupGridColor},
		PersistentElementMax {"groupGridSize",      &DebuggableBitmapViewer::groupGridSize, MAX_GUTTER_SIZE + 1},
	};

	// [Internal State]
	std::vector<uint8_t> dataBuf;
	std::vector<uint32_t> pixelBuf;
	std::optional<gl::Texture> imageTex;
	std::optional<gl::Texture> pixelGridTex;
	std::optional<gl::Texture> checkerTex;
};

} // namespace openmsx

#endif
