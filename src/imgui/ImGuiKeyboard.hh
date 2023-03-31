#ifndef IMGUI_KEYBOARD_HH
#define IMGUI_KEYBOARD_HH

#include "ImGuiReadHandler.hh"

struct ImGuiTextBuffer;

namespace openmsx {

class ImGuiManager;
class MSXMotherBoard;

class ImGuiKeyboard : public ImGuiReadHandler
{
public:
	ImGuiKeyboard(ImGuiManager& manager_)
		: manager(manager_) {}

	void save(ImGuiTextBuffer& buf);
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard& motherBoard);

public:
	bool show = false;

private:
	ImGuiManager& manager;
};

} // namespace openmsx

#endif
