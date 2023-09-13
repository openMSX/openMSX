#include "ImGuiMachine.hh"

#include "CustomFont.h"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
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

#include "StringOp.hh"
#include "StringReplacer.hh"

#include <imgui_stdlib.h>
#include <imgui.h>

using namespace std::literals;


namespace openmsx {

void ImGuiMachine::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Machine", [&]{
		auto& reactor = manager.getReactor();
		const auto& hotKey = reactor.getHotKey();

		ImGui::MenuItem("Select MSX machine ...", nullptr, &showSelectMachine);

		if (motherBoard) {
			auto& controller = motherBoard->getMSXCommandController();
			if (auto* firmwareSwitch = dynamic_cast<BooleanSetting*>(controller.findSetting("firmwareswitch"))) {
				Checkbox(*firmwareSwitch);
			}
		}

		auto& pauseSetting = reactor.getGlobalSettings().getPauseSetting();
		bool pause = pauseSetting.getBoolean();
		auto pauseShortCut = getShortCutForCommand(hotKey, "toggle pause");
		if (ImGui::MenuItem("Pause", pauseShortCut.c_str(), &pause)) {
			pauseSetting.setBoolean(pause);
		}

		auto resetShortCut = getShortCutForCommand(hotKey, "reset");
		if (ImGui::MenuItem("Reset", resetShortCut.c_str(), nullptr, motherBoard != nullptr)) {
			manager.executeDelayed(TclObject("reset"));
		}

		auto& powerSetting = reactor.getGlobalSettings().getPowerSetting();
		bool power = powerSetting.getBoolean();
		auto powerShortCut = getShortCutForCommand(hotKey, "toggle power");
		if (ImGui::MenuItem("Power", powerShortCut.c_str(), &power)) {
			powerSetting.setBoolean(power);
		}
	});
}

void ImGuiMachine::paint(MSXMotherBoard* motherBoard)
{
	if (showSelectMachine) {
		paintSelectMachine(motherBoard);
	}
}


