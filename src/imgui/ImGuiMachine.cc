#include "ImGuiMachine.hh"

#include "CustomFont.h"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiMedia.hh"
#include "ImGuiUtils.hh"

#include "BooleanSetting.hh"
#include "CartridgeSlotManager.hh"
#include "Debuggable.hh"
#include "Debugger.hh"
#include "GlobalSettings.hh"
#include "MSXCommandController.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "RealDrive.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"

#include "enumerate.hh"
#include "narrow.hh"

#include <imgui_stdlib.h>
#include <imgui.h>

#include <memory>

using namespace std::literals;


namespace openmsx {

void ImGuiMachine::save(ImGuiTextBuffer& buf)
{
	for (const auto& item : recentMachines) {
		buf.appendf("machine.recent=%s\n", item.c_str());
	}
}

void ImGuiMachine::loadLine(std::string_view name, zstring_view value)
{
	if (name == "machine.recent") {
		recentMachines.push_back(value);
	}
}

void ImGuiMachine::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Machine", [&]{
		auto& reactor = manager.getReactor();
		const auto& hotKey = reactor.getHotKey();

		ImGui::MenuItem("Select MSX machine ...", nullptr, &showSelectMachine);

		if (motherBoard) {
			const auto& controller = motherBoard->getMSXCommandController();
			if (auto* firmwareSwitch = dynamic_cast<BooleanSetting*>(controller.findSetting("firmwareswitch"))) {
				Checkbox(hotKey, "Firmware switch", *firmwareSwitch);
			}
		}

		auto& pauseSetting = reactor.getGlobalSettings().getPauseSetting();
		bool pause = pauseSetting.getBoolean();
		if (auto shortCut = getShortCutForCommand(hotKey, "toggle pause");
		    ImGui::MenuItem("Pause", shortCut.c_str(), &pause)) {
			pauseSetting.setBoolean(pause);
		}

		if (auto shortCut = getShortCutForCommand(hotKey, "reset");
		    ImGui::MenuItem("Reset", shortCut.c_str(), nullptr, motherBoard != nullptr)) {
			manager.executeDelayed(TclObject("reset"));
		}

		auto& powerSetting = reactor.getGlobalSettings().getPowerSetting();
		bool power = powerSetting.getBoolean();
		if (auto shortCut = getShortCutForCommand(hotKey, "toggle power");
		    ImGui::MenuItem("Power", shortCut.c_str(), &power)) {
			powerSetting.setBoolean(power);
		}

		ImGui::Separator();
		ImGui::MenuItem("Test MSX hardware ...", nullptr, &showTestHardware);
	});
}

void ImGuiMachine::paint(MSXMotherBoard* motherBoard)
{
	if (showSelectMachine) {
		paintSelectMachine(motherBoard);
	}
	if (showTestHardware) {
		paintTestHardware();
	}
}


