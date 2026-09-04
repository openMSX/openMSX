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
	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiSlotMap::show}
	};
};

} // namespace openmsx

#endif
