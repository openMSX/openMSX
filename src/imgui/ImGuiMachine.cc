#include "ImGuiMachine.hh"

#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "MSXMotherBoard.hh"
#include "Reactor.hh"

#include <imgui.h>

namespace openmsx {

void ImGuiMachine::showMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Machine")) {
		return;
	}
	bool hasMachine = motherBoard != nullptr;

	ImGui::MenuItem("Select MSX machine ...", nullptr, &showSelectMachine);
	ImGui::MenuItem("Select extensions TODO", nullptr, nullptr, hasMachine);
	if (ImGui::MenuItem("Reset", nullptr, nullptr, hasMachine)) {
		manager.executeDelayed(TclObject("reset"));
	}

	ImGui::EndMenu();
}

void ImGuiMachine::paint(MSXMotherBoard* motherBoard)
{
	if (showSelectMachine) {
		paintSelectMachine(motherBoard);
	}
}

void ImGuiMachine::paintSelectMachine(MSXMotherBoard* motherBoard)
{
	if (!ImGui::Begin("Select MSX machine", &showSelectMachine)) {
		ImGui::End();
		return;
	}

	auto& reactor = manager.getReactor();
	auto instances = reactor.getMachineIDs();
	auto currentInstance = reactor.getMachineID();
	if (instances.size() > 1 || currentInstance.empty()) {
		ImGui::TextUnformatted("Instances:");
		ImGui::Indent();
		float height = (std::min(4.0f, float(instances.size())) + 0.25f) * ImGui::GetTextLineHeightWithSpacing();
		if (ImGui::BeginListBox("##empty", {-FLT_MIN, height})) {
			int i = 0;
			for (const auto& name : instances) {
				ImGui::PushID(++i);
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
					std::cerr << "DEBUG activate_machine " << name << "\n";
					manager.executeDelayed(makeTclList("activate_machine", name));
				}
				if (ImGui::BeginPopupContextItem("instance context menu")) {
					if (ImGui::Selectable("Delete instance")) {
						manager.executeDelayed(makeTclList("delete_machine", name));
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}
			ImGui::EndListBox();
		}
		ImGui::Unindent();
		ImGui::Separator();
	}

	if (motherBoard) {
		std::string machineName(motherBoard->getMachineName());
		std::string display = strCat("Current machine: ", machineName);
		if (ImGui::TreeNode(display.c_str())) {
			printConfigInfo(machineName);
			ImGui::TreePop();
		}
		if (newMachineConfig.empty()) newMachineConfig = machineName;
		ImGui::Separator();
	}

	ImGui::TextUnformatted("Select machine:");
	ImGui::Indent();
	if (ImGui::BeginCombo("##combo", newMachineConfig.c_str())) {
		const auto& allConfigs = getAllConfigs();
		ImGuiListClipper clipper; // only draw the actually visible rows
		clipper.Begin(allConfigs.size());
		while (clipper.Step()) {
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
				const auto& config = allConfigs[i];
				bool ok = getTestResult(config).empty();
				if (!ok) ImGui::PushStyleColor(ImGuiCol_Text, 0xFF0000FF);
				if (ImGui::Selectable(config.c_str(), config == newMachineConfig)) {
					newMachineConfig = config;
				}
				if (!ok) ImGui::PopStyleColor();
			}
		}
		ImGui::EndCombo();
	}
	if (!newMachineConfig.empty()) {
		bool ok = printConfigInfo(newMachineConfig);

		ImGui::BeginDisabled(!ok);
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
		ImGui::EndDisabled();
	}
	ImGui::Unindent();

	ImGui::End();
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
		ImGui::PushStyleColor(ImGuiCol_Text, 0xFF0000FF);
		ImGui::TextWrapped("%s", test.c_str());
		ImGui::PopStyleColor();
	}
	return ok;
}

} // namespace openmsx
