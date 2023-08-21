#ifndef IMGUI_MEDIA_HH
#define IMGUI_MEDIA_HH

#include "ImGuiPart.hh"

#include "RomTypes.hh"
#include "TclObject.hh"

#include "circular_buffer.hh"
#include "zstring_view.hh"

#include <functional>
#include <map>
#include <set>
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
		MediaInfo(std::string_view name_)
			: name(name_), recent(HISTORY_SIZE) {}

		const std::string name; // diska/carta/...
		circular_buffer<RecentItem> recent;

		std::string imageName; // for the advanced menu
		std::vector<std::string> ipsPatches; // only disk and cart
		RomType romType = ROM_UNKNOWN; // only for cart, 'ROM_UNKNOWN' means 'auto'

		bool showAdvanced = false;

		friend auto operator<(const MediaInfo& x, const MediaInfo& y) {
			return x.name < y.name;
		}
		friend auto operator<(const MediaInfo& x, std::string_view y) {
			return std::string_view(x.name) < y;
		}
	};
	MediaInfo& getInfo(std::string_view media);

private:
	TclObject showDiskInfo(MediaInfo& info);
	TclObject showRomInfo (MediaInfo& info);
	void selectImage(MediaInfo& info, const std::string& title,
	                 std::function<std::string()> createFilter, zstring_view current);
	void selectMapperType(MediaInfo& info);
	void selectPatches(MediaInfo& info);
	void insertMediaButton(MediaInfo& info, zstring_view title);
	void advancedDiskMenu(MediaInfo& info);
	void advancedRomMenu (MediaInfo& cartInfo, MediaInfo& extInfo);
	void insertMedia(MediaInfo& info,
	                 const std::string& filename,
	                 std::span<const std::string> patches = {},
	                 RomType romType = ROM_UNKNOWN);
	void addRecent(MediaInfo& info, const std::string& filename, std::span<const std::string> patches = {},
	               RomType romType = ROM_UNKNOWN);
	const std::vector<std::string>& getAvailableExtensions();
	const TclObject& getExtensionInfo(const std::string& extension);
	void extensionTooltip(const std::string& extName);

private:
	ImGuiManager& manager;

	std::set<MediaInfo, std::less<>> mediaStuff; // values need stable address
	std::vector<std::string> availableExtensionsCache;
	std::map<std::string, TclObject> extensionInfoCache;
	int patchIndex = -1;
};

} // namespace openmsx

#endif
