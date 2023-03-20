#include "ImGuiMedia.hh"
#include "ImGuiManager.hh"

#include "CartridgeSlotManager.hh"
#include "CassettePlayerCLI.hh"
#include "DiskImageCLI.hh"
#include "MSXRomCLI.hh"
#include "RealDrive.hh"
#include "join.hh"
#include "view.hh"

#include <imgui.h>

namespace openmsx {

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
	if (!ImGui::BeginMenu("Media", motherBoard != nullptr)) {
		return;
	}
	assert(motherBoard);
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

	auto showCurrent = [&](TclObject current, const char* type) {
		if (current.empty()) {
			ImGui::Text("no %s inserted", type);
		} else {
			ImGui::Text("Current: %s", current.getString().c_str());
		}
		ImGui::Separator();
	};

	// diskX
	auto drivesInUse = RealDrive::getDrivesInUse(*motherBoard); // TODO use this or fully rely on commands?
	std::string driveName = "diskX";
	for (auto i : xrange(RealDrive::MAX_DRIVES)) {
		if (!(*drivesInUse)[i]) continue;
		driveName[4] = char('a' + i);
		if (auto cmdResult = manager.execute(TclObject(driveName))) {
			elementInGroup();
			if (ImGui::BeginMenu(driveName.c_str())) {
				auto currentImage = cmdResult->getListIndex(interp, 1);
				showCurrent(currentImage, "disk");
				if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
					manager.executeDelayed(makeTclList(driveName, "eject"));
				}
				if (ImGui::MenuItem("Insert disk image")) {
					manager.openFile.selectFileCommand(
						"Select disk image for " + driveName,
						buildFilter("disk images", DiskImageCLI::getExtensions()),
						makeTclList(driveName, "insert"));
				}
				ImGui::EndMenu();
			}
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
			if (ImGui::BeginMenu(cartName.c_str())) {
				auto currentImage = cmdResult->getListIndex(interp, 1);
				showCurrent(currentImage, "cart"); // TODO cart/ext
				if (ImGui::MenuItem("Eject", nullptr, false, !currentImage.empty())) {
					manager.executeDelayed(makeTclList(cartName, "eject"));
				}
				if (ImGui::BeginMenu("ROM cartridge")) {
					if (ImGui::MenuItem("select ROM file")) {
						manager.openFile.selectFileCommand(
							"Select ROM image for " + cartName,
							buildFilter("Rom images", MSXRomCLI::getExtensions()),
							makeTclList(cartName, "insert"));
					}
					ImGui::MenuItem("select ROM type: TODO");
					ImGui::MenuItem("patch files: TODO");
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("insert extension")) {
					ImGui::TextUnformatted("TODO");
				}
				ImGui::EndMenu();
			}
		}
	}
	endGroup();

	// cassetteplayer
	if (auto cmdResult = manager.execute(TclObject("cassetteplayer"))) {
		elementInGroup();
		if (ImGui::BeginMenu("cassetteplayer")) {
			auto currentImage = cmdResult->getListIndex(interp, 1);
			showCurrent(currentImage, "cassette");
			if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
				manager.executeDelayed(makeTclList("cassetteplayer", "eject"));
			}
			if (ImGui::MenuItem("insert cassette image")) {
				manager.openFile.selectFileCommand(
					"Select cassette image",
					buildFilter("Cassette images", CassettePlayerCLI::getExtensions()),
					makeTclList("cassetteplayer", "insert"));
			}
			ImGui::EndMenu();
		}
	}
	endGroup();

	// laserdisc
	if (auto cmdResult = manager.execute(TclObject("laserdiscplayer"))) {
		elementInGroup();
		if (ImGui::BeginMenu("laserdisc")) {
			auto currentImage = cmdResult->getListIndex(interp, 1);
			showCurrent(currentImage, "laserdisc");
			if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
				manager.executeDelayed(makeTclList("laserdiscplayer", "eject"));
			}
			if (ImGui::MenuItem("insert laserdisc image")) {
				manager.openFile.selectFileCommand(
					"Select laserdisc image",
					buildFilter("Laserdisk images", std::array<std::string_view, 1>{"ogv"}),
					makeTclList("laserdiscplayer", "insert"));
			}
			ImGui::EndMenu();
		}
	}

	ImGui::EndMenu();
}

} // namespace openmsx
