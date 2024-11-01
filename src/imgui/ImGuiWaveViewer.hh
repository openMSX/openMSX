#ifndef IMGUI_WAVEVIEWER_HH
#define IMGUI_WAVEVIEWER_HH

#include "ImGuiPart.hh"

namespace openmsx {

class ImGuiWaveViewer final : public ImGuiPart
{
public:
	using ImGuiPart::ImGuiPart;

	[[nodiscard]] zstring_view iniName() const override { return "wave-viewer"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiWaveViewer::show}
	};
};

} // namespace openmsx

#endif
