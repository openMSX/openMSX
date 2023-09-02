#include "ImGuiMedia.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "CartridgeSlotManager.hh"
#include "CassettePlayerCLI.hh"
#include "DiskImageCLI.hh"
#include "HardwareConfig.hh"
#include "HD.hh"
#include "IDECDROM.hh"
#include "MSXRomCLI.hh"
#include "Reactor.hh"
#include "RealDrive.hh"
#include "RomInfo.hh"

#include "join.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "view.hh"

#include <CustomFont.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

using namespace std::literals;


namespace openmsx {

void ImGuiMedia::save(ImGuiTextBuffer& buf)
{
	auto saveItem = [&](const MediaItem& item, zstring_view name) {
		if (item.name.empty()) return;
		buf.appendf("%s.name=%s\n", name.c_str(), item.name.c_str());
		for (const auto& patch : item.ipsPatches) {
			buf.appendf("%s.patch=%s\n", name.c_str(), patch.c_str());
		}
		if (item.romType != ROM_UNKNOWN) {
			buf.appendf("%s.romType=%s\n", name.c_str(),
				std::string(RomInfo::romTypeToName(item.romType)).c_str());
		}
	};
	auto saveGroup = [&](const ItemGroup& group, zstring_view name) {
		saveItem(group.edit, name);
		auto recentName = tmpStrCat(name, ".recent");
		for (const auto& item : group.recent) {
			saveItem(item, recentName);
		}
		// don't save patchIndex
	};

	std::string name;
	name = "diska";
	for (const auto& info : diskMediaInfo) {
		saveGroup(info.groups[0], tmpStrCat(name, ".image"));
		saveGroup(info.groups[1], tmpStrCat(name, ".dirAsDsk"));
		// don't save groups[2]
		//if (info.select) buf.appendf("%s.select=%d\n", name.c_str(), info.select);
		if (info.show) buf.appendf("%s.show=1\n", name.c_str());
		name.back()++;
	}

	name = "carta";
	for (const auto& info : cartridgeMediaInfo) {
		saveGroup(info.groups[0], tmpStrCat(name, ".rom"));
		saveGroup(info.groups[1], tmpStrCat(name, ".extension"));
		//if (info.select) buf.appendf("%s.select=%d\n", name.c_str(), info.select);
		if (info.show) buf.appendf("%s.show=1\n", name.c_str());
		name.back()++;
	}

	name = "hda";
	for (const auto& info : hdMediaInfo) {
		saveGroup(info, name);
		name.back()++;
	}

	name = "cda";
	for (const auto& info : cdMediaInfo) {
		saveGroup(info, name);
		name.back()++;
	}

	saveGroup(extensionMediaInfo, "extension");
	saveGroup(cassetteMediaInfo, "cassette");
	saveGroup(laserdiscMediaInfo, "laserdisc");
}

void ImGuiMedia::loadLine(std::string_view name, zstring_view value)
{
	auto get = [&](std::string_view prefix, auto& array) -> std::remove_reference_t<decltype(array[0])>* {
		if ((name.size() >= (prefix.size() + 2)) && name.starts_with(prefix) && (name[prefix.size() + 1] == '.')) {
			char c = name[prefix.size()];
			if (('a' <= c) && (c < char('a' + array.size()))) {
				return &array[c - 'a'];
			}
		}
		return nullptr;
	};
	auto loadItem = [&](MediaItem& item, std::string_view suffix) {
		if (suffix == "name") {
			item.name = value;
		} else if (suffix == "patch") {
			item.ipsPatches.emplace_back(suffix);
		} else if (suffix == "romType") {
			if (auto type = RomInfo::nameToRomType(value); type != ROM_UNKNOWN) {
				item.romType = type;
			}
		}
	};
	auto loadGroup = [&](ItemGroup& group, std::string_view suffix) {
		if (suffix.starts_with("recent.")) {
			if (suffix == "recent.name" && !group.recent.full()) {
				group.recent.push_back(MediaItem{});
			}
			if (!group.recent.empty()) {
				loadItem(group.recent.back(), suffix.substr(7));
			}
		} else {
			loadItem(group.edit, suffix);
		}
	};

	if (auto* disk = get("disk", diskMediaInfo)) {
		auto suffix = name.substr(6);
		if (suffix.starts_with("image.")) {
			loadGroup(disk->groups[0], suffix.substr(6));
		} else if (suffix.starts_with("dirAsDsk.")) {
			loadGroup(disk->groups[1], suffix.substr(9));
		} else if (suffix == "select") {
			if (auto i = StringOp::stringTo<int>(value)) {
				if (SELECT_DISK_IMAGE <= *i && *i <= SELECT_RAMDISK) {
					disk->select = *i;
				}
			}
		} else if (suffix == "show") {
			disk->show = StringOp::stringToBool(value);
		}
	} else if (auto* cart = get("cart", cartridgeMediaInfo)) {
		auto suffix = name.substr(6);
		if (suffix.starts_with("rom.")) {
			loadGroup(cart->groups[0], suffix.substr(4));
		} else if (suffix.starts_with("extension.")) {
			loadGroup(cart->groups[1], suffix.substr(10));
		} else if (suffix == "select") {
			if (auto i = StringOp::stringTo<int>(value)) {
				if (SELECT_ROM_IMAGE <= *i && *i <= SELECT_EXTENSION) {
					cart->select = *i;
				}
			}
		} else if (suffix == "show") {
			cart->show = StringOp::stringToBool(value);
		}
	} else if (auto* hd = get("hd", hdMediaInfo)) {
		loadGroup(*hd, name.substr(4));
	} else if (auto* cd = get("cd", cdMediaInfo)) {
		loadGroup(*cd, name.substr(4));
	} else if (name.starts_with("extension.")) {
		loadGroup(extensionMediaInfo, name.substr(10));
	} else if (name.starts_with("casette.")) {
		loadGroup(cassetteMediaInfo, name.substr(8));
	} else if (name.starts_with("laserdisc.")) {
		loadGroup(laserdiscMediaInfo, name.substr(10));
	}
}

static std::string buildFilter(std::string_view description, std::span<const std::string_view> extensions)
{
	return strCat(
		description, " (",
		join(view::transform(extensions,
		                     [](const auto& ext) { return strCat("*.", ext); }),
		     ' '),
		"){",
		join(view::transform(extensions,
		                     [](const auto& ext) { return strCat('.', ext); }),
		     ','),
		",.gz,.zip}");
}

static std::string diskFilter()
{
	return buildFilter("Disk images", DiskImageCLI::getExtensions());
}

static std::string romFilter()
{
	return buildFilter("ROM images", MSXRomCLI::getExtensions());
}

static std::string hdFilter()
{
	return buildFilter("Hard disk images", std::array{"dsk"sv});
}

static std::string cdFilter()
{
	return buildFilter("CDROM images", std::array{"dsk"sv}); // TODO correct ??
}

static std::string display(const ImGuiMedia::MediaItem& item)
{
	std::string result = item.name;
	if (item.romType != ROM_UNKNOWN) {
		strAppend(result, " (", RomInfo::romTypeToName(item.romType), ')');
	}
	if (auto n = item.ipsPatches.size()) {
		strAppend(result, " (+", n, " patch", (n == 1 ? "" : "es"), ')');
	}
	return result;
}

const std::vector<std::string>& ImGuiMedia::getAvailableExtensions()
{
	if (availableExtensionsCache.empty()) {
		availableExtensionsCache = Reactor::getHwConfigs("extensions");
	}
	return availableExtensionsCache;
}

const TclObject& ImGuiMedia::getExtensionInfo(const std::string& extension)
{
	auto [it, inserted] = extensionInfoCache.try_emplace(extension);
	auto& result = it->second;
	if (inserted) {
		if (auto r = manager.execute(makeTclList("openmsx_info", "extensions", extension))) {
			result = *r;
		}
	}
	return result;
}

void ImGuiMedia::printExtensionInfo(const std::string& extName)
{
	const auto& info = getExtensionInfo(extName);
	im::Table("##extension-info", 2, [&]{
		for (const auto& i : info) {
			ImGui::TableNextColumn();
			im::TextWrapPos(ImGui::GetFontSize() * 35.0f, [&]{
				ImGui::TextUnformatted(i);
			});
		}
	});
}

void ImGuiMedia::extensionTooltip(const std::string& extName)
{
	im::ItemTooltip([&]{
		printExtensionInfo(extName);
	});
}

void ImGuiMedia::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Media", motherBoard != nullptr, [&]{
		auto& interp = manager.getInterpreter();

