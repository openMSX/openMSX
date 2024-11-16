#ifndef IMGUI_SPRITE_VIEWER_HH
#define IMGUI_SPRITE_VIEWER_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "gl_vec.hh"

#include <string>

namespace openmsx {

class ImGuiSpriteViewer final : public ImGuiPart
{
public:
	ImGuiSpriteViewer(ImGuiManager& manager_, size_t index);

	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = true;

private:
	std::string title;

	bool overrideAll    = false;
	bool overrideMode   = false;
	bool overrideSize   = false;
	bool overrideMag    = false;
	bool overrideTrans  = false;
	bool overridePat    = false;
	bool overrideAtt    = false;
	bool overrideScroll = false;
	bool overrideLines  = false;
	int manualMode = 1;
	int manualSize = 8;
	int manualMag = 0;
	int manualTransparent = 0;
	int manualPatBase = 0;
	int manualAttBase = 0;
	int manualVerticalScroll = 0;
	int manualLines = 0; // 0->192, 1->212, 2->256
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

	static constexpr auto validSizes = {8, 16};
	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",                &ImGuiSpriteViewer::show},
		PersistentElement{"overrideAll",         &ImGuiSpriteViewer::overrideAll},
		PersistentElement{"overrideMode",        &ImGuiSpriteViewer::overrideMode},
		PersistentElement{"overrideSize",        &ImGuiSpriteViewer::overrideSize},
		PersistentElement{"overrideMag",         &ImGuiSpriteViewer::overrideMag},
		PersistentElement{"overrideTrans",       &ImGuiSpriteViewer::overrideTrans},
		PersistentElement{"overridePat",         &ImGuiSpriteViewer::overridePat},
		PersistentElement{"overrideAtt",         &ImGuiSpriteViewer::overrideAtt},
		PersistentElement{"overrideScroll",      &ImGuiSpriteViewer::overrideScroll},
		PersistentElement{"overrideLines",       &ImGuiSpriteViewer::overrideLines},
		PersistentElementMinMax{"mode",          &ImGuiSpriteViewer::manualMode, 1, 3}, // 1 or 2
		PersistentElementEnum{"size",            &ImGuiSpriteViewer::manualSize, validSizes},
		PersistentElementMax{"mag",              &ImGuiSpriteViewer::manualMag, 2},
		PersistentElementMax{"transparent",      &ImGuiSpriteViewer::manualTransparent, 2},
		PersistentElementMax{"patBase",          &ImGuiSpriteViewer::manualPatBase, 0x20000},
		PersistentElementMax{"attBase",          &ImGuiSpriteViewer::manualAttBase, 0x20000},
		PersistentElementMax{"verticalScroll",   &ImGuiSpriteViewer::manualVerticalScroll, 256},
		PersistentElementMax{"lines",            &ImGuiSpriteViewer::manualLines, 3},
		PersistentElementMax{"zoom",             &ImGuiSpriteViewer::zoom, 8},
		PersistentElement{"showGrid",            &ImGuiSpriteViewer::grid},
		PersistentElementMax{"checkerBoardSize", &ImGuiSpriteViewer::checkerBoardSize, 256},
		PersistentElementMax{"boundingBoxOnAll", &ImGuiSpriteViewer::boundingBoxOnAll, 2},
		PersistentElement{"gridColor",           &ImGuiSpriteViewer::gridColor},
		PersistentElement{"checkerBoardColor1",  &ImGuiSpriteViewer::checkerBoardColor1},
		PersistentElement{"checkerBoardColor2",  &ImGuiSpriteViewer::checkerBoardColor2},
		PersistentElement{"boundingBox",         &ImGuiSpriteViewer::drawBoundingBox},
		PersistentElement{"boundingBoxColor",    &ImGuiSpriteViewer::boundingBoxColor},
		PersistentElement{"spritesPerLineLimit", &ImGuiSpriteViewer::enableLimitPerLine},
		PersistentElement{"stopY",               &ImGuiSpriteViewer::enableStopY}
	};
};

} // namespace openmsx

#endif
