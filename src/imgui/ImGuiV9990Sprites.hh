#ifndef IMGUI_V9990_SPRITES_HH
#define IMGUI_V9990_SPRITES_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "gl_vec.hh"

#include <string>

namespace openmsx {

/** Sprite viewer for the V9990 (GFX9000) pattern modes P1 and P2.
  *
  * This is the V9990 counterpart of ImGuiSpriteViewer. It shows the sprite
  * pattern table, the sprite attribute table and the rendered sprites.
  */
class ImGuiV9990Sprites final : public ImGuiPart
{
public:
	ImGuiV9990Sprites(ImGuiManager& manager_, size_t index);

	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void updateGridTex(gl::Texture& tex, int w, int h);
	void updateCheckerTex();

public:
	bool show = true;

private:
	std::string title;

	bool overrideAll  = false;
	bool overrideMode = false;
	bool overridePat  = false;
	int manualMode = 0; // 0->P1, 1->P2
	int manualPatBase = 0;
	int patternPalette = 0; // 0-3, palette used to show the sprite pattern table
	int zoom = 0; // single zoom for all sections (0->1x, ..., 7->8x)
	static constexpr int maxCheckerBoardSize = 255;
	int checkerBoardSize = 4;
	int boundingBoxOnAll = 0;
	gl::vec4 gridColor{0.5f, 0.5f, 0.5f, 0.5f}; // RGBA
	static constexpr gl::vec4 defaultCheckerBoardColor1{0.2f, 0.2f, 0.2f, 0.8f};
	static constexpr gl::vec4 defaultCheckerBoardColor2{0.4f, 0.4f, 0.4f, 0.8f};
	gl::vec4 checkerBoardColor1{defaultCheckerBoardColor1}; // RGBA
	gl::vec4 checkerBoardColor2{defaultCheckerBoardColor2}; // RGBA
	gl::vec4 boundingBoxColor  {1.0f, 0.0f, 0.0f, 0.8f}; // RGBA
	bool grid = true;
	bool drawBoundingBox = true;
	bool enableLimitPerLine = true;

	// The hovered pattern/sprite, remembered for the copy popups.
	int hoveredPattern = 0;
	int hoveredSprite = 0;

	gl::Texture patternTex {gl::Null{}}; // TODO also deallocate when needed
	gl::Texture attribTex  {gl::Null{}};
	gl::Texture gridTex    {gl::Null{}};
	gl::Texture zoomGridTex{gl::Null{}};
	gl::Texture checkerTex {gl::Null{}};
	gl::Texture renderTex  {gl::Null{}};

	static constexpr auto persistentElements = std::tuple{
		PersistentElement   {"show",                &ImGuiV9990Sprites::show},
		PersistentElement   {"overrideAll",         &ImGuiV9990Sprites::overrideAll},
		PersistentElement   {"overrideMode",        &ImGuiV9990Sprites::overrideMode},
		PersistentElement   {"overridePat",         &ImGuiV9990Sprites::overridePat},
		PersistentElementMax{"mode",                &ImGuiV9990Sprites::manualMode, 2}, // P1 or P2
		PersistentElementMax{"patBase",             &ImGuiV9990Sprites::manualPatBase, 0x80000},
		PersistentElementMax{"patternPalette",      &ImGuiV9990Sprites::patternPalette, 4},
		PersistentElementMax{"zoom",                &ImGuiV9990Sprites::zoom, 8},
		PersistentElement   {"showGrid",            &ImGuiV9990Sprites::grid},
		PersistentElementMax{"checkerBoardSize",    &ImGuiV9990Sprites::checkerBoardSize, maxCheckerBoardSize + 1},
		PersistentElementMax{"boundingBoxOnAll",    &ImGuiV9990Sprites::boundingBoxOnAll, 2},
		PersistentElement   {"gridColor",           &ImGuiV9990Sprites::gridColor},
		PersistentElement   {"checkerBoardColor1",  &ImGuiV9990Sprites::checkerBoardColor1},
		PersistentElement   {"checkerBoardColor2",  &ImGuiV9990Sprites::checkerBoardColor2},
		PersistentElement   {"boundingBox",         &ImGuiV9990Sprites::drawBoundingBox},
		PersistentElement   {"boundingBoxColor",    &ImGuiV9990Sprites::boundingBoxColor},
		PersistentElement   {"spritesPerLineLimit", &ImGuiV9990Sprites::enableLimitPerLine}
	};
};

} // namespace openmsx

#endif
