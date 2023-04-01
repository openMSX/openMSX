#ifndef IMGUI_MEDIA_HH
#define IMGUI_MEDIA_HH

#include "ImGuiPart.hh"

namespace openmsx {

class ImGuiManager;
class MSXMotherBoard;

class ImGuiMedia final : public ImGuiPart
{
public:
	ImGuiMedia(ImGuiManager& manager_)
		: manager(manager_) {}

	void showMenu(MSXMotherBoard* motherBoard) override;

private:
	ImGuiManager& manager;
};

} // namespace openmsx

#endif