		enum { NONE, ITEM, SEPARATOR } status = NONE;
		auto endGroup = [&] {
			if (status == ITEM) status = SEPARATOR;
		};
		auto elementInGroup = [&] {
			if (status == SEPARATOR) {
				ImGui::Separator();
			}
			status = ITEM;
		};

		auto showCurrent = [&](TclObject current, std::string_view type) {
			if (current.empty()) {
				ImGui::StrCat("Current: no ", type, " inserted");
			} else {
				ImGui::StrCat("Current: ", current.getString());
			}
			ImGui::Separator();
		};

		auto showRecent = [&](std::string_view mediaName, ItemGroup& group, std::function<void(const std::string&)> toolTip = {}) {
			if (!group.recent.empty()) {
				im::Indent([&] {
					im::Menu(strCat("Recent##", mediaName).c_str(), [&]{
						int count = 0;
						for (const auto& item : group.recent) {
							auto d = strCat(display(item), "##", count++);
							if (ImGui::MenuItem(d.c_str())) {
								group.edit = item;
								insertMedia(mediaName, group);
							}
							if (toolTip) toolTip(item.name);
						}
					});
				});
			}
		};

		// diskX
		auto drivesInUse = RealDrive::getDrivesInUse(*motherBoard);
		for (auto i : xrange(RealDrive::MAX_DRIVES)) {
			if (!(*drivesInUse)[i]) continue;
			elementInGroup();
			auto displayName = strCat("Disk Drive ", char('A' + i));
			ImGui::MenuItem(displayName.c_str(), nullptr, &diskMediaInfo[i].show);
		}
		endGroup();

