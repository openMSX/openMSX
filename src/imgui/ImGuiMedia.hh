#ifndef IMGUI_MEDIA_HH
#define IMGUI_MEDIA_HH

#include "ImGuiPart.hh"

#include "circular_buffer.hh"

#include <map>

namespace openmsx {

class ImGuiManager;
class MSXMotherBoard;

class ImGuiMedia final : public ImGuiPart
{
public:
	ImGuiMedia(ImGuiManager& manager_)
		: manager(manager_) {}

	[[nodiscard]] zstring_view iniName() const override { return "media"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;

private:
	void insertMedia(const std::string& media, const std::string& filename);
	void addRecent(const std::string& media, const std::string& filename);

private:
	ImGuiManager& manager;

	std::map<std::string, circular_buffer<std::string>> recentMedia; // TODO load/save
};

} // namespace openmsx

#endif
