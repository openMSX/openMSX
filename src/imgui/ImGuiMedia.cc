#include "ImGuiMedia.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "CartridgeSlotManager.hh"
#include "CassettePlayerCLI.hh"
#include "DiskImageCLI.hh"
#include "MSXRomCLI.hh"
#include "RealDrive.hh"
#include "RomInfo.hh"

#include "join.hh"
#include "ranges.hh"
#include "view.hh"

#include <CustomFont.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

using namespace std::literals;


namespace openmsx {

void ImGuiMedia::save(ImGuiTextBuffer& buf)
{
	for (const auto& [media, info] : mediaStuff) {
		for (const auto& item : info.recent) {
			buf.appendf("recent.%s=%s\n", media.c_str(), item.filename.c_str());
			for (const auto& patch : item.ipsPatches) {
				buf.appendf("patch.%s=%s\n", media.c_str(), patch.c_str());
			}
			if (item.romType != ROM_UNKNOWN) {
				buf.appendf("romType.%s=%s\n", media.c_str(),
				            std::string(RomInfo::romTypeToName(item.romType)).c_str());
			}
		}
		buf.appendf("show.%s=%d\n", media.c_str(), info.showAdvanced);
	}
}

void ImGuiMedia::loadLine(std::string_view name, zstring_view value)
{
	auto getInfo = [&](size_t prefixLen) -> MediaInfo& {
		std::string media(name.substr(prefixLen));
		auto [it, inserted] = mediaStuff.try_emplace(media);
		return it->second;
	};

	if (name.starts_with("recent.")) {
		if (auto& history = getInfo(strlen("recent.")).recent; !history.full()) {
			RecentItem item;
			item.filename = value;
			// no ips patches (yet)
			history.push_back(std::move(item));
		}
	}
	if (name.starts_with("patch.")) {
		if (auto& history = getInfo(strlen("patch.")).recent; !history.empty()) {
			history.back().ipsPatches.emplace_back(value);
		}
	}
	if (name.starts_with("romType.")) {
		if (auto& history = getInfo(strlen("romType.")).recent; !history.empty()) {
			history.back().romType = RomInfo::nameToRomType(value);
		}
	}
	if (name.starts_with("show.")) {
		auto& info = getInfo(strlen("show."));
		info.showAdvanced = StringOp::stringToBool(value);
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

static std::string display(const ImGuiMedia::RecentItem& item, int count)
{
	std::string result = item.filename;
	if (item.romType != ROM_UNKNOWN) {
		strAppend(result, " (", RomInfo::romTypeToName(item.romType), ')');
	}
	if (auto n = item.ipsPatches.size()) {
		strAppend(result, " (+", n, " patch", (n == 1 ? "" : "es"), ')');
	}
	strAppend(result, "##", count);
	return result;
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

		auto showRecent = [&](const std::string& media, MediaInfo& info) {
			if (!info.recent.empty()) {
				im::Indent([&] {
					im::Menu("Recent", [&]{
						int count = 0;
						for (const auto& item : info.recent) {
							if (ImGui::MenuItem(display(item, count).c_str())) {
								insertMedia(media, info, item.filename, item.ipsPatches, item.romType);
							}
							++count;
						}
					});
				});
			}
		};

		// diskX
		auto drivesInUse = RealDrive::getDrivesInUse(*motherBoard);
		std::string driveName = "diskX";
		for (auto i : xrange(RealDrive::MAX_DRIVES)) {
			if (!(*drivesInUse)[i]) continue;
			driveName[4] = char('a' + i);
			const std::string displayDriveName = std::string("Disk Drive ") + char(::toupper(driveName[4]));
			if (auto cmdResult = manager.execute(TclObject(driveName))) {
				elementInGroup();
				auto& info = mediaStuff.try_emplace(driveName).first->second;
				im::Menu(displayDriveName.c_str(), [&]{
					auto currentImage = cmdResult->getListIndex(interp, 1);
					showCurrent(currentImage, "disk");
					if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
						manager.executeDelayed(makeTclList(driveName, "eject"));
					}
					if (ImGui::MenuItem("Insert disk image...")) {
						manager.openFile.selectFile(
							"Select disk image for " + displayDriveName,
							diskFilter(),
							[this, driveName, &info](const auto& fn) { this->insertMedia(driveName, info, fn); });
					}
					showRecent(driveName, info);
					ImGui::MenuItem("Insert disk window (advanced)", nullptr, &info.showAdvanced);
				});
			}
		}
		endGroup();

		// cartA / extX TODO
		auto& slotManager = motherBoard->getSlotManager();
		std::string cartName = "cartX";
		for (auto slot : xrange(CartridgeSlotManager::MAX_SLOTS)) {
			if (!slotManager.slotExists(slot)) continue;
			cartName[4] = char('a' + slot);
			const std::string displaySlotName = std::string("Cartridge Slot ") + char(::toupper(cartName[4]));
			if (auto cmdResult = manager.execute(TclObject(cartName))) {
				elementInGroup();
				im::Menu(displaySlotName.c_str(), [&]{
					auto currentImage = cmdResult->getListIndex(interp, 1);
					showCurrent(currentImage, "cart"); // TODO cart/ext
					if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
						manager.executeDelayed(makeTclList(cartName, "eject"));
					}
					auto& info = mediaStuff.try_emplace(cartName).first->second;
					if (ImGui::MenuItem("Select ROM file...")) {
						manager.openFile.selectFile(
							"Select ROM image for " + displaySlotName,
							romFilter(),
							[this, cartName, &info](const auto& fn) { this->insertMedia(cartName, info, fn); });
					}
					showRecent(cartName, info);
					ImGui::MenuItem("Insert ROM window (advanced)", nullptr, &info.showAdvanced);
					ImGui::Separator();

					if (ImGui::MenuItem("insert extension TODO")) {
						ImGui::TextUnformatted("TODO"sv);
					}
				});
			}
		}
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
				auto& info = mediaStuff.try_emplace("cassetteplayer").first->second;
				if (ImGui::MenuItem("Insert cassette image...")) {
					manager.openFile.selectFile(
						"Select cassette image",
						buildFilter("Cassette images", CassettePlayerCLI::getExtensions()),
						[this, &info](const auto& fn) { this->insertMedia("cassetteplayer", info, fn); });
				}
				showRecent("cassetteplayer", info);
			});
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
				auto& info = mediaStuff.try_emplace("laserdiscplayer").first->second;
				if (ImGui::MenuItem("Insert LaserDisc image...")) {
					manager.openFile.selectFile(
						"Select LaserDisc image",
						buildFilter("LaserDisc images", std::array<std::string_view, 1>{"ogv"}),
						[this, &info](const auto& fn) { this->insertMedia("laserdiscplayer", info, fn); });
				}
				showRecent("laserdiscplayer", info);
			});
		}
	});
}

