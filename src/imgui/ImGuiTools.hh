#ifndef IMGUI_TOOLS_HH
#define IMGUI_TOOLS_HH

#include "ImGuiPart.hh"

namespace openmsx {

class ImGuiManager;

class ImGuiTools final : public ImGuiPart
{
public:
	ImGuiTools(ImGuiManager& manager_)
		: manager(manager_) {}

	void showMenu(MSXMotherBoard* motherBoard) override;

private:
	ImGuiManager& manager;
};

} // namespace openmsx

#endif