void ImGuiMachine::paintSelectMachine(const MSXMotherBoard* motherBoard)
{
	ImGui::SetNextWindowSize(gl::vec2{29, 26} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Select MSX machine", &showSelectMachine, [&]{
		auto& reactor = manager.getReactor();
		auto instances = reactor.getMachineIDs();
		auto currentInstance = reactor.getMachineID();
		if (instances.size() > 1 || currentInstance.empty()) {
			ImGui::TextUnformatted("Instances:"sv);
			HelpMarker("Switch between different machine instances. Right-click to delete an instance.");
			im::Indent([&]{
				float height = (std::min(4.0f, float(instances.size())) + 0.25f) * ImGui::GetTextLineHeightWithSpacing();
				im::ListBox("##empty", {-FLT_MIN, height}, [&]{
					im::ID_for_range(instances.size(), [&](int i) {
						const auto& name = instances[i];
						bool isCurrent = name == currentInstance;
						auto board = reactor.getMachine(name);
						std::string display = [&]{
							if (board) {
								auto configName = board->getMachineName();
								auto* info = findMachineInfo(configName);
								assert(info);
								auto time = (board->getCurrentTime() - EmuTime::zero()).toDouble();
								return strCat(info->displayName, " (", formatTime(time), ')');
							} else {
								return std::string(name);
							}
						}();
						if (ImGui::Selectable(display.c_str(), isCurrent)) {
							manager.executeDelayed(makeTclList("activate_machine", name));
						}
						im::PopupContextItem("instance context menu", [&]{
							if (ImGui::Selectable("Delete instance")) {
								manager.executeDelayed(makeTclList("delete_machine", name));
							}
						});
					});
				});
			});
			ImGui::Separator();
		}

		if (motherBoard) {
			auto configName = motherBoard->getMachineName();
			auto* info = findMachineInfo(configName);
			assert(info);
			std::string display = strCat("Current machine: ", info->displayName);
			im::TreeNode(display.c_str(), [&]{
				printConfigInfo(*info);
			});
			if (newMachineConfig.empty()) newMachineConfig = configName;
			if (auto& defaultMachine = reactor.getMachineSetting();
			    defaultMachine.getString() != configName) {
				if (ImGui::Button("Make this the default machine")) {
					defaultMachine.setValue(TclObject(configName));
				}
				simpleToolTip("Use this as the default MSX machine when openMSX starts.");
			} else {
				im::Indent([] {
					ImGui::TextUnformatted("(This is the default machine)"sv);
					HelpMarker("If you select another machine than the default machine, a button will appear here to make that machine the default, i.e. the machine that is used when openMSX starts.");
				});
			}

			ImGui::Separator();
		}

		auto showMachine = [&](MachineInfo& info, bool doubleClickToSelect) {
			bool ok = getTestResult(info).empty();
			im::StyleColor(!ok, ImGuiCol_Text, getColor(imColor::ERROR), [&]{
				bool selected = info.configName == newMachineConfig;
				if (ImGui::Selectable(info.displayName.c_str(), selected,
						doubleClickToSelect ? ImGuiSelectableFlags_AllowDoubleClick: ImGuiSelectableFlags_None)) {
					newMachineConfig = info.configName;
					if (ok && (doubleClickToSelect ? ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) : true)) {
						showSelectMachine = false; // close window
						manager.executeDelayed(makeTclList("machine", newMachineConfig));
						addRecentItem(recentMachines, newMachineConfig);
					}
				}
				if (selected) {
					if (ImGui::IsWindowAppearing()) ImGui::SetScrollHereY();
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay | ImGuiHoveredFlags_Stationary)) {
					im::ItemTooltip([&]{
						printConfigInfo(info);
					});
				}
			});
		};

		im::TreeNode("Recently used", recentMachines.empty() ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_DefaultOpen, [&]{
			if (recentMachines.empty()) {
				ImGui::TextUnformatted("(none)"sv);
			} else {
				im::Combo("##recent", "Switch to recently used machine...", [&]{
					for (const auto& item : recentMachines) {
						if (auto* info = findMachineInfo(item)) {
							showMachine(*info, false);
						}
					}
				});
				simpleToolTip("Replace the current with the selected machine.");
			}
		});
		ImGui::Separator();

		ImGui::TextUnformatted("Available machines:"sv);
		auto& allMachines = getAllMachines();
		std::string filterDisplay = "filter";
		if (!filterType.empty() || !filterRegion.empty() || !filterString.empty()) strAppend(filterDisplay, ':');
		if (!filterType.empty()) strAppend(filterDisplay, ' ', filterType);
		if (!filterRegion.empty()) strAppend(filterDisplay, ' ', filterRegion);
		if (!filterString.empty()) strAppend(filterDisplay, ' ', filterString);
		strAppend(filterDisplay, "###filter");
		im::TreeNode(filterDisplay.c_str(), ImGuiTreeNodeFlags_DefaultOpen, [&]{
			displayFilterCombo(filterType, "Type", allMachines);
			displayFilterCombo(filterRegion, "Region", allMachines);
			if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
			ImGui::InputText(ICON_IGFD_FILTER, &filterString);
			simpleToolTip("A list of substrings that must be part of the machine name.\n"
					"\n"
					"For example: enter 'pa' to search for 'Panasonic' machines. "
					"Then refine the search by appending '<space>st' to find the 'Panasonic FS-A1ST' machine.");
		});

		auto filteredMachines = to_vector(xrange(allMachines.size()));
		applyComboFilter("Type",   filterType,   allMachines, filteredMachines);
		applyComboFilter("Region", filterRegion, allMachines, filteredMachines);
		applyDisplayNameFilter(filterString, allMachines, filteredMachines);

		auto it = ranges::find(filteredMachines, newMachineConfig,
			[&](auto idx) { return allMachines[idx].configName; });
		bool inFilteredList = it != filteredMachines.end();
		int selectedIdx = inFilteredList ? narrow<int>(*it) : -1;

		im::ListBox("##list", {-FLT_MIN, -ImGui::GetFrameHeightWithSpacing()}, [&]{
			im::ListClipper(filteredMachines.size(), selectedIdx, [&](int i) {
				auto idx = filteredMachines[i];
				auto& info = allMachines[idx];
				showMachine(info, true);
			});
		});

		bool ok = [&]{
			if (!inFilteredList) return false;
			auto* info = findMachineInfo(newMachineConfig);
			if (!info) return false;
			const auto& test = getTestResult(*info);
			return test.empty();
		}();
		im::Disabled(!ok, [&]{
			if (ImGui::Button("Replace current machine")) {
				manager.executeDelayed(makeTclList("machine", newMachineConfig));
			}
			simpleToolTip("Replace the current machine with the selected machine. "
					"Alternatively you can also double click in the list above (in addition that also closes this window).");
			ImGui::SameLine(0.0f, 10.0f);
			if (ImGui::Button("New machine instance")) {
				std::string script = strCat(
					"set id [create_machine]\n"
					"set err [catch {${id}::load_machine ", newMachineConfig, "} error_result]\n"
					"if {$err} {\n"
					"    delete_machine $id\n"
					"    error \"Error activating new machine: $error_result\"\n"
					"} else {\n"
					"    activate_machine $id\n"
					"}\n");
				manager.executeDelayed(TclObject(script));
			}
			simpleToolTip("Create a new machine instance (next to the current machine). "
					"Later you can switch between the different instances (like different tabs in a web browser).");
		});
	});
}