		// cartA / extX
		auto& slotManager = motherBoard->getSlotManager();
		for (auto i : xrange(CartridgeSlotManager::MAX_SLOTS)) {
			if (!slotManager.slotExists(i)) continue;
			elementInGroup();
			auto displayName = strCat("Cartridge Slot ", char('A' + i));
			ImGui::MenuItem(displayName.c_str(), nullptr, &cartridgeMediaInfo[i].show);
		}
		endGroup();

		// extensions (needed for I/O-only extensions, or when you don't care about the exact slot)
		elementInGroup();
		im::Menu("Extensions", [&]{
			auto mediaName = "ext"sv;
			auto& group = extensionMediaInfo;
			im::Menu("Insert", [&]{
				for (const auto& ext : getAvailableExtensions()) {
					if (ImGui::MenuItem(ext.c_str())) {
						group.edit.name = ext;
						insertMedia(mediaName, group);
					}
					extensionTooltip(ext);
				}
			});

			showRecent(mediaName, group, [this](const std::string& e) { extensionTooltip(e); });

			ImGui::Separator();

			const auto& extensions = motherBoard->getExtensions();
			im::Disabled(extensions.empty(), [&]{
				im::Menu("Remove", [&]{
					for (const auto& ext : extensions) {
						if (ImGui::Selectable(ext->getName().c_str())) {
							manager.executeDelayed(makeTclList("remove_extension", ext->getName()));
						}
						extensionTooltip(ext->getConfigName());
					}
				});
			});
		});
		endGroup();