void ImGuiMedia::paint(MSXMotherBoard* motherBoard)
{
	if (!motherBoard) return;

	auto drivesInUse = RealDrive::getDrivesInUse(*motherBoard);
	std::string media = "diskX";
	for (auto i : xrange(RealDrive::MAX_DRIVES)) {
		if (!(*drivesInUse)[i]) continue;
		media[4] = char('a' + i);
		auto& info = mediaStuff.try_emplace(media).first->second;
		if (info.showAdvanced) {
			advancedDiskMenu(media, info);
		}
	}

	auto& slotManager = motherBoard->getSlotManager();
	media = "cartX";
	for (auto slot : xrange(CartridgeSlotManager::MAX_SLOTS)) {
		if (!slotManager.slotExists(slot)) continue;
		media[4] = char('a' + slot);
		auto& info = mediaStuff.try_emplace(media).first->second;
		if (info.showAdvanced) {
			advancedRomMenu(media, info);
		}
	}
}

static void getAndPrintPatches(const TclObject& cmdResult, TclObject& result)
{
	if (auto patches = cmdResult.getOptionalDictValue(TclObject("patches"));
	    patches && !patches->empty()) {
		result = *patches;
		ImGui::TextUnformatted("IPS patches:");
		im::Indent([&]{
			for (const auto& patch : result) {
				ImGui::TextUnformatted(patch);
			}
		});
	}
}

void ImGuiMedia::selectImage(MediaInfo& info, const std::string& title, std::function<std::string()> createFilter)
{
	ImGui::InputText("##image", &info.imageName);
	ImGui::SameLine(0.0f, 0.0f);
	im::Combo("##recent", "", ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft, [&]{
		int count = 0;
		for (auto& item : info.recent) {
			if (ImGui::Selectable(display(item, count).c_str())) {
				info.imageName = item.filename;
				info.ipsPatches = item.ipsPatches;
				info.romType = item.romType;
			}
			++count;
		}
	});
	ImGui::SameLine();
	if (ImGui::Button(ICON_IGFD_FOLDER_OPEN)) {
		manager.openFile.selectFile(
			title,
			createFilter(),
			[&](const auto& fn) { info.imageName = fn; });
	}
	simpleToolTip("browse");
}

