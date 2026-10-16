#ifndef IMGUI_FRANKY_SPRITES_HH
#define IMGUI_FRANKY_SPRITES_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "gl_vec.hh"

#include <string>

namespace openmsx {

/** Sprite viewer for the Sega VDP inside the Franky cartridge.
  *
  * This is the Franky counterpart of ImGuiSpriteViewer. It supports both
  * the TMS9918-compatible sprites (modes 0-3) and the SMS mode 4 sprites.
  */
class ImGuiFrankySprites final : public ImGuiPart
{
public:
	ImGuiFrankySprites(ImGuiManager& manager_, size_t index);

	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = true;

private:
	std::string title;

	// Sprite type: none (text mode), 1 (TMS9918, modes 0-3) or SMS (mode 4).
	enum SpriteType : int { NONE = 0, TMS = 1, SMS = 2 };

	bool overrideAll    = false;
	bool overrideType   = false;
	bool overrideSize   = false;
	bool overrideMag    = false;
	bool overrideTrans  = false;
	bool overridePat    = false;
	bool overrideAtt    = false;
	bool overrideLines  = false;
	int manualType = 1;
	int manualSize = 8;
	int manualMag = 0;
	int manualTransparent = 1;
	int manualPatBase = 0;
	int manualAttBase = 0;
	int manualLines = 0; // 0->192, 1->224, 2->240
	int zoom = 0; // 0->1x, 1->2x, ..., 7->8x
	int checkerBoardSize = 4;
	int boundingBoxOnAll = 0;
	gl::vec4 gridColor{0.5f, 0.5f, 0.5f, 0.5f}; // RGBA
	gl::vec4 checkerBoardColor1{0.2f, 0.2f, 0.2f, 0.8f}; // RGBA
	gl::vec4 checkerBoardColor2{0.4f, 0.4f, 0.4f, 0.8f}; // RGBA
	gl::vec4 boundingBoxColor  {1.0f, 0.0f, 0.0f, 0.8f}; // RGBA
	bool grid = true;
	bool drawBoundingBox = true;
	bool enableLimitPerLine = true;
	bool enableStopY = true;

	gl::Texture patternTex {gl::Null{}}; // TODO also deallocate when needed
	gl::Texture gridTex    {gl::Null{}};
	gl::Texture zoomGridTex{gl::Null{}};
	gl::Texture checkerTex {gl::Null{}};
	gl::Texture renderTex  {gl::Null{}};
	gl::vecN<2, int> gridPosition;

	static constexpr auto validSizes = {8, 16};
	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",                &ImGuiFrankySprites::show},
		PersistentElement{"overrideAll",         &ImGuiFrankySprites::overrideAll},
		PersistentElement{"overrideType",        &ImGuiFrankySprites::overrideType},
		PersistentElement{"overrideSize",        &ImGuiFrankySprites::overrideSize},
		PersistentElement{"overrideMag",         &ImGuiFrankySprites::overrideMag},
		PersistentElement{"overrideTrans",       &ImGuiFrankySprites::overrideTrans},
		PersistentElement{"overridePat",         &ImGuiFrankySprites::overridePat},
		PersistentElement{"overrideAtt",         &ImGuiFrankySprites::overrideAtt},
		PersistentElement{"overrideLines",       &ImGuiFrankySprites::overrideLines},
		PersistentElementMinMax{"type",          &ImGuiFrankySprites::manualType, 1, 2}, // 1 or SMS
		PersistentElementEnum{"size",            &ImGuiFrankySprites::manualSize, validSizes},
		PersistentElementMax{"mag",              &ImGuiFrankySprites::manualMag, 2},
		PersistentElementMax{"transparent",      &ImGuiFrankySprites::manualTransparent, 2},
		PersistentElementMax{"patBase",          &ImGuiFrankySprites::manualPatBase, 0x4000},
		PersistentElementMax{"attBase",          &ImGuiFrankySprites::manualAttBase, 0x4000},
		PersistentElementMax{"lines",            &ImGuiFrankySprites::manualLines, 2},
		PersistentElementMax{"zoom",             &ImGuiFrankySprites::zoom, 8},
		PersistentElement{"showGrid",            &ImGuiFrankySprites::grid},
		PersistentElementMax{"checkerBoardSize", &ImGuiFrankySprites::checkerBoardSize, 256},
		PersistentElementMax{"boundingBoxOnAll", &ImGuiFrankySprites::boundingBoxOnAll, 2},
		PersistentElement{"gridColor",           &ImGuiFrankySprites::gridColor},
		PersistentElement{"checkerBoardColor1",  &ImGuiFrankySprites::checkerBoardColor1},
		PersistentElement{"checkerBoardColor2",  &ImGuiFrankySprites::checkerBoardColor2},
		PersistentElement{"boundingBox",         &ImGuiFrankySprites::drawBoundingBox},
		PersistentElement{"boundingBoxColor",    &ImGuiFrankySprites::boundingBoxColor},
		PersistentElement{"spritesPerLineLimit", &ImGuiFrankySprites::enableLimitPerLine},
		PersistentElement{"stopY",               &ImGuiFrankySprites::enableStopY}
	};
};

} // namespace openmsx

#endif
