#ifndef IMGUI_PLOTTER_VIEWER_HH
#define IMGUI_PLOTTER_VIEWER_HH

#include "ImGuiPart.hh"
#include "GLUtil.hh"
#include <optional>

namespace openmsx {

class ImGuiPlotterViewer final : public ImGuiPart
{
public:
	explicit ImGuiPlotterViewer(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "Plotter viewer"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

	bool show = false;

private:
	std::optional<gl::ColorTexture> texture;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiPlotterViewer::show},
	};
};

} // namespace openmsx

#endif