void ImGuiMedia::selectMapperType(MediaInfo& info)
{
	bool isAutoDetect = info.romType == ROM_UNKNOWN;
	constexpr const char* autoStr = "auto detect";
	std::string current = isAutoDetect ? autoStr : std::string(RomInfo::romTypeToName(info.romType));
	ImGui::SetNextItemWidth(-80.0f);
	im::Combo("mapper-type", current.c_str(), [&]{
		if (ImGui::Selectable(autoStr, isAutoDetect)) {
			info.romType = ROM_UNKNOWN;
		}
		int count = 0;
		for (const auto& romInfo : RomInfo::getRomTypeInfo()) {
			bool selected = info.romType == static_cast<RomType>(count);
			if (ImGui::Selectable(std::string(romInfo.name).c_str(), selected)) {
				info.romType = static_cast<RomType>(count);
			}
			simpleToolTip(romInfo.description);
			++count;
		}
	});
}

void ImGuiMedia::selectPatches(MediaInfo& info)
{
	std::string patchesTitle = "IPS patches";
	if (!info.ipsPatches.empty()) {
		strAppend(patchesTitle, " (", info.ipsPatches.size(), ')');
	}
	strAppend(patchesTitle, "###patches");
	im::TreeNode(patchesTitle.c_str(), [&]{
		ImGui::SetNextItemWidth(-60.0f);
		im::Group([&]{
			im::ListBox("##", [&]{
				int count = 0;
				for (const auto& patch : info.ipsPatches) {
					if (ImGui::Selectable(patch.c_str(), count == patchIndex)) {
						patchIndex = count;
					}
					++count;
				}
			});
		});
		ImGui::SameLine();
		im::Group([&]{
			if (ImGui::Button("Add")) {
				manager.openFile.selectFile(
					"Select disk IPS patch",
					buildFilter("IPS patches", std::array<std::string_view, 1>{"ips"}),
					[&](const std::string& ips) { info.ipsPatches.push_back(ips); });
			}
			im::Disabled(patchIndex < 0 ||  patchIndex >= narrow<int>(info.ipsPatches.size()), [&] {
				if (ImGui::Button("Remove")) {
					info.ipsPatches.erase(info.ipsPatches.begin() + patchIndex);
				}
			});
		});
	});
}

void ImGuiMedia::insertMediaButton(MediaInfo& info, const std::string& media, zstring_view title)
{
	im::Disabled(info.imageName.empty(), [&]{
		if (ImGui::Button(title.c_str())) {
			insertMedia(media, info, info.imageName, info.ipsPatches, info.romType);
		}
	});
}

void ImGuiMedia::showDiskInfo(const std::string& media, MediaInfo& info)
{
	auto cmdResult = manager.execute(makeTclList("machine_info", "media", media));
	if (!cmdResult) return;

	bool copyCurrent = ImGui::SmallButton("Current disk:");
	TclObject currentTarget;
	TclObject currentPatches;
	bool showEject = true;
	im::Indent([&]{
		bool skip = false;
		if (auto type = cmdResult->getOptionalDictValue(TclObject("type"))) {
			auto s = type->getString();
			if (s == "empty") {
				ImGui::TextUnformatted("No disk inserted");
				skip = true;
				showEject = false;
			} else if (s == "ramdisk") {
				ImGui::TextUnformatted("RAM disk");
				skip = true;
			} else if (s == "dirasdisk") {
				ImGui::TextUnformatted("Dir as disk:");
			} else {
				assert(s == "file");
				ImGui::TextUnformatted("Disk image:");
			}
		}
		if (!skip) {
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
			getAndPrintPatches(*cmdResult, currentPatches);
		}
	});
	if (copyCurrent) {
		info.imageName = currentTarget.getString();
		info.ipsPatches = to_vector<std::string>(currentPatches);
	}
	if (showEject && ImGui::Button("Eject")) {
		manager.executeDelayed(makeTclList(media, "eject"));
	}
	ImGui::Separator();
}

