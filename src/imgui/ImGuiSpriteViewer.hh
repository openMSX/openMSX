#ifndef IMGUI_SPRITE_VIEWER_HH
#define IMGUI_SPRITE_VIEWER_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "gl_vec.hh"

namespace openmsx {

class ImGuiManager;

class ImGuiSpriteViewer final : public ImGuiPart
{
public:
	ImGuiSpriteViewer(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "sprite viewer"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	ImGuiManager& manager;

	int manual = 0; // 0 -> use VDP settings, 1 -> use manual settings
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
};

} // namespace openmsx

#endif