		// cassetteplayer
		if (auto cmdResult = manager.execute(TclObject("cassetteplayer"))) {
			elementInGroup();
			im::Menu("Tape Deck", [&]{
				auto currentImage = cmdResult->getListIndex(interp, 1);
				showCurrent(currentImage, "cassette");
				if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
					manager.executeDelayed(makeTclList("cassetteplayer", "eject"));
				}
				if (ImGui::MenuItem("Insert cassette image...")) {
					manager.openFile.selectFile(
						"Select cassette image",
						buildFilter("Cassette images", CassettePlayerCLI::getExtensions()),
						[this](const auto& fn) {
							cassetteMediaInfo.edit.name = fn;
							this->insertMedia("cassetteplayer", cassetteMediaInfo);
						},
						currentImage.getString());
				}
				showRecent("cassetteplayer", cassetteMediaInfo);
			});
		}
		endGroup();

		// hdX
		auto hdInUse = HD::getDrivesInUse(*motherBoard);
		std::string hdName = "hdX";
		for (auto i : xrange(HD::MAX_HD)) {
			if (!(*hdInUse)[i]) continue;
			hdName.back() = char('a' + i);
			auto displayName = strCat("Hard Disk ", char('A' + i));
			if (auto cmdResult = manager.execute(TclObject(hdName))) {
				elementInGroup();
				auto& group = hdMediaInfo[i];
				im::Menu(displayName.c_str(), [&]{
					auto currentImage = cmdResult->getListIndex(interp, 1);
					showCurrent(currentImage, "hard disk");
					bool powered = motherBoard->isPowered();
					im::Disabled(powered, [&]{
						if (ImGui::MenuItem("Select hard disk image...")) {
							manager.openFile.selectFile(
								"Select image for " + displayName,
								hdFilter(),
								[this, &group, hdName](const auto& fn) {
									group.edit.name = fn;
									this->insertMedia(hdName, group);
								},
								currentImage.getString());
						}
					});
					if (powered) {
						HelpMarker("Hard disk image cannot be switched while the MSX is powered on.");
					}
					im::Disabled(powered, [&]{
						showRecent(hdName, group);
					});
				});
			}
		}
		endGroup();

		// cdX
		auto cdInUse = IDECDROM::getDrivesInUse(*motherBoard);
		std::string cdName = "cdX";
		for (auto i : xrange(IDECDROM::MAX_CD)) {
			if (!(*cdInUse)[i]) continue;
			cdName.back() = char('a' + i);
			auto displayName = strCat("CDROM Drive ", char('A' + i));
			if (auto cmdResult = manager.execute(TclObject(cdName))) {
				elementInGroup();
				auto& group = cdMediaInfo[i];
				im::Menu(displayName.c_str(), [&]{
					auto currentImage = cmdResult->getListIndex(interp, 1);
					showCurrent(currentImage, "CDROM");
					if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
						manager.executeDelayed(makeTclList(cdName, "eject"));
					}
					if (ImGui::MenuItem("Insert CDROM image...")) {
						manager.openFile.selectFile(
							"Select CDROM image for " + displayName,
							cdFilter(),
							[this, &group, cdName](const auto& fn) {
								group.edit.name = fn;
								this->insertMedia(cdName, group);
							},
							currentImage.getString());
					}
					showRecent(cdName, group);
				});
			}
		}
		endGroup();

		// laserdisc
		if (auto cmdResult = manager.execute(TclObject("laserdiscplayer"))) {
			elementInGroup();
			im::Menu("LaserDisc Player", [&]{
				auto currentImage = cmdResult->getListIndex(interp, 1);
				showCurrent(currentImage, "laserdisc");
				if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
					manager.executeDelayed(makeTclList("laserdiscplayer", "eject"));
				}
				if (ImGui::MenuItem("Insert LaserDisc image...")) {
					manager.openFile.selectFile(
						"Select LaserDisc image",
						buildFilter("LaserDisc images", std::array<std::string_view, 1>{"ogv"}),
						[this](const auto& fn) {
							laserdiscMediaInfo.edit.name = fn;
							this->insertMedia("laserdiscplayer", laserdiscMediaInfo);
						},
						currentImage.getString());
				}
				showRecent("laserdiscplayer", laserdiscMediaInfo);
			});
		}
	});
}

void ImGuiMedia::paint(MSXMotherBoard* motherBoard)
{
	if (!motherBoard) return;

	auto drivesInUse = RealDrive::getDrivesInUse(*motherBoard);
	for (auto i : xrange(RealDrive::MAX_DRIVES)) {
		if (!(*drivesInUse)[i]) continue;
		if (diskMediaInfo[i].show) {
			diskMenu(i);
		}
	}

	auto& slotManager = motherBoard->getSlotManager();
	for (auto i : xrange(CartridgeSlotManager::MAX_SLOTS)) {
		if (!slotManager.slotExists(i)) continue;
		if (cartridgeMediaInfo[i].show) {
			cartridgeMenu(i);
		}
	}
}

