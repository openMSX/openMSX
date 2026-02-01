#ifndef IMGUI_RASTER_VIEWER_HH
#define IMGUI_RASTER_VIEWER_HH

#include "ImGuiPart.hh"

#include "VDP.hh"

#include "GLUtil.hh"
#include "MemBuffer.hh"

#include <span>

namespace openmsx {

class ImGuiTraceViewer;

class ImGuiRasterViewer final : public ImGuiPart
{
public:
	ImGuiRasterViewer(ImGuiManager& manager, ImGuiTraceViewer& traceViewer);

	[[nodiscard]] zstring_view iniName() const override { return "Raster Viewer"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void paintSettings();
	void paintDisplay(MSXMotherBoard& motherBoard);
	void paintConfigure(MSXMotherBoard& motherBoard);
	void initPixelBuffer(size_t lines);
	gl::vec2 drawCrosshair(
		gl::ivec2 vdpPos, gl::vec4 color, gl::vec2 scrnPos, int zoom,
		std::span<const unsigned, VDP::NUM_LINES_MAX> lineWidths);
	void drawMarker(
		gl::ivec2 vdpPos, gl::vec4 color, gl::vec2 scrnPos, int zoom,
		std::span<const unsigned, VDP::NUM_LINES_MAX> lineWidths);
	void drawRegion(
		gl::ivec2 from, gl::ivec2 to, gl::vec4 color, gl::vec2 scrnTopLeft, int zoom,
		std::span<const unsigned> lineWidths);

	struct TraceInfo {
		TraceInfo(std::string name_, gl::vec4 color_, bool enabled_)
			: name(std::move(name_)), color(color_), enabled(enabled_) {}
		std::string name;
		gl::vec4 color;
		bool enabled = true;
	};
	TraceInfo& getTraceInfoFor(std::string_view name);

public:
	bool show = false;

private:
	ImGuiTraceViewer& traceViewer;
	std::vector<TraceInfo> traceInfos; // sorted on name

	// settings
	gl::vec4 gridColor{0.0f, 0.0f, 0.0f, 0.5f}; // RGBA
	gl::vec4 fadeOutColor{0.0f, 0.0f, 0.0f, 0.75f}; // RGBA
	gl::vec4 beamColor{1.0f, 0.0f, 0.0f, 0.8f}; // RGBA
	gl::vec4 vblankIrqColor{1.0f, 1.0f, 0.0f, 0.8f}; // RGBA
	gl::vec4 lineIrqColor{0.0f, 1.0f, 1.0f, 0.8f}; // RGBA
	gl::vec4 markerColor1{0.0f, 1.0f, 0.0f, 0.8f}; // RGBA
	gl::vec4 markerColor2{1.0f, 0.0f, 0.0f, 0.8f}; // RGBA
	gl::vec4 shadeColor{1.0f, 1.0f, 0.0f, 0.3f}; // RGBA
	std::string shadeTrace;
	enum Shade : int { DISABLED, MARKERS, TRACE };
	int shadeType = Shade::MARKERS;
	int zoomSelect = 0; // +1 for actual zoom factor
	bool showGrid = true;
	bool showFadeOut = true;
	bool showBeam = true;
	bool showVblankIrq = false;
	bool showLineIrq = false;
	bool showTraces = false;
	bool showMarkers = true;
	bool showConfigure = false;

	MemBuffer<uint32_t> pixelBuffer;
	gl::Texture viewTex{gl::Null{}};
	gl::Texture gridTex320{gl::Null{}};
	gl::Texture gridTex640{gl::Null{}};
	gl::Texture checkerTex{gl::Null{}};

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",           &ImGuiRasterViewer::show},
		PersistentElement{"zoom",           &ImGuiRasterViewer::zoomSelect},
		PersistentElement{"grid",           &ImGuiRasterViewer::showGrid},
		PersistentElement{"fadeOut",        &ImGuiRasterViewer::showFadeOut},
		PersistentElement{"beam",           &ImGuiRasterViewer::showBeam},
		PersistentElement{"vblankIrq",      &ImGuiRasterViewer::showVblankIrq},
		PersistentElement{"lineIrq",        &ImGuiRasterViewer::showLineIrq},
		PersistentElement{"traces",         &ImGuiRasterViewer::showTraces},
		PersistentElement{"markers",        &ImGuiRasterViewer::showMarkers},
		PersistentElement{"shadeTrace",     &ImGuiRasterViewer::shadeTrace},
		PersistentElement{"shadeType",      &ImGuiRasterViewer::shadeType},
		PersistentElement{"gridColor",      &ImGuiRasterViewer::gridColor},
		PersistentElement{"fadeOutColor",   &ImGuiRasterViewer::fadeOutColor},
		PersistentElement{"beamColor",      &ImGuiRasterViewer::beamColor},
		PersistentElement{"vblankIrqColor", &ImGuiRasterViewer::vblankIrqColor},
		PersistentElement{"lineIrqColor",   &ImGuiRasterViewer::lineIrqColor},
		PersistentElement{"marker1Color",   &ImGuiRasterViewer::markerColor1},
		PersistentElement{"marker2Color",   &ImGuiRasterViewer::markerColor2},
		PersistentElement{"shadeColor",     &ImGuiRasterViewer::shadeColor},
	};
};

} // namespace openmsx

#endif
