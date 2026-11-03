#ifndef IMGUI_V9990_TILES_HH
#define IMGUI_V9990_TILES_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "gl_vec.hh"

#include <string>

namespace openmsx {

/** Tile viewer for the V9990 (GFX9000) pattern modes P1 and P2.
  *
  * This is the V9990 counterpart of ImGuiCharacter (the MSX VDP tile
  * viewer). It shows the pattern table(s) and the name table(s) of the
  * current mode, or of a manually selected mode and palette offset.
  */
class ImGuiV9990Tiles final : public ImGuiPart
{
public:
	ImGuiV9990Tiles(ImGuiManager& manager_, size_t index);

	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void initHexDigits();
	void updateGridTex(int zx, int zy);

public:
	bool show = true;

private:
	std::string title;

	int zoom = 0; // 0->1x, 1->2x, ..., 7->8x
	bool overrideAll      = false;
	bool overrideMode     = false;
	bool overridePaletteA = false;
	bool overridePaletteB = false;
	bool grid = true;
	bool nameTableOverlay = false;
	gl::vec4 gridColor{0.0f, 0.0f, 0.0f, 0.5f}; // RGBA

	int manualMode = 0; // 0->P1, 1->P2
	int manualPaletteA = 0; // 0-3
	int manualPaletteB = 1; // 0-3

	// The hovered pattern (layer and number), remembered for the copy popup.
	gl::vecN<2, int> hoveredPattern;

	gl::Texture patternTexA{gl::Null{}}; // TODO also deallocate when needed
	gl::Texture patternTexB{gl::Null{}}; // P1 layer B
	gl::Texture gridTex    {gl::Null{}};
	gl::Texture smallHexDigits{gl::Null{}};

	static constexpr auto persistentElements = std::tuple{
		PersistentElement   {"show",             &ImGuiV9990Tiles::show},
		PersistentElement   {"overrideAll",      &ImGuiV9990Tiles::overrideAll},
		PersistentElement   {"overrideMode",     &ImGuiV9990Tiles::overrideMode},
		PersistentElement   {"overridePaletteA", &ImGuiV9990Tiles::overridePaletteA},
		PersistentElement   {"overridePaletteB", &ImGuiV9990Tiles::overridePaletteB},
		PersistentElementMax{"zoom",             &ImGuiV9990Tiles::zoom, 8},
		PersistentElement   {"showGrid",         &ImGuiV9990Tiles::grid},
		PersistentElement   {"overlay",          &ImGuiV9990Tiles::nameTableOverlay},
		PersistentElement   {"gridColor",        &ImGuiV9990Tiles::gridColor},
		PersistentElementMax{"mode",             &ImGuiV9990Tiles::manualMode, 2}, // P1 or P2
		PersistentElementMax{"paletteA",         &ImGuiV9990Tiles::manualPaletteA, 4},
		PersistentElementMax{"paletteB",         &ImGuiV9990Tiles::manualPaletteB, 4}
	};
};

} // namespace openmsx

#endif
