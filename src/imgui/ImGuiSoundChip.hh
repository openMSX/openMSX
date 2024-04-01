#ifndef IMGUI_SOUND_CHIP_HH
#define IMGUI_SOUND_CHIP_HH

#include "ImGuiPart.hh"

#include <map>
#include <string>

namespace openmsx {

class ImGuiSoundChip final : public ImGuiPart
{
public:
	explicit ImGuiSoundChip(ImGuiManager& manager_)
		: ImGuiPart(manager_) {}

	[[nodiscard]] zstring_view iniName() const override { return "sound chip settings"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void showChipSettings(MSXMotherBoard& motherBoard);
	void showChannelSettings(MSXMotherBoard& motherBoard, const std::string& name, bool* enabled);

private:
	std::map<std::string, bool, std::less<>> channels;
public:
	bool showSoundChipSettings = false;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiSoundChip::showSoundChipSettings}
		// manually handle "showChannels.xxx"
	};
};

} // namespace openmsx

#endif
