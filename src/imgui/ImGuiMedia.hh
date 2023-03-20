#ifndef IMGUI_MEDIA_HH
#define IMGUI_MEDIA_HH

namespace openmsx {

class ImGuiManager;
class MSXMotherBoard;

class ImGuiMedia
{
public:
	ImGuiMedia(ImGuiManager& manager_)
		: manager(manager_) {}

	void showMenu(MSXMotherBoard* motherBoard);

private:
	ImGuiManager& manager;
};

} // namespace openmsx

#endif