static TclObject getPatches(const TclObject& cmdResult)
{
	return cmdResult.getOptionalDictValue(TclObject("patches")).value_or(TclObject{});
}

static void printPatches(const TclObject& patches)
{
	if (!patches.empty()) {
		ImGui::TextUnformatted("IPS patches:");
		im::Indent([&]{
			for (const auto& patch : patches) {
				ImGui::TextUnformatted(patch);
			}
		});
	}
}

bool ImGuiMedia::selectRecent(ItemGroup& group)
{
	ImGui::SetNextItemWidth(-52.0f);
	bool interacted = ImGui::InputText("##image", &group.edit.name);
	ImGui::SameLine(0.0f, 0.0f);
	im::Combo("##recent", "", ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft, [&]{
		int count = 0;
		for (auto& item : group.recent) {
			auto d = strCat(display(item), "##", count++);
			if (ImGui::Selectable(d.c_str())) {
				group.edit = item;
				interacted = true;
			}
		}
	});
	interacted |= ImGui::IsItemActive();
	return interacted;
}

bool ImGuiMedia::selectImage(ItemGroup& group, const std::string& title,
                             std::function<std::string()> createFilter, zstring_view current)
{
	bool interacted = false;
	im::ID("file", [&]{
		interacted |= selectRecent(group);
	});
	ImGui::SameLine();
	if (ImGui::Button(ICON_IGFD_FOLDER_OPEN"##file")) {
		interacted = true;
		manager.openFile.selectFile(
			title,
			createFilter(),
			[&](const auto& fn) { group.edit.name = fn; },
			current);
	}
	simpleToolTip("browse file");
	return interacted;
}

bool ImGuiMedia::selectDirectory(ItemGroup& group, const std::string& title, zstring_view current)
{
	bool interacted = false;
	im::ID("directory", [&]{
		interacted |= selectRecent(group);
	});
	ImGui::SameLine();
	if (ImGui::Button(ICON_IGFD_FOLDER_OPEN"##dir")) {
		interacted = true;
		manager.openFile.selectDirectory(
			title,
			[&](const auto& fn) { group.edit.name = fn; },
			current);
	}
	simpleToolTip("browse directory");
	return interacted;
}

bool ImGuiMedia::selectMapperType(MediaItem& item)
{
	bool interacted = false;
	bool isAutoDetect = item.romType == ROM_UNKNOWN;
	constexpr const char* autoStr = "auto detect";
	std::string current = isAutoDetect ? autoStr : std::string(RomInfo::romTypeToName(item.romType));
	ImGui::SetNextItemWidth(-80.0f);
	im::Combo("mapper-type", current.c_str(), [&]{
		if (ImGui::Selectable(autoStr, isAutoDetect)) {
			interacted = true;
			item.romType = ROM_UNKNOWN;
		}
		int count = 0;
		for (const auto& romInfo : RomInfo::getRomTypeInfo()) {
			bool selected = item.romType == static_cast<RomType>(count);
			if (ImGui::Selectable(std::string(romInfo.name).c_str(), selected)) {
				interacted = true;
				item.romType = static_cast<RomType>(count);
			}
			simpleToolTip(romInfo.description);
			++count;
		}
	});
	interacted |= ImGui::IsItemActive();
	return interacted;
}