void ImGuiMedia::showRomInfo(const std::string& media, MediaInfo& info)
{
	auto cmdResult = manager.execute(makeTclList("machine_info", "media", media));
	if (!cmdResult) return;

	bool copyCurrent = ImGui::SmallButton("Current cartridge:");
	TclObject currentTarget;
	TclObject currentPatches;
	RomType currentRomType = ROM_UNKNOWN;
	bool showEject = true;
	im::Indent([&]{
		if (auto type = cmdResult->getOptionalDictValue(TclObject("type"))) {
			auto s = type->getString();
			if (s == "extension") {
				ImGui::TextUnformatted("Extension:");
				if (auto target = cmdResult->getOptionalDictValue(TclObject("target"))) {
					ImGui::TextUnformatted(target->getString());
					if (auto device = cmdResult->getOptionalDictValue(TclObject("devicename"))) {
						ImGui::TextUnformatted(device->getString());
					}
				}
			} else {
				assert(s == "rom");
				ImGui::TextUnformatted("ROM:");
				if (auto target = cmdResult->getOptionalDictValue(TclObject("target"))) {
					currentTarget = *target;
					ImGui::SameLine();
					ImGui::TextUnformatted(target->getString());
					if (auto mapper = cmdResult->getOptionalDictValue(TclObject("mappertype"))) {
						ImGui::StrCat("Mapper-type: ", mapper->getString());
						currentRomType = RomInfo::nameToRomType(mapper->getString());
					}
					getAndPrintPatches(*cmdResult, currentPatches);
				}
			}
		} else {
			ImGui::TextUnformatted("No cartridge inserted");
			showEject = false;
		}
	});
	if (copyCurrent) {
		info.imageName = currentTarget.getString();
		info.ipsPatches = to_vector<std::string>(currentPatches);
		info.romType = currentRomType;
	}
	if (showEject && ImGui::Button("Eject")) {
		manager.executeDelayed(makeTclList(media, "eject"));
	}
	ImGui::Separator();
}

void ImGuiMedia::advancedDiskMenu(const std::string& media, MediaInfo& info)
{
	const std::string displayDriveName = std::string("Disk Drive ") + char(::toupper(media[4]));
	im::Window(displayDriveName.c_str(), &info.showAdvanced, [&]{
		showDiskInfo(media, info);
		ImGui::TextUnformatted("Select new disk"sv);
		HelpMarker("Type-in or browse-to the disk image. Or type the special name 'ramdisk'.");
		im::Indent([&]{
			selectImage(info, strCat("Select disk image for ", displayDriveName), &diskFilter);
			selectPatches(info);

		});
		insertMediaButton(info, media, "Insert selected disk");
	});
}

void ImGuiMedia::advancedRomMenu(const std::string& media, MediaInfo& info)
{
	const std::string displaySlotName = std::string("Cartridge Slot ") + char(::toupper(media[4]));
	im::Window(displaySlotName.c_str(), &info.showAdvanced, [&]{
		showRomInfo(media, info);
		ImGui::TextUnformatted("Select new ROM:"sv);
		im::Indent([&]{
			selectImage(info, strCat("Select ROM image for ", displaySlotName), &romFilter);
			selectMapperType(info);
			selectPatches(info);
		});
		insertMediaButton(info, media, "Insert selected ROM");
		ImGui::Separator();

		ImGui::TextUnformatted("Select new extension:"sv);
		im::Indent([&]{
			im::Combo("##extension", "TODO", [&]{
			});
		});
		if (ImGui::Button("Insert selected extension TODO")) {
			std::cerr << "TODO\n";
		}
	});
}

void ImGuiMedia::insertMedia(
	const std::string& media, MediaInfo& info,
	const std::string& filename, std::span<const std::string> patches, RomType romType)
{
	if (filename.empty()) return;
	auto cmd = makeTclList(media, "insert", filename);
	for (const auto& patch : patches) {
		cmd.addListElement("-ips");
		cmd.addListElement(patch);
	}
	if (romType != ROM_UNKNOWN) {
		cmd.addListElement("-romtype");
		cmd.addListElement(RomInfo::romTypeToName(romType));
	}
	manager.executeDelayed(cmd,
		[this, &info, filename, patches, romType](const TclObject&) {
			// only add to 'recent' when insert command succeeded
			addRecent(info, filename, patches, romType);
		});
}

void ImGuiMedia::addRecent(MediaInfo& info, const std::string& filename, std::span<const std::string> patches, RomType romType)
{
	auto& recent = info.recent;
	RecentItem item;
	item.filename = filename;
	item.ipsPatches.assign(patches.begin(), patches.end());
	item.romType = romType;
	if (auto it2 = ranges::find(recent, item); it2 != recent.end()) {
		// was already present, move to front
		std::rotate(recent.begin(), it2, it2 + 1);
	} else {
		// new entry, add it, but possibly remove oldest entry
		if (recent.full()) recent.pop_back();
		recent.push_front(std::move(item));
	}
}

} // namespace openmsx
