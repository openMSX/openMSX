#ifndef IMGUI_KEYBOARD_HH
#define IMGUI_KEYBOARD_HH

#include "ImGuiPart.hh"

#include <string>
#include <vector>

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
	// The labels of the keys are calculated once per machine (they come
	// from its unicodemap, which doesn't change during its lifetime).
	std::string labelsMachineID;
	std::vector<std::string> labels; // in the order of the key table

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiKeyboard::show}
	};
};

} // namespace openmsx

#endif
