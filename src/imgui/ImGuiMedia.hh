#ifndef IMGUI_MEDIA_HH
#define IMGUI_MEDIA_HH

#include "ImGuiPart.hh"

#include "CartridgeSlotManager.hh"
#include "HD.hh"
#include "IDECDROM.hh"
#include "RealDrive.hh"
#include "RomTypes.hh"
#include "TclObject.hh"

#include "circular_buffer.hh"
#include "function_ref.hh"
#include "stl.hh"
#include "zstring_view.hh"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace openmsx {

class CassettePlayer;
class HardwareConfig;
class RomInfo;

class ImGuiMedia final : public ImGuiPart
{
public:
	struct ExtensionInfo {
		std::string configName;
		std::string displayName;
		std::vector<std::pair<std::string, std::string>> configInfo;
		std::optional<std::string> testResult; // lazily initialized
	};

public:
	using ImGuiPart::ImGuiPart;

	[[nodiscard]] zstring_view iniName() const override { return "media"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

	[[nodiscard]] std::string displayNameForExtension(std::string_view config);
	[[nodiscard]] std::string displayNameForRom(const std::string& filename, bool compact = false);
	[[nodiscard]] std::string displayNameForHardwareConfig(const HardwareConfig& config, bool compact = false);
	[[nodiscard]] std::string displayNameForSlotContent(const CartridgeSlotManager& slotManager, unsigned slotNr, bool compact = false);
	[[nodiscard]] std::string slotAndNameForHardwareConfig(const CartridgeSlotManager& slotManager, const HardwareConfig& config);
	[[nodiscard]] std::string displayNameForDriveContent(unsigned drive, bool compact = false);

	std::vector<ExtensionInfo>& getAllExtensions();
	void resetExtensionInfo();
	ExtensionInfo* findExtensionInfo(std::string_view config);
	[[nodiscard]] const std::string& getTestResult(ExtensionInfo& info);

	void addRecent(const TclObject& cmd);

public:
	enum class SelectDiskType : int {
		IMAGE, DIR_AS_DISK, RAMDISK, EMPTY,
		NUM
	};
	enum class SelectCartridgeType : int {
		IMAGE, EXTENSION, EMPTY,
		NUM
	};

	struct MediaItem {
		std::string name;
		std::vector<std::string> ipsPatches; // only used for disk and rom images
		RomType romType = RomType::UNKNOWN; // only used for rom images

		[[nodiscard]] bool isEject() const { return romType == RomType::NUM; } // hack
		[[nodiscard]] bool operator==(const MediaItem&) const = default;
	};

	struct ItemGroup { // MediaItem + history
		static constexpr size_t HISTORY_SIZE = 8;

		MediaItem edit;
		int patchIndex = -1; // only used for IPSItem
		circular_buffer<MediaItem> recent{HISTORY_SIZE};
	};

	struct CartridgeMediaInfo {
		CartridgeMediaInfo() {
			groups[SelectCartridgeType::EMPTY].edit.romType = RomType::NUM; // hack: indicates "eject"
		}
		array_with_enum_index<SelectCartridgeType, ItemGroup> groups;
		SelectCartridgeType select = SelectCartridgeType::IMAGE;
		bool show = false;
	};
	struct DiskMediaInfo {
		DiskMediaInfo() {
			groups[SelectDiskType::RAMDISK].edit.name = "ramdsk";
			groups[SelectDiskType::EMPTY].edit.romType = RomType::NUM; // hack: indicates "eject"
		}
		array_with_enum_index<SelectDiskType, ItemGroup> groups;
		SelectDiskType select = SelectDiskType::IMAGE;
		bool show = false;
	};
	struct CassetteMediaInfo {
		ItemGroup group;
		bool show = false;
	};

public:
	bool resetOnCartChanges = true;

	static void printDatabase(const RomInfo& romInfo, const char* buf);
	static bool selectMapperType(const char* label, RomType& item);

	static std::string diskFilter();

private:
	bool selectRecent(ItemGroup& group, function_ref<std::string(const std::string&)> displayFunc, float width) const;
	bool selectImage(ItemGroup& group, const std::string& title,
	                 function_ref<std::string()> createFilter, zstring_view current,
	                 function_ref<std::string(const std::string&)> displayFunc = std::identity{},
	                 const std::function<void()>& createNewCallback = {});
	bool selectDirectory(ItemGroup& info, const std::string& title, zstring_view current,
	                     const std::function<void()>& createNewCallback);
	bool selectPatches(MediaItem& item, int& patchIndex);
	bool insertMediaButton(std::string_view mediaName, const ItemGroup& group, bool* showWindow);
	TclObject showDiskInfo(std::string_view mediaName, DiskMediaInfo& info);
	TclObject showCartridgeInfo(std::string_view mediaName, CartridgeMediaInfo& info, int slot);
	void diskMenu(int i);
	void cartridgeMenu(int i);
	void cassetteMenu(CassettePlayer& cassettePlayer);
	void insertMedia(std::string_view mediaName, const MediaItem& item);

	void printExtensionInfo(ExtensionInfo& info);
	void extensionTooltip(ExtensionInfo& info);
	bool drawExtensionFilter();

private:
	std::array<DiskMediaInfo, RealDrive::MAX_DRIVES> diskMediaInfo;
	std::array<CartridgeMediaInfo, CartridgeSlotManager::MAX_SLOTS> cartridgeMediaInfo;
	ItemGroup extensionMediaInfo;
	CassetteMediaInfo cassetteMediaInfo;
	std::array<ItemGroup, HD::MAX_HD> hdMediaInfo;
	std::array<ItemGroup, IDECDROM::MAX_CD> cdMediaInfo;
	ItemGroup laserdiscMediaInfo;

	std::string filterType;
	std::string filterString;
	bool filterOpen = false;

	std::vector<ExtensionInfo> extensionInfo;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"resetOnCartChanges", &ImGuiMedia::resetOnCartChanges},
		// most media stuff is handled elsewhere
	};
};

} // namespace openmsx

#endif
