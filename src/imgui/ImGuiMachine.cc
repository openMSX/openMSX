#include "ImGuiMachine.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "BooleanSetting.hh"
#include "GlobalSettings.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"

#include <imgui.h>

using namespace std::literals;


namespace openmsx {

void ImGuiMachine::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Machine", [&]{
		bool hasMachine = motherBoard != nullptr;

		ImGui::MenuItem("Select MSX machine ...", nullptr, &showSelectMachine);
		ImGui::MenuItem("Select extensions TODO", nullptr, nullptr, hasMachine);

		auto& pauseSetting = manager.getReactor().getGlobalSettings().getPauseSetting();
		bool pause = pauseSetting.getBoolean();
		if (ImGui::MenuItem("Pause", "PAUSE", &pause)) {
			pauseSetting.setBoolean(pause);
		}

		if (ImGui::MenuItem("Reset", nullptr, nullptr, hasMachine)) {
			manager.executeDelayed(TclObject("reset"));
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
	im::Window("Select MSX machine", &showSelectMachine, [&]{
		auto& reactor = manager.getReactor();
		auto instances = reactor.getMachineIDs();
		auto currentInstance = reactor.getMachineID();
		if (instances.size() > 1 || currentInstance.empty()) {
			ImGui::TextUnformatted("Instances:"sv);
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
									auto time = (board->getCurrentTime() - EmuTime::zero()).toDouble();
									return strCat(board->getMachineName(), " (", formatTime(time), ')');
								} else {
									return std::string(name);
								}
							}();
							if (ImGui::Selectable(display.c_str(), isCurrent)) {
								// TODO error handling?
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
			std::string machineName(motherBoard->getMachineName());
			std::string display = strCat("Current machine: ", machineName);
			im::TreeNode(display.c_str(), [&]{
				printConfigInfo(machineName);
			});
			if (newMachineConfig.empty()) newMachineConfig = machineName;
			auto& defaultMachine = reactor.getMachineSetting();
			if (defaultMachine.getString() != machineName) {
				if (ImGui::Button("Make this the default machine")) {
					defaultMachine.setValue(TclObject(machineName));
				}
				simpleToolTip("Use this as the default MSX machine when openMSX starts.");
			}
			ImGui::Separator();
		}

		ImGui::TextUnformatted("Select machine:"sv);
		im::Indent([&]{
			im::TreeNode("filter", [&]{
				auto combo = [&](std::string& selection, zstring_view key) {
					im::Combo(key.c_str(), selection.empty() ? "--all--" : selection.c_str(), [&]{
						if (ImGui::Selectable("--all--")) {
							selection.clear();
						}
						for (const auto& type : getAllValuesFor(TclObject(key))) {
							if (ImGui::Selectable(type.c_str())) {
								selection = type;
							}
						}
					});
				};
				combo(filterType, "type");
				combo(filterRegion, "region");
			});
			im::Combo("##combo", newMachineConfig.c_str(), [&]{
				auto allConfigs = getAllConfigs();
				auto filter = [&](std::string_view key, const std::string& value) {
					if (value.empty()) return;
					TclObject keyObj(key);
					std::erase_if(allConfigs, [&](const std::string& config) {
						const auto& info = getConfigInfo(config);
						auto val = info.getOptionalDictValue(keyObj);
						if (!val) return true; // remove items that don't have the key
						return *val != value;
					});
				};
				filter("type", filterType);
				filter("region", filterRegion);
				ImGuiListClipper clipper; // only draw the actually visible rows
				clipper.Begin(allConfigs.size());
				while (clipper.Step()) {
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
						const auto& config = allConfigs[i];
						bool ok = getTestResult(config).empty();
						im::StyleColor(ImGuiCol_Text, ok ? 0xFFFFFFFF : 0xFF0000FF, [&]{
							if (ImGui::Selectable(config.c_str(), config == newMachineConfig)) {
								newMachineConfig = config;
							}
						});
					}
				}
			});
			if (!newMachineConfig.empty()) {
				bool ok = printConfigInfo(newMachineConfig);
				im::Disabled(!ok, [&]{
					if (ImGui::Button("Replace current machine instance")) {
						// TODO error handling
						manager.executeDelayed(makeTclList("machine", newMachineConfig));
					}
					if (ImGui::Button("Create new machine instance")) {
						// TODO improve error handling
						std::string script = strCat(
							"set id [create_machine]\n"
							"set err [catch {${id}::load_machine ", newMachineConfig, "} error_result]\n"
							"if {$err} {\n"
							"    delete_machine $id\n"
							"    message \"Error activating new machine: $error_result\" error\n"
							"} else {\n"
							"    activate_machine $id\n"
							"}\n");
						manager.executeDelayed(TclObject(script));
					}
				});
			}
		});
	});
}

const std::vector<std::string>& ImGuiMachine::getAllConfigs()
{
	if (allConfigsCache.empty()) {
		allConfigsCache = Reactor::getHwConfigs("machines");
	}
	return allConfigsCache;
}

const std::string& ImGuiMachine::getTestResult(const std::string& config)
{
	auto [it, inserted] = testCache.try_emplace(config);
	auto& result = it->second;
	if (inserted) {
		manager.executeDelayed(
			makeTclList("test_machine", config),
			[&](const TclObject& r) { result = r.getString(); },
			[&](const std::string& e) { result = e; });
	}
	return result;
}

const TclObject& ImGuiMachine::getConfigInfo(const std::string& config)
{
	auto [it, inserted] = configInfoCache.try_emplace(config);
	auto& result = it->second;
	if (inserted) {
		if (auto r = manager.execute(makeTclList("openmsx_info", "machines", config))) {
			result = *r;
		}
	}
	return result;
}

std::vector<std::string> ImGuiMachine::getAllValuesFor(const TclObject& key)
{
	std::vector<std::string> result;
	for (const auto& config : getAllConfigs()) {
		const auto& info = getConfigInfo(config);
		if (auto type = info.getOptionalDictValue(key)) {
			if (!contains(result, *type)) { // O(N^2), but that's fine
				result.emplace_back(type->getString());
			}
		}
	}
	ranges::sort(result);
	return result;
}

bool ImGuiMachine::printConfigInfo(const std::string& config)
{
	const auto& test = getTestResult(config);
	bool ok = test.empty();
	if (ok) {
		const auto& info = getConfigInfo(config);
		auto& interp = manager.getInterpreter();
		for (unsigned i = 0, sz = info.size(); (i + 1) < sz; i += 2) {
			std::string key  {info.getListIndex(interp, i + 0).getString()};
			std::string value{info.getListIndex(interp, i + 1).getString()};
			ImGui::TextWrapped("%s: %s", key.c_str(), value.c_str());
		}
	} else {
		im::StyleColor(ImGuiCol_Text, 0xFF0000FF, [&]{
			ImGui::TextWrapped("%s", test.c_str());
		});
	}
	return ok;
}

} // namespace openmsx
