#ifndef IMGUI_KEYBOARD_HH
#define IMGUI_KEYBOARD_HH

#include "ImGuiPart.hh"

namespace openmsx {

class ImGuiKeyboard final : public ImGuiPart
{
public:
	using ImGuiPart::ImGuiPart;

	[[nodiscard]] zstring_view iniName() const override { return "virtual keyboard"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiKeyboard::show}
	};
};

} // namespace openmsx

#endif