bool ImGuiMedia::selectPatches(MediaItem& item, int& patchIndex)
{
	bool interacted = false;
	std::string patchesTitle = "IPS patches";
	if (!item.ipsPatches.empty()) {
		strAppend(patchesTitle, " (", item.ipsPatches.size(), ')');
	}
	strAppend(patchesTitle, "###patches");
	im::TreeNode(patchesTitle.c_str(), [&]{
		ImGui::SetNextItemWidth(-60.0f);
		im::Group([&]{
			im::ListBox("##", [&]{
				int count = 0;
				for (const auto& patch : item.ipsPatches) {
					if (ImGui::Selectable(patch.c_str(), count == patchIndex)) {
						interacted = true;
						patchIndex = count;
					}
					++count;
				}
			});
		});
		ImGui::SameLine();
		im::Group([&]{
			if (ImGui::Button("Add")) {
				interacted = true;
				manager.openFile.selectFile(
					"Select disk IPS patch",
					buildFilter("IPS patches", std::array<std::string_view, 1>{"ips"}),
					[&](const std::string& ips) { item.ipsPatches.push_back(ips); });
			}
			im::Disabled(patchIndex < 0 ||  patchIndex >= narrow<int>(item.ipsPatches.size()), [&] {
				if (ImGui::Button("Remove")) {
					interacted = true;
					item.ipsPatches.erase(item.ipsPatches.begin() + patchIndex);
				}
			});
		});
	});
	return interacted;
}

void ImGuiMedia::insertMediaButton(std::string_view mediaName, ItemGroup& group, bool* showWindow)
{
	im::Disabled(group.edit.name.empty(), [&]{
		const auto& style = ImGui::GetStyle();
		auto width = 4.0f * style.FramePadding.x + style.ItemSpacing.x +
			     ImGui::CalcTextSize("Apply").x + ImGui::CalcTextSize("Ok").x;
		ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - width + style.WindowPadding.x);
		if (ImGui::Button("Apply")) {
			insertMedia(mediaName, group);
		}
		ImGui::SameLine();
		if (ImGui::Button("Ok")) {
			insertMedia(mediaName, group);
			*showWindow = false;
		}
	});
}

TclObject ImGuiMedia::showDiskInfo(std::string_view mediaName, DiskMediaInfo& info)
{
	TclObject currentTarget;
	auto cmdResult = manager.execute(makeTclList("machine_info", "media", mediaName));
	if (!cmdResult) return currentTarget;

	int selectType = [&]{
		auto type = cmdResult->getOptionalDictValue(TclObject("type"));
		assert(type);
		auto s = type->getString();
		if (s == "empty") {
			return SELECT_EMPTY_DISK;
		} else if (s == "ramdsk") {
			return SELECT_RAMDISK;
		} else if (s == "dirasdisk") {
			return SELECT_DIR_AS_DISK;
		} else {
			assert(s == "file");
			return SELECT_DISK_IMAGE;
		}
	}();
	std::string_view typeStr = [&]{
		switch (selectType) {
			case SELECT_EMPTY_DISK:  return "No disk inserted";
			case SELECT_RAMDISK:     return "RAM disk";
			case SELECT_DIR_AS_DISK: return "Dir as disk:";
			case SELECT_DISK_IMAGE:  return "Disk image:";
			default: UNREACHABLE;
		}
	}();
	bool disableEject = selectType == SELECT_EMPTY_DISK;
	bool detailedInfo = selectType == one_of(SELECT_DIR_AS_DISK, SELECT_DISK_IMAGE);
	auto currentPatches = getPatches(*cmdResult);

	bool copyCurrent = false;
	im::Disabled(disableEject, [&]{
		copyCurrent = ImGui::SmallButton("Current disk");
		HelpMarker("Press to copy current disk to 'Select new disk' section.");
	});

	im::Indent([&]{
		ImGui::TextUnformatted(typeStr);
		if (detailedInfo) {
			if (auto target = cmdResult->getOptionalDictValue(TclObject("target"))) {
				currentTarget = *target;
				ImGui::SameLine();
				ImGui::TextUnformatted(currentTarget.getString());
			}
			if (auto ro = cmdResult->getOptionalDictValue(TclObject("readonly"))) {
				if (auto b = ro->getOptionalBool(); b && *b) {
					ImGui::TextUnformatted("Read-only");
				}
			}
			printPatches(currentPatches);
		}
	});
	if (copyCurrent && selectType != SELECT_EMPTY_DISK) {
		info.select = selectType;
		auto& edit = info.groups[selectType].edit;
		edit.name = currentTarget.getString();
		edit.ipsPatches = to_vector<std::string>(currentPatches);
	}
	im::Disabled(disableEject, [&]{
		if (ImGui::Button("Eject")) {
			manager.executeDelayed(makeTclList(mediaName, "eject"));
		}
	});
	ImGui::Separator();
	return currentTarget;
}