void ImGuiMachine::paintTestHardware()
{
	ImGui::SetNextWindowSize(gl::vec2{41.0f, 32.5f} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Test MSX hardware", &showTestHardware, [&]{
		auto formatNum = [](size_t num, std::string_view text) {
			if (num == 0) {
				return strCat("No ", text, "###", text);
			} else {
				return strCat(num, ' ', text, "###", text);
			}
		};
		auto printList = [](const auto& indices, const auto& infos) {
			auto n = narrow<int>(indices.size());
			auto clamped = std::clamp(narrow_cast<float>(n), 2.5f, 7.5f);
			gl::vec2 listSize{0.0f, clamped * ImGui::GetTextLineHeightWithSpacing()};

			ImGui::SetNextItemWidth(-FLT_MIN);
			im::ListBox("##workingList", listSize, [&]{
				im::ListClipper(n, [&](int i) {
					auto& info = infos[indices[i]];
					ImGui::TextUnformatted(info.displayName);
					assert(info.testResult);
					simpleToolTip(*info.testResult);
				});
			});
		};

		bool doTest = true;

		auto& allMachines = getAllMachines();
		std::vector<size_t> workingMachines, nonWorkingMachines;
		for (auto [idx, info] : enumerate(allMachines)) {
			if (!info.testResult.has_value()) {
				if (!doTest) continue; // only 1 test per iteration
				doTest = false;
			}
			const auto& result = getTestResult(info);
			(result.empty() ? workingMachines : nonWorkingMachines).push_back(idx);
		}
		bool allMachinesTested = allMachines.size() == (workingMachines.size() + nonWorkingMachines.size());

		im::VisuallyDisabled(!allMachinesTested, [&]{
			im::TreeNode("Machines", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				auto workingText = formatNum(workingMachines.size(), "working machines");
				im::TreeNode(workingText.c_str(), [&]{
					printList(workingMachines, allMachines);
				});
				auto nonWorkingText = formatNum(nonWorkingMachines.size(), "non-working machines");
				int flags = nonWorkingMachines.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen;
				im::TreeNode(nonWorkingText.c_str(), flags, [&]{
					printList(nonWorkingMachines, allMachines);
				});
			});
		});

		auto& allExtensions = manager.media->getAllExtensions();
		std::vector<size_t> workingExtensions, nonWorkingExtensions;
		for (auto [idx, info] : enumerate(allExtensions)) {
			if (!info.testResult.has_value()) {
				if (!doTest) continue; // only 1 test per iteration
				doTest = false;
			}
			const auto& result = manager.media->getTestResult(info);
			(result.empty() ? workingExtensions : nonWorkingExtensions).push_back(idx);
		}
		bool allExtensionsTested = allExtensions.size() == (workingExtensions.size() + nonWorkingExtensions.size());

		im::VisuallyDisabled(!allExtensionsTested, [&]{
			im::TreeNode("Extensions", ImGuiTreeNodeFlags_DefaultOpen, [&]{
				auto workingText = formatNum(workingExtensions.size(), "working extensions");
				im::TreeNode(workingText.c_str(), [&]{
					printList(workingExtensions, allExtensions);
				});
				auto nonWorkingText = formatNum(nonWorkingExtensions.size(), "non-working extensions");
				int flags = nonWorkingExtensions.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen;
				im::TreeNode(nonWorkingText.c_str(), flags, [&]{
					printList(nonWorkingExtensions, allExtensions);
				});

			});
		});

		if (!nonWorkingMachines.empty() || !nonWorkingExtensions.empty()) {
			im::Disabled(!allMachinesTested || !allExtensionsTested, [&]{
				if (ImGui::Button("Copy list of non-working hardware to clipboard")) {
					std::string result;
					auto print = [&](std::string_view label, const auto& indices, const auto& infos) {
						if (!indices.empty()) {
							strAppend(result, "Non-working ", label, ":\n");
							for (auto idx : indices) {
								const auto& info = infos[idx];
								strAppend(result, '\t', info.displayName, " (", info.configName, ")\n",
										"\t\t", *info.testResult, '\n');
							}
							strAppend(result, '\n');
						}
					};
					print("machines", nonWorkingMachines, allMachines);
					print("extensions", nonWorkingExtensions, allExtensions);
					ImGui::SetClipboardText(result.c_str());
				}
			});
		}

		im::Disabled(!allMachinesTested || !allExtensionsTested, [&]{
			if (ImGui::Button("Rerun test")) {
				manager.media->resetExtensionInfo();
				machineInfo.clear();
			}
		});
		ImGui::Separator();
		// TODO: what to do if the folder doesn't exist? Should we go
		// one higher? (But then the button doesn't browse to where it
		// promises to browse to and that may be confusing, e.g. if
		// people put their system roms there...
		if (ImGui::Button("Open user system ROMs folder...")) {
			SDL_OpenURL(strCat("file://", FileOperations::getUserDataDir(), "/systemroms").c_str());
		}
		if (ImGui::Button("Open system wide system ROMs folder...")) {
			SDL_OpenURL(strCat("file://", FileOperations::getSystemDataDir(), "/systemroms").c_str());
		}
	});
}

