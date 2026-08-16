#ifndef IMGUI_SLOTMAP_HH
#define IMGUI_SLOTMAP_HH

#include "ImGuiPart.hh"

namespace openmsx {

class ImGuiSlotMap final : public ImGuiPart
{
public:
	using ImGuiPart::ImGuiPart;

	[[nodiscard]] zstring_view iniName() const override { return "slot-map"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	bool vertical = true; // pages as rows, like the memory maps in most manuals
	bool fitToContent = false; // resize the window after switching layout

	// 'vertical' is saved separately, as a "layout=vertical|horizontal" line.
	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiSlotMap::show}
	};
};

} // namespace openmsx

#endif