void ImGuiMachine::paintSelectMachine(MSXMotherBoard* motherBoard)
{
	ImGui::SetNextWindowSize(gl::vec2{36, 31} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
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
					int i = 0;
					for (const auto& name : instances) {
						im::ID(++i, [&]{
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
					}
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
			auto& defaultMachine = reactor.getMachineSetting();
			if (defaultMachine.getString() != configName) {
				if (ImGui::Button("Make this the default machine")) {
					defaultMachine.setValue(TclObject(configName));
				}
				simpleToolTip("Use this as the default MSX machine when openMSX starts.");
			}
			ImGui::Separator();
		}

		im::TreeNode("Available machines", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			std::string filterDisplay = "filter";
			if (!filterType.empty() || !filterRegion.empty() || !filterString.empty()) strAppend(filterDisplay, ':');
			if (!filterType.empty()) strAppend(filterDisplay, ' ', filterType);
			if (!filterRegion.empty()) strAppend(filterDisplay, ' ', filterRegion);
			if (!filterString.empty()) strAppend(filterDisplay, ' ', filterString);
			strAppend(filterDisplay, "###filter");
			im::TreeNode(filterDisplay.c_str(), [&]{
				auto combo = [&](std::string& selection, zstring_view key) {
					im::Combo(key.c_str(), selection.empty() ? "--all--" : selection.c_str(), [&]{
						if (ImGui::Selectable("--all--")) {
							selection.clear();
						}
						for (const auto& type : getAllValuesFor(key)) {
							if (ImGui::Selectable(type.c_str())) {
								selection = type;
							}
						}
					});
				};
				combo(filterType, "Type");
				combo(filterRegion, "Region");
				ImGui::InputText(ICON_IGFD_SEARCH, &filterString);
				simpleToolTip("A list of substrings that must be part of the machine name.\n"
				              "\n"
				              "For example: enter 'pa' to search for 'Panasonic' machines. "
				              "Then refine the search by appending '<space>st' to find the 'Panasonic FS-A1ST' machine.");
			});
			im::ListBox("##list", [&]{
				auto& allMachines = getAllMachines();
				auto filteredMachines = to_vector(xrange(allMachines.size()));
				auto filter = [&](std::string_view key, const std::string& value) {
					if (value.empty()) return;
					std::erase_if(filteredMachines, [&](auto idx) {
						const auto& info = allMachines[idx].configInfo;
						const auto* val = getOptionalDictValue(info, key);
						if (!val) return true; // remove items that don't have the key
						return *val != value;
					});
				};
				filter("Type", filterType);
				filter("Region", filterRegion);
				if (!filterString.empty()) {
					std::erase_if(filteredMachines, [&](auto idx) {
						const auto& display = allMachines[idx].displayName;
						return !ranges::all_of(StringOp::split_view<StringOp::REMOVE_EMPTY_PARTS>(filterString, ' '),
							[&](auto part) { return StringOp::containsCaseInsensitive(display, part); });
					});
				}

				ImGuiListClipper clipper; // only draw the actually visible rows
				clipper.Begin(narrow<int>(filteredMachines.size()));
				while (clipper.Step()) {
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
						auto idx = filteredMachines[i];
						auto& info = allMachines[idx];
						bool ok = getTestResult(info).empty();
						im::StyleColor(!ok, ImGuiCol_Text, {1.0f, 0.0f, 0.0f, 1.0f}, [&]{
							if (ImGui::Selectable(info.displayName.c_str(), info.configName == newMachineConfig, ImGuiSelectableFlags_AllowDoubleClick)) {
								newMachineConfig = info.configName;
								if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
									manager.executeDelayed(makeTclList("machine", newMachineConfig));
								}
							}
							im::ItemTooltip([&]{
								printConfigInfo(info);
							});
						});
					}
				}
			});
		});
		ImGui::Separator();
		im::TreeNode("Selected machine", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			if (!newMachineConfig.empty()) {
				bool ok = printConfigInfo(newMachineConfig);
				ImGui::Spacing();
				im::Disabled(!ok, [&]{
					if (ImGui::Button("Replace")) {
						manager.executeDelayed(makeTclList("machine", newMachineConfig));
					}
					simpleToolTip("Replace the current machine with the selected machine. "
					              "Alternatively you can also double click in the list above.");
					ImGui::SameLine(0.0f, 10.0f);
					if (ImGui::Button("New")) {
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
			}
		});
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

	auto& debugger = mb.getDebugger();
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

	auto& carts = mb.getSlotManager();
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

std::vector<std::string> ImGuiMachine::getAllValuesFor(std::string_view key)
{
	std::vector<std::string> result;
	for (const auto& machine : getAllMachines()) {
		if (const auto* type = getOptionalDictValue(machine.configInfo, key)) {
			if (!contains(result, *type)) { // O(N^2), but that's fine
				result.emplace_back(*type);
			}
		}
	}
	ranges::sort(result, StringOp::caseless{});
	return result;
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
					im::TextWrapPos(ImGui::GetFontSize() * 35.0f, [&] {
						ImGui::TextUnformatted(value);
					});
				}
			}
		});
	} else {
		im::StyleColor(ImGuiCol_Text, 0xFF0000FF, [&]{
			im::TextWrapPos(ImGui::GetFontSize() * 35.0f, [&] {
				ImGui::TextUnformatted(test);
			});
		});
	}
	return ok;
}

bool ImGuiMachine::printConfigInfo(const std::string& config)
{
	auto* info = findMachineInfo(config);
	if (!info) return false;
	return printConfigInfo(*info);
}

ImGuiMachine::MachineInfo* ImGuiMachine::findMachineInfo(std::string_view config)
{
	auto& allMachines = getAllMachines();
	auto it = ranges::find(allMachines, config, &MachineInfo::configName);
	return (it != allMachines.end()) ? &*it : nullptr;
}

} // namespace openmsx
