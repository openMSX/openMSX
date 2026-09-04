#ifndef IMGUI_IOPORTS_HH
#define IMGUI_IOPORTS_HH

#include "ImGuiPart.hh"

namespace openmsx {

class ImGuiIoPorts final : public ImGuiPart
{
public:
	using ImGuiPart::ImGuiPart;

	// Note: not "ioports", that's already taken by the hex editor on the
	// 'ioports' debuggable (DebuggableEditor::iniName() returns its title).
	[[nodiscard]] zstring_view iniName() const override { return "io ports"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiIoPorts::show}
	};
};

} // namespace openmsx

#endif
