#ifndef IMGUI_SMS_VDP_SPRITES_HH
#define IMGUI_SMS_VDP_SPRITES_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "gl_vec.hh"

#include <array>
#include <cstdint>
#include <string>

namespace openmsx {

/** Sprite viewer for the Sega VDP inside the Franky cartridge.
  *
  * This is the Franky counterpart of ImGuiSpriteViewer. It supports both
  * the TMS9918-compatible sprites (modes 0-3) and the SMS mode 4 sprites.
  */
class ImGuiSMSVDPSprites final : public ImGuiPart
{
public:
	ImGuiSMSVDPSprites(ImGuiManager& manager_, size_t index);

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
	bool renderAfterStopY = false; // rendered sprites: also show sprites after Y=0xD0
	gl::Texture patternTex {gl::Null{}}; // TODO also deallocate when needed
	gl::Texture gridTex    {gl::Null{}};
	gl::Texture zoomGridTex{gl::Null{}};
	gl::Texture checkerTex {gl::Null{}};
	gl::Texture renderTex  {gl::Null{}};
	gl::vecN<2, int> gridPosition;

	static constexpr auto validSizes = {8, 16};
	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",                &ImGuiSMSVDPSprites::show},
		PersistentElement{"overrideAll",         &ImGuiSMSVDPSprites::overrideAll},
		PersistentElement{"overrideType",        &ImGuiSMSVDPSprites::overrideType},
		PersistentElement{"overrideSize",        &ImGuiSMSVDPSprites::overrideSize},
		PersistentElement{"overrideMag",         &ImGuiSMSVDPSprites::overrideMag},
		PersistentElement{"overrideTrans",       &ImGuiSMSVDPSprites::overrideTrans},
		PersistentElement{"overridePat",         &ImGuiSMSVDPSprites::overridePat},
		PersistentElement{"overrideAtt",         &ImGuiSMSVDPSprites::overrideAtt},
		PersistentElement{"overrideLines",       &ImGuiSMSVDPSprites::overrideLines},
		PersistentElementMinMax{"type",          &ImGuiSMSVDPSprites::manualType, 1, 3}, // 1 or SMS
		PersistentElementEnum{"size",            &ImGuiSMSVDPSprites::manualSize, validSizes},
		PersistentElementMax{"mag",              &ImGuiSMSVDPSprites::manualMag, 2},
		PersistentElementMax{"transparent",      &ImGuiSMSVDPSprites::manualTransparent, 2},
		PersistentElementMax{"patBase",          &ImGuiSMSVDPSprites::manualPatBase, 0x4000},
		PersistentElementMax{"attBase",          &ImGuiSMSVDPSprites::manualAttBase, 0x4000},
		PersistentElementMax{"lines",            &ImGuiSMSVDPSprites::manualLines, 3}, // 192, 224 or 240
		PersistentElementMax{"zoom",             &ImGuiSMSVDPSprites::zoom, 8},
		PersistentElement{"showGrid",            &ImGuiSMSVDPSprites::grid},
		PersistentElementMax{"checkerBoardSize", &ImGuiSMSVDPSprites::checkerBoardSize, maxCheckerBoardSize + 1},
		PersistentElementMax{"boundingBoxOnAll", &ImGuiSMSVDPSprites::boundingBoxOnAll, 2},
		PersistentElement{"gridColor",           &ImGuiSMSVDPSprites::gridColor},
		PersistentElement{"checkerBoardColor1",  &ImGuiSMSVDPSprites::checkerBoardColor1},
		PersistentElement{"checkerBoardColor2",  &ImGuiSMSVDPSprites::checkerBoardColor2},
		PersistentElement{"boundingBox",         &ImGuiSMSVDPSprites::drawBoundingBox},
		PersistentElement{"boundingBoxColor",    &ImGuiSMSVDPSprites::boundingBoxColor},
		PersistentElement{"spritesPerLineLimit", &ImGuiSMSVDPSprites::enableLimitPerLine},
		PersistentElement{"renderAfterStopY",    &ImGuiSMSVDPSprites::renderAfterStopY}
	};
};

} // namespace openmsx

#endif
