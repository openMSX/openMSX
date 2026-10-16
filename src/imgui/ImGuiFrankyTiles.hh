#ifndef IMGUI_FRANKY_TILES_HH
#define IMGUI_FRANKY_TILES_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "gl_vec.hh"

#include <cstdint>
#include <span>
#include <string>

namespace openmsx {

/** Tile viewer for the Sega VDP inside the Franky cartridge.
  *
  * This is the Franky counterpart of ImGuiCharacter (the MSX VDP tile
  * viewer). It supports the TMS9918-compatible modes 0-3 and the
  * SMS-specific mode 4.
  */
class ImGuiFrankyTiles final : public ImGuiPart
{
public:
	ImGuiFrankyTiles(ImGuiManager& manager_, size_t index);

	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	// VRAM-table addresses of the current mode (SMSVDPCore::drawScanlineMode*).
	struct FrankyTables {
		unsigned patBase;
		unsigned patMask; // pattern-number mask
		unsigned colBase;
		unsigned colMask; // pattern-number mask for the color table
		unsigned namBase;
		unsigned tile1Mask; // mode-4 tile-number masks (SMS1)
		unsigned tile2Mask;
	};

	// Mode/color settings for renderPatterns().
	struct PatternParams {
		int mode;
		int fgCol;
		int bgCol;
		bool overrideColorTable;
		uint8_t overrideColorValue;
		int lines;
	};

	static void renderPatterns(const PatternParams& params,
	                           std::span<const uint32_t, 32> palette,
	                           std::span<const uint8_t, 0x4000> vram,
	                           const FrankyTables& tables,
	                           std::span<uint32_t> output);
	void initHexDigits();

public:
	bool show = true;

private:
	std::string title;

	int zoom = 0; // 0->1x, 1->2x, ..., 7->8x
	bool overrideAll     = false;
	bool overrideMode    = false;
	bool overrideFgCol   = false;
	bool overrideBgCol   = false;
	bool overridePat     = false;
	bool overrideCol     = false;
	bool overrideNam     = false;
	bool overrideRows    = false;
	bool overrideColor0  = false;
	bool grid = true;
	bool nameTableOverlay = false;
	bool overrideColorTable = false;
	int overrideColorValue = 0xf1;
	bool rasterBeam = false;
	gl::vec4 gridColor{0.0f, 0.0f, 0.0f, 0.5f}; // RGBA
	gl::vec4 rasterBeamColor{1.0f, 0.0f, 0.0f, 0.8f}; // RGBA

	// Sega VDP screen modes; the values match the VDP mode numbers
	// (0-3 are the TMS9918-compatible modes, 4 is the SMS mode).
	enum FrankyScrnMode : int { GRAPHIC1, TEXT, GRAPHIC2, MULTICOLOR, MODE4, OTHER };
	int manualMode = 0;
	int manualFgCol = 15;
	int manualBgCol = 4;
	int manualPatBase = 0;
	int manualColBase = 0;
	int manualNamBase = 0;
	int manualRows = 0;
	int manualColor0 = 16;
	int patternPalette = 0; // mode 4: palette shown in the pattern table
	gl::vecN<2, int> gridPosition;

	gl::Texture patternTex{gl::Null{}}; // TODO also deallocate when needed
	gl::Texture gridTex   {gl::Null{}};
	gl::Texture smallHexDigits{gl::Null{}};

	static constexpr auto persistentElements = std::tuple{
		PersistentElement   {"show",            &ImGuiFrankyTiles::show},
		PersistentElement   {"overrideAll",     &ImGuiFrankyTiles::overrideAll},
		PersistentElement   {"overrideMode",    &ImGuiFrankyTiles::overrideMode},
		PersistentElement   {"overrideFgCol",   &ImGuiFrankyTiles::overrideFgCol},
		PersistentElement   {"overrideBgCol",   &ImGuiFrankyTiles::overrideBgCol},
		PersistentElement   {"overridePat",     &ImGuiFrankyTiles::overridePat},
		PersistentElement   {"overrideCol",     &ImGuiFrankyTiles::overrideCol},
		PersistentElement   {"overrideNam",     &ImGuiFrankyTiles::overrideNam},
		PersistentElement   {"overrideRows",    &ImGuiFrankyTiles::overrideRows},
		PersistentElement   {"overrideColor0",  &ImGuiFrankyTiles::overrideColor0},
		PersistentElementMax{"zoom",            &ImGuiFrankyTiles::zoom, 8},
		PersistentElement   {"showGrid",        &ImGuiFrankyTiles::grid},
		PersistentElement   {"overlay",         &ImGuiFrankyTiles::nameTableOverlay},
		PersistentElement   {"overrideColorTable", &ImGuiFrankyTiles::overrideColorTable},
		PersistentElement   {"overrideColorValue", &ImGuiFrankyTiles::overrideColorValue},
		PersistentElement   {"showRasterBeam",  &ImGuiFrankyTiles::rasterBeam},
		PersistentElement   {"rasterBeamColor", &ImGuiFrankyTiles::rasterBeamColor},
		PersistentElement   {"gridColor",       &ImGuiFrankyTiles::gridColor},
		PersistentElementMax{"mode",            &ImGuiFrankyTiles::manualMode, OTHER}, // GRAPHIC1..MULTICOLOR
		PersistentElementMax{"fgCol",           &ImGuiFrankyTiles::manualFgCol, 15},
		PersistentElementMax{"bgCol",           &ImGuiFrankyTiles::manualBgCol, 15},
		PersistentElementMax{"patBase",         &ImGuiFrankyTiles::manualPatBase, 0x4000},
		PersistentElementMax{"colBase",         &ImGuiFrankyTiles::manualColBase, 0x4000},
		PersistentElementMax{"namBase",         &ImGuiFrankyTiles::manualNamBase, 0x4000},
		PersistentElementMax{"rows",            &ImGuiFrankyTiles::manualRows, 2},
		PersistentElementMax{"color0",          &ImGuiFrankyTiles::manualColor0, 16 + 1},
		PersistentElementMax{"patternPalette",  &ImGuiFrankyTiles::patternPalette, 1}
	};
};

} // namespace openmsx

#endif