TclObject ImGuiMedia::showRomInfo(std::string_view mediaName, CartridgeMediaInfo& info, int slot)
{
	TclObject currentTarget;
	auto cmdResult = manager.execute(makeTclList("machine_info", "media", mediaName));
	if (!cmdResult) return currentTarget;

	int selectType = [&]{
		if (auto type = cmdResult->getOptionalDictValue(TclObject("type"))) {
			auto s = type->getString();
			if (s == "extension") {
				return SELECT_EXTENSION;
			} else {
				assert(s == "rom");
				return SELECT_ROM_IMAGE;
			}
		} else {
			return SELECT_EMPTY_SLOT;
		}
	}();
	std::string_view typeStr = [&]{
		switch (selectType) {
			case SELECT_ROM_IMAGE:  return "ROM:";
			case SELECT_EXTENSION:  return "Extension:";
			case SELECT_EMPTY_SLOT: return "No cartridge inserted";
			default: UNREACHABLE;
		}
	}();
	bool disableEject = selectType == SELECT_EMPTY_SLOT;
	auto currentPatches = getPatches(*cmdResult);

	bool copyCurrent = false;
	im::Disabled(disableEject, [&]{
		copyCurrent = ImGui::SmallButton("Current cartridge");
	});
	auto& slotManager = manager.getReactor().getMotherBoard()->getSlotManager();
	auto [ps, ss] = slotManager.getPsSs(slot);
	std::string slotStr = strCat("(slot ", ps);
	if (ss != -1) strAppend(slotStr, '-', ss);
	strAppend(slotStr, ')');
	ImGui::SameLine();
	ImGui::TextUnformatted(slotStr);

	RomType currentRomType = ROM_UNKNOWN;
	im::Indent([&]{
		ImGui::TextUnformatted(typeStr);
		if (auto target = cmdResult->getOptionalDictValue(TclObject("target"))) {
			currentTarget = *target;
			if (selectType == SELECT_EXTENSION) {
				printExtensionInfo(std::string(target->getString()));
			} else if (selectType == SELECT_ROM_IMAGE) {
				ImGui::SameLine();
				ImGui::TextUnformatted(target->getString());
				if (auto mapper = cmdResult->getOptionalDictValue(TclObject("mappertype"))) {
					ImGui::StrCat("Mapper-type: ", mapper->getString());
					currentRomType = RomInfo::nameToRomType(mapper->getString());
				}
				printPatches(currentPatches);
			}
		}
	});
	if (copyCurrent && selectType != SELECT_EMPTY_SLOT) {
		info.select = selectType;
		auto& edit = info.groups[selectType].edit;
		edit.name = currentTarget.getString();
		edit.ipsPatches = to_vector<std::string>(currentPatches);
		edit.romType = currentRomType;
	}
	im::Disabled(disableEject, [&]{
		if (ImGui::Button("Eject")) {
			manager.executeDelayed(makeTclList(mediaName, "eject"));
		}
	});
	ImGui::Separator();
	return currentTarget;
}

