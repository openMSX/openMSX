#ifndef IMGUI_MEDIA_HH
#define IMGUI_MEDIA_HH

#include "ImGuiPart.hh"

#include "RomTypes.hh"

#include "circular_buffer.hh"
#include "zstring_view.hh"

#include <functional>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace openmsx {

class ImGuiManager;
class MSXMotherBoard;

class ImGuiMedia final : public ImGuiPart
{
public:
	struct RecentItem {
		std::string filename;
		std::vector<std::string> ipsPatches; // used for disk<x> and cart<x>
		RomType romType = ROM_UNKNOWN;

		auto operator<=>(const RecentItem&) const = default;
	};

public:
	ImGuiMedia(ImGuiManager& manager_)
		: manager(manager_) {}

	[[nodiscard]] zstring_view iniName() const override { return "media"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadStart() override { mediaStuff.clear(); }
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	struct MediaInfo {
		static constexpr size_t HISTORY_SIZE = 8;
		MediaInfo() : recent(HISTORY_SIZE) {}

		circular_buffer<RecentItem> recent;

		std::string imageName; // for the advanced menu
		std::vector<std::string> ipsPatches; // only disk and cart
		RomType romType = ROM_UNKNOWN; // only for cart, 'ROM_UNKNOWN' means 'auto'

		bool showAdvanced = false;
	};

private:
	void showDiskInfo(const std::string& media, MediaInfo& info);
	void showRomInfo (const std::string& media, MediaInfo& info);
	void selectImage(MediaInfo& info, const std::string& title, std::function<std::string()> createFilter);
	void selectMapperType(MediaInfo& info);
	void selectPatches(MediaInfo& info);
	void insertMediaButton(MediaInfo& info, const std::string& media, zstring_view title);
	void advancedDiskMenu(const std::string& media, MediaInfo& info);
	void advancedRomMenu (const std::string& media, MediaInfo& info);
	void insertMedia(const std::string& media, MediaInfo& info,
	                 const std::string& filename,
	                 std::span<const std::string> patches = {},
	                 RomType romType = ROM_UNKNOWN);
	void addRecent(MediaInfo& info, const std::string& filename, std::span<const std::string> patches,
	               RomType romType);

private:
	ImGuiManager& manager;

	std::map<std::string, MediaInfo> mediaStuff; // values need stable address
	int patchIndex = -1;
};

} // namespace openmsx

#endif
