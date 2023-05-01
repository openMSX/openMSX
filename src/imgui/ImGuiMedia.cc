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

#include <imgui.h>

#include <algorithm>

using namespace std::literals;


namespace openmsx {

static constexpr size_t HISTORY_SIZE = 8;

void ImGuiMedia::save(ImGuiTextBuffer& buf)
{
	for (const auto& [media, history] : recentMedia) {
		for (const auto& fn : history) {
			buf.appendf("recent.%s=%s\n", media.c_str(), fn.c_str());
		}
	}
}

void ImGuiMedia::loadLine(std::string_view name, zstring_view value)
{
	if (name.starts_with("recent.")) {
		std::string media(name.substr(7));
		auto [it, inserted] = recentMedia.try_emplace(media, HISTORY_SIZE);
		auto& history = it->second;
		if (!history.full()) {
			history.push_back(value);
		}
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

		auto showRecent = [&](const std::string& media) {
			if (auto* recent = lookup(recentMedia, media); !recent->empty()) {
				im::Indent([&] {
					im::Menu("Recent", [&]{
						for (const auto& fn : *recent) {
							if (ImGui::MenuItem(fn.c_str())) {
								insertMedia(media, fn);
							}
						}
					});
				});
			}
		};

		// diskX
		auto drivesInUse = RealDrive::getDrivesInUse(*motherBoard); // TODO use this or fully rely on commands?
		std::string driveName = "diskX";
		for (auto i : xrange(RealDrive::MAX_DRIVES)) {
			if (!(*drivesInUse)[i]) continue;
			driveName[4] = char('a' + i);
			if (auto cmdResult = manager.execute(TclObject(driveName))) {
				elementInGroup();
				im::Menu(driveName.c_str(), [&]{
					auto currentImage = cmdResult->getListIndex(interp, 1);
					showCurrent(currentImage, "disk");
					if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
						manager.executeDelayed(makeTclList(driveName, "eject"));
					}
					if (ImGui::MenuItem("Insert disk image...")) {
						manager.openFile.selectFile(
							"Select disk image for " + driveName,
							buildFilter("disk images", DiskImageCLI::getExtensions()),
							[this, driveName](const auto& fn) { this->insertMedia(driveName, fn); });
					}
					showRecent(driveName);
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
						if (ImGui::MenuItem("Select ROM file...")) {
							manager.openFile.selectFile(
								"Select ROM image for " + cartName,
								buildFilter("Rom images", MSXRomCLI::getExtensions()),
								[this, cartName](const auto& fn) { this->insertMedia(cartName, fn); });
						}
						showRecent(cartName);
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
				if (ImGui::MenuItem("Insert cassette image...")) {
					manager.openFile.selectFile(
						"Select cassette image",
						buildFilter("Cassette images", CassettePlayerCLI::getExtensions()),
						[this](const auto& fn) { this->insertMedia("cassetteplayer", fn); });
				}
				showRecent("cassetteplayer");
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
				if (ImGui::MenuItem("Insert laserdisc image...")) {
					manager.openFile.selectFile(
						"Select laserdisc image",
						buildFilter("Laserdisk images", std::array<std::string_view, 1>{"ogv"}),
						[this](const auto& fn) { this->insertMedia("laserdiscplayer", fn); });
				}
				showRecent("laserdiscplayer");
			});
		}
	});
}

void ImGuiMedia::insertMedia(const std::string& media, const std::string& filename)
{
	manager.executeDelayed(makeTclList(media, "insert", filename));
	addRecent(media, filename);
}

void ImGuiMedia::addRecent(const std::string& media, const std::string& filename)
{
	auto [it, inserted] = recentMedia.try_emplace(media, HISTORY_SIZE);
	auto& recent = it->second;

	if (auto it2 = ranges::find(recent, filename); it2 != recent.end()) {
		// was already present, move to front
		std::rotate(recent.begin(), it2, it2 + 1);
	} else {
		// new entry, add it, but possibly remove oldest entry
		if (recent.full()) recent.pop_back();
		recent.push_front(filename);
	}
}

} // namespace openmsx