std::vector<ImGuiMachine::MachineInfo>& ImGuiMachine::getAllMachines()
{
	if (machineInfo.empty()) {
		machineInfo = parseAllConfigFiles<MachineInfo>(manager, "machines", {"Manufacturer"sv, "Product code"sv});
	}
	return machineInfo;
}

static void amendConfigInfo(MSXMotherBoard& mb, ImGuiMachine::MachineInfo& info)
{
	auto& configInfo = info.configInfo;

	const auto& debugger = mb.getDebugger();
	unsigned ramSize = 0;
	for (const auto& [name, debuggable] : debugger.getDebuggables()) {
		if (debuggable->getDescription() == one_of("memory mapper", "ram")) {
			ramSize += debuggable->getSize();
		}
	}
	configInfo.emplace_back("RAM size", strCat(ramSize / 1024, "kB"));

	if (auto* vdp = dynamic_cast<VDP*>(mb.findDevice("VDP"))) {
		configInfo.emplace_back("VRAM size", strCat(vdp->getVRAM().getSize() / 1024, "kB"));
		configInfo.emplace_back("VDP version", vdp->getVersionString());
	}

	if (auto drives = RealDrive::getDrivesInUse(mb)) {
		configInfo.emplace_back("Disk drives", strCat(narrow<int>(drives->count())));
	}

	const auto& carts = mb.getSlotManager();
	configInfo.emplace_back("Cartridge slots", strCat(carts.getNumberOfSlots()));
}

const std::string& ImGuiMachine::getTestResult(MachineInfo& info)
{
	if (!info.testResult) {
		info.testResult.emplace(); // empty string (for now)

		auto& reactor = manager.getReactor();
		manager.executeDelayed([&reactor, &info]() mutable {
			// don't create extra mb while drawing
			try {
				MSXMotherBoard mb(reactor);
				mb.getMSXCliComm().setSuppressMessages(true);
				mb.loadMachine(info.configName);
				assert(info.testResult->empty());
				amendConfigInfo(mb, info);
			} catch (MSXException& e) {
				info.testResult = e.getMessage(); // error
			}
		});
	}
	return info.testResult.value();
}

bool ImGuiMachine::printConfigInfo(MachineInfo& info)
{
	const auto& test = getTestResult(info);
	bool ok = test.empty();
	if (ok) {
		im::Table("##machine-info", 2, ImGuiTableFlags_SizingFixedFit, [&]{
			for (const auto& [desc, value_] : info.configInfo) {
				const auto& value = value_; // clang workaround
				if (ImGui::TableNextColumn()) {
					ImGui::TextUnformatted(desc);
				}
				if (ImGui::TableNextColumn()) {
					im::TextWrapPos(ImGui::GetFontSize() * 28.0f, [&] {
						ImGui::TextUnformatted(value);
					});
				}
			}
		});
	} else {
		im::StyleColor(ImGuiCol_Text, getColor(imColor::ERROR), [&]{
			im::TextWrapPos(ImGui::GetFontSize() * 35.0f, [&] {
				ImGui::TextUnformatted(test);
			});
		});
	}
	return ok;
}

ImGuiMachine::MachineInfo* ImGuiMachine::findMachineInfo(std::string_view config)
{
	auto& allMachines = getAllMachines();
	auto it = ranges::find(allMachines, config, &MachineInfo::configName);
	return (it != allMachines.end()) ? std::to_address(it) : nullptr;
}

} // namespace openmsx
