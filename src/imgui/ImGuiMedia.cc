#include "ImGuiMedia.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "CartridgeSlotManager.hh"
#include "CassettePlayerCLI.hh"
#include "DiskImageCLI.hh"
#include "MSXRomCLI.hh"
#include "RealDrive.hh"

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
	return buildFilter("disk images", DiskImageCLI::getExtensions());
}

static std::string display(const ImGuiMedia::RecentItem& item, int count)
{
	std::string result = item.filename;
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
								insertMedia(media, info, item.filename, item.ipsPatches);
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
			if (auto cmdResult = manager.execute(TclObject(driveName))) {
				elementInGroup();
				auto& info = mediaStuff.try_emplace(driveName).first->second;
				im::Menu(driveName.c_str(), [&]{
					auto currentImage = cmdResult->getListIndex(interp, 1);
					showCurrent(currentImage, "disk");
					if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
						manager.executeDelayed(makeTclList(driveName, "eject"));
					}
					if (ImGui::MenuItem("Insert disk image...")) {
						manager.openFile.selectFile(
							"Select disk image for " + driveName,
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
			if (auto cmdResult = manager.execute(TclObject(cartName))) {
				elementInGroup();
				im::Menu(cartName.c_str(), [&]{
					auto currentImage = cmdResult->getListIndex(interp, 1);
					showCurrent(currentImage, "cart"); // TODO cart/ext
					if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
						manager.executeDelayed(makeTclList(cartName, "eject"));
					}
					im::Menu("ROM cartridge", [&]{
						auto& info = mediaStuff.try_emplace(cartName).first->second;
						if (ImGui::MenuItem("Select ROM file...")) {
							manager.openFile.selectFile(
								"Select ROM image for " + cartName,
								buildFilter("Rom images", MSXRomCLI::getExtensions()),
								[this, cartName, &info](const auto& fn) { this->insertMedia(cartName, info, fn); });
						}
						showRecent(cartName, info);
						ImGui::MenuItem("select ROM type: TODO");
						ImGui::MenuItem("patch files: TODO");
					});
					if (ImGui::MenuItem("insert extension")) {
						ImGui::TextUnformatted("TODO"sv);
					}
				});
			}
		}
		endGroup();

		// cassetteplayer
		if (auto cmdResult = manager.execute(TclObject("cassetteplayer"))) {
			elementInGroup();
			im::Menu("cassetteplayer", [&]{
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
			im::Menu("laserdisc", [&]{
				auto currentImage = cmdResult->getListIndex(interp, 1);
				showCurrent(currentImage, "laserdisc");
				if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
					manager.executeDelayed(makeTclList("laserdiscplayer", "eject"));
				}
				auto& info = mediaStuff.try_emplace("laserdiscplayer").first->second;
				if (ImGui::MenuItem("Insert laserdisc image...")) {
					manager.openFile.selectFile(
						"Select laserdisc image",
						buildFilter("Laserdisk images", std::array<std::string_view, 1>{"ogv"}),
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
	std::string driveName = "diskX";
	for (auto i : xrange(RealDrive::MAX_DRIVES)) {
		if (!(*drivesInUse)[i]) continue;
		driveName[4] = char('a' + i);
		auto& info = mediaStuff.try_emplace(driveName).first->second;
		if (info.showAdvanced) {
			advancedDiskMenu(driveName, info);
		}
	}
}

void ImGuiMedia::advancedDiskMenu(const std::string& driveName, MediaInfo& info)
{
	im::Window(driveName.c_str(), &info.showAdvanced, [&]{
		if (auto cmdResult = manager.execute(makeTclList("machine_info", "media", driveName))) {
			bool copyCurrent = ImGui::SmallButton("Current disk:");
			TclObject currentTarget;
			TclObject currentPatches;
			im::Indent([&]{
				bool skip = false;
				if (auto type = cmdResult->getOptionalDictValue(TclObject("type"))) {
					auto s = type->getString();
					if (s == "empty") {
						ImGui::TextUnformatted("No disk inserted");
						skip = true;
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
					if (auto patches = cmdResult->getOptionalDictValue(TclObject("patches"));
					    patches && !patches->empty()) {
						currentPatches = *patches;
						ImGui::TextUnformatted("IPS patches:");
						im::Indent([&]{
							for (const auto& patch : currentPatches) {
								ImGui::TextUnformatted(patch);
							}
						});
					}
				}
			});
			if (copyCurrent) {
				info.imageName = currentTarget.getString();
				info.ipsPatches = to_vector<std::string>(currentPatches);
			}
			ImGui::Separator();
		}

		ImGui::TextUnformatted("Select new disk"sv);
		HelpMarker("Type-in or browse-to the disk image. Or type the special name 'ramdisk'.");
		im::Indent([&]{
			ImGui::InputText("##disk", &info.imageName);
			ImGui::SameLine(0.0f, 0.0f);
			im::Combo("##recent", "", ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft, [&]{
				int count = 0;
				for (auto& item : info.recent) {
					if (ImGui::Selectable(display(item, count).c_str())) {
						info.imageName = item.filename;
						info.ipsPatches = item.ipsPatches;
					}
					++count;
				}
			});
			ImGui::SameLine();
			if (ImGui::Button(ICON_IGFD_FOLDER_OPEN)) {
				manager.openFile.selectFile( // TODO allow to select directory
					"Select disk image for " + driveName,
					diskFilter(),
					[&](const auto& fn) { info.imageName = fn; });
			}
			simpleToolTip("browse");

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
		});
		im::Disabled(info.imageName.empty(), [&]{
			if (ImGui::Button("Insert disk")) {
				insertMedia(driveName, info, info.imageName, info.ipsPatches);
			}
		});
	});
}

void ImGuiMedia::insertMedia(
	const std::string& media, MediaInfo& info,
	const std::string& filename, std::span<const std::string> patches)
{
	if (filename.empty()) return;
	auto cmd = makeTclList(media, "insert", filename);
	for (const auto& patch : patches) {
		cmd.addListElement("-ips");
		cmd.addListElement(patch);
	}
	manager.executeDelayed(cmd,
		[this, &info, filename, patches](const TclObject&) { addRecent(info, filename, patches); });
}

void ImGuiMedia::addRecent(MediaInfo& info, const std::string& filename, std::span<const std::string> patches)
{
	auto& recent = info.recent;
	RecentItem item;
	item.filename = filename;
	item.ipsPatches.assign(patches.begin(), patches.end());
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
