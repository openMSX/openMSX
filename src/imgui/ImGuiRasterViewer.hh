#ifndef IMGUI_RASTER_VIEWER_HH
#define IMGUI_RASTER_VIEWER_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "MemBuffer.hh"

namespace openmsx {

class VDP;

class ImGuiRasterViewer final : public ImGuiPart
{
public:
	ImGuiRasterViewer(ImGuiManager& manager_);

	[[nodiscard]] zstring_view iniName() const override { return "Raster Viewer"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void paintSettings();
	void paintDisplay(VDP* vdp);
	void initPixelBuffer(size_t lines);

public:
	bool show = false;

private:
	// settings
	gl::vec4 gridColor{0.0f, 0.0f, 0.0f, 0.5f}; // RGBA
	gl::vec4 beamColor{1.0f, 0.0f, 0.0f, 0.8f}; // RGBA
	gl::vec4 fadeOutColor{0.0f, 0.0f, 0.0f, 0.75f}; // RGBA
	int zoomSelect = 0; // +1 for actual zoom factor
	bool showGrid = true;
	bool showBeam = true;
	bool showFadeOut = true;

	MemBuffer<uint32_t> pixelBuffer;
	gl::Texture viewTex{gl::Null{}};
	gl::Texture gridTex320{gl::Null{}};
	gl::Texture gridTex640{gl::Null{}};
	gl::Texture checkerTex{gl::Null{}};

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",         &ImGuiRasterViewer::show},
		PersistentElement{"zoom",         &ImGuiRasterViewer::zoomSelect},
		PersistentElement{"grid",         &ImGuiRasterViewer::showGrid},
		PersistentElement{"beam",         &ImGuiRasterViewer::showBeam},
		PersistentElement{"fadeOut",      &ImGuiRasterViewer::showFadeOut},
		PersistentElement{"gridColor",    &ImGuiRasterViewer::gridColor},
		PersistentElement{"beamColor",    &ImGuiRasterViewer::beamColor},
		PersistentElement{"fadeOutColor", &ImGuiRasterViewer::fadeOutColor},
	};
};

} // namespace openmsx

#endif