void ImGuiMedia::diskMenu(int i)
{
	auto& info = diskMediaInfo[i];
	auto mediaName = strCat("disk", char('a' + i));
	auto displayName = strCat("Disk Drive ", char('A' + i));
	ImGui::SetNextWindowSize(gl::vec2{29, 22} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window(displayName.c_str(), &info.show, [&]{
		auto current = showDiskInfo(mediaName, info);
		im::Child("select", {0, -ImGui::GetFrameHeightWithSpacing()}, [&]{
			ImGui::TextUnformatted("Select new disk"sv);

			ImGui::RadioButton("disk image", &info.select, SELECT_DISK_IMAGE);
			im::VisuallyDisabled(info.select != SELECT_DISK_IMAGE, [&]{
				im::Indent([&]{
					auto& group = info.groups[SELECT_DISK_IMAGE];
					bool interacted = selectImage(group, strCat("Select disk image for ", displayName), &diskFilter, current.getString());
					interacted |= selectPatches(group.edit, group.patchIndex);
					if (interacted) info.select = SELECT_DISK_IMAGE;
				});
			});
			ImGui::RadioButton("dir as disk", &info.select, SELECT_DIR_AS_DISK);
			im::VisuallyDisabled(info.select != SELECT_DIR_AS_DISK, [&]{
				im::Indent([&]{
					bool interacted = selectDirectory(info.groups[SELECT_DIR_AS_DISK], strCat("Select directory for ", displayName), current.getString());
					if (interacted) info.select = SELECT_DIR_AS_DISK;
				});
			});
			ImGui::RadioButton("RAM disk", &info.select, SELECT_RAMDISK);
		});
		insertMediaButton(mediaName, info.groups[info.select], &info.show);
	});
}

void ImGuiMedia::cartridgeMenu(int i)
{
	auto& info = cartridgeMediaInfo[i];
	auto displayName = strCat("Cartridge Slot ", char('A' + i));
	ImGui::SetNextWindowSize(gl::vec2{29, 22} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window(displayName.c_str(), &info.show, [&]{
		auto cartName = strCat("cart", char('a' + i));
		auto extName = strCat("ext", char('a' + i));

		auto current = showRomInfo(cartName, info, i);

		im::Child("select", {0, -ImGui::GetFrameHeightWithSpacing()}, [&]{
			ImGui::TextUnformatted("Select new cartridge:"sv);

			ImGui::RadioButton("ROM image", &info.select, SELECT_ROM_IMAGE);
			im::VisuallyDisabled(info.select != SELECT_ROM_IMAGE, [&]{
				im::Indent([&]{
					auto& group = info.groups[SELECT_ROM_IMAGE];
					auto& item = group.edit;
					bool interacted = selectImage(group, strCat("Select ROM image for ", displayName), &romFilter, current.getString());
					interacted |= selectMapperType(item);
					interacted |= selectPatches(item, group.patchIndex);
					if (interacted) info.select = SELECT_ROM_IMAGE;
				});
			});
			ImGui::RadioButton("extension", &info.select, SELECT_EXTENSION);
			im::VisuallyDisabled(info.select != SELECT_EXTENSION, [&]{
				im::Indent([&]{
					auto& group = info.groups[SELECT_EXTENSION];
					auto& item = group.edit;
					bool interacted = false;
					im::Combo("##extension", item.name.c_str(), [&]{
						for (const auto& name : getAvailableExtensions()) {
							if (ImGui::Selectable(name.c_str())) {
								interacted = true;
								item.name = name;
							}
							extensionTooltip(name);
						}
					});
					interacted |= ImGui::IsItemActive();
					if (interacted) info.select = SELECT_EXTENSION;
				});
			});
		});
		insertMediaButton(info.select == SELECT_ROM_IMAGE ? cartName : extName,
		                  info.groups[info.select], &info.show);
	});
}

static void addRecent(ImGuiMedia::ItemGroup& group)
{
	auto& recent = group.recent;
	if (auto it2 = ranges::find(recent, group.edit); it2 != recent.end()) {
		// was already present, move to front
		std::rotate(recent.begin(), it2, it2 + 1);
	} else {
		// new entry, add it, but possibly remove oldest entry
		if (recent.full()) recent.pop_back();
		recent.push_front(group.edit);
	}
}

void ImGuiMedia::insertMedia(std::string_view mediaName, ItemGroup& group)
{
	auto& item = group.edit;
	if (item.name.empty()) return;

	auto cmd = makeTclList(mediaName, "insert", item.name);
	for (const auto& patch : item.ipsPatches) {
		cmd.addListElement("-ips", patch);
	}
	if (item.romType != ROM_UNKNOWN) {
		cmd.addListElement("-romtype", RomInfo::romTypeToName(item.romType));
	}
	manager.executeDelayed(cmd,
		[&group](const TclObject&) {
			// only add to 'recent' when insert command succeeded
			addRecent(group);
		});
}

} // namespace openmsx
