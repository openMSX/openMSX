#ifndef IMGUI_SOUND_CHIP_HH
#define IMGUI_SOUND_CHIP_HH

#include "ImGuiReadHandler.hh"

#include <map>
#include <string>

struct ImGuiTextBuffer;

namespace openmsx {

class ImGuiManager;
class MSXMotherBoard;

class ImGuiSoundChip : public ImGuiReadHandler
{
public:
	ImGuiSoundChip(ImGuiManager& manager_)
		: manager(manager_) {}

	void save(ImGuiTextBuffer& buf);
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard& motherBoard);

private:
	void showChipSettings(MSXMotherBoard& motherBoard);
	void showChannelSettings(MSXMotherBoard& motherBoard, const std::string& name, bool* enabled);

private:
	ImGuiManager& manager;
	std::map<std::string, bool> channels;
public:
	bool showSoundChipSettings = false;
};

} // namespace openmsx

#endif
