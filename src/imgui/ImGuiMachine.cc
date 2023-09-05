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
		bool hasMachine = motherBoard != nullptr;

		ImGui::MenuItem("Select MSX machine ...", nullptr, &showSelectMachine);

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

[[nodiscard]] static const std::string* getOptionalDictValue(
	const std::vector<std::pair<std::string, std::string>>& info,
	std::string_view key)
{
	auto it = ranges::find_if(info, [&](const auto& p) { return p.first == key; });
	if (it == info.end()) return {};
	return &it->second;
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
									auto time = (board->getCurrentTime() - EmuTime::zero()).toDouble();
									return strCat(board->getMachineName(), " (", formatTime(time), ')');
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
				combo(filterType, "type");
				combo(filterRegion, "region");
				ImGui::InputText(ICON_IGFD_SEARCH, &filterString);
				simpleToolTip("A list of substrings that must be part of the machine name.\n"
				              "\n"
				              "For example: enter 'pa' to search for 'Panasonic' machines. "
				              "Then refine the search by appending '<space>st' to find the 'Panasonic FS-A1ST' machine.");
			});
			im::ListBox("##list", [&]{
				auto filteredConfigs = getAllConfigs();
				auto filter = [&](std::string_view key, const std::string& value) {
					if (value.empty()) return;
					std::erase_if(filteredConfigs, [&](const std::string& config) {
						const auto& info = getConfigInfo(config);
						const auto* val = getOptionalDictValue(info, key);
						if (!val) return true; // remove items that don't have the key
						return *val != value;
					});
				};
				filter("type", filterType);
				filter("region", filterRegion);
				if (!filterString.empty()) {
					std::erase_if(filteredConfigs, [&](const std::string& config) {
						const auto& display = getDisplayName(config);
						return !ranges::all_of(StringOp::split_view<StringOp::REMOVE_EMPTY_PARTS>(filterString, ' '),
							[&](auto part) { return StringOp::containsCaseInsensitive(display, part); });
					});
				}

				ImGuiListClipper clipper; // only draw the actually visible rows
				clipper.Begin(narrow<int>(filteredConfigs.size()));
				while (clipper.Step()) {
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
						const auto& config = filteredConfigs[i];
						bool ok = getTestResult(config).empty();
						im::StyleColor(ImGuiCol_Text, ok ? 0xFFFFFFFF : 0xFF0000FF, [&]{
							const auto& display = getDisplayName(config);
							if (ImGui::Selectable(display.c_str(), config == newMachineConfig, ImGuiSelectableFlags_AllowDoubleClick)) {
								newMachineConfig = config;
								if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
									manager.executeDelayed(makeTclList("machine", newMachineConfig));
								}
							}
							im::ItemTooltip([&]{
								printConfigInfo(config);
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
		manager.executeDelayed([&, config]{
			// don't create extra mb while drawing
			try {
				MSXMotherBoard mb(manager.getReactor());
				mb.loadMachine(config);
				assert(result.empty());
				amendConfigInfo(mb, config);
			} catch (MSXException& e) {
				result = e.getMessage(); // error
			}
		});
	}
	return result;
}

std::vector<std::pair<std::string, std::string>>& ImGuiMachine::getConfigInfo(const std::string& config)
{
	static constexpr auto replacer = StringReplacer::create(
		"manufacturer", "Manufacturer",
		"code",         "Product code",
		"release_year", "Release year",
		"description",  "Description",
		"type",         "Type");

	auto [it, inserted] = configInfoCache.try_emplace(config);
	auto& result = it->second;
	if (inserted) {
		if (auto r = manager.execute(makeTclList("openmsx_info", "machines", config))) {
			//result = *r;
			auto first = r->begin();
			auto last = r->end();
			while (first != last) {
				auto desc = *first++;
				if (first == last) break; // shouldn't happen
				auto value = *first++;
				if (!value.empty()) {
					result.emplace_back(std::string(replacer(desc)),
					                    std::string(value));
				}
			}
		}
	}
	return result;
}

void ImGuiMachine::amendConfigInfo(MSXMotherBoard& mb, const std::string& config)
{
	auto& info = getConfigInfo(config);

	auto& debugger = mb.getDebugger();
	unsigned ramSize = 0;
	for (const auto& [name, debuggable] : debugger.getDebuggables()) {
		if (debuggable->getDescription() == one_of("memory mapper", "ram")) {
			ramSize += debuggable->getSize();
		}
	}
	info.emplace_back("RAM size", strCat(ramSize / 1024, "kB"));

	if (auto* vdp = dynamic_cast<VDP*>(mb.findDevice("VDP"))) {
		info.emplace_back("VRAM size", strCat(vdp->getVRAM().getSize() / 1024, "kB"));
		info.emplace_back("VDP version", vdp->getVersionString());
	}

	if (auto drives = RealDrive::getDrivesInUse(mb)) {
		info.emplace_back("Disk drives", strCat(narrow<int>(drives->count())));
	}

	auto& carts = mb.getSlotManager();
	info.emplace_back("Cartridge slots", strCat(carts.getNumberOfSlots()));
}

std::vector<std::string> ImGuiMachine::getAllValuesFor(std::string_view key)
{
	std::vector<std::string> result;
	for (const auto& config : getAllConfigs()) {
		const auto& info = getConfigInfo(config);
		if (const auto* type = getOptionalDictValue(info, key)) {
			if (!contains(result, *type)) { // O(N^2), but that's fine
				result.emplace_back(*type);
			}
		}
	}
	ranges::sort(result, StringOp::caseless{});
	return result;
}

bool ImGuiMachine::printConfigInfo(const std::string& config)
{
	const auto& test = getTestResult(config);
	bool ok = test.empty();
	if (ok) {
		const auto& info = getConfigInfo(config);
		im::Table("##machine-info", 2, ImGuiTableFlags_SizingFixedFit, [&]{
			for (const auto& [desc, value_] : info) {
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

const std::string& ImGuiMachine::getDisplayName(const std::string& config)
{
	auto [it, inserted] = displayCache.try_emplace(config);
	auto& result = it->second;
	if (inserted) {
		const auto& info = getConfigInfo(config);
		if (const auto* manufacturer = getOptionalDictValue(info, "Manufacturer")) {
			strAppend(result, *manufacturer); // possibly an empty string;
		}
		if (const auto* code = getOptionalDictValue(info, "Product code")) {
			if (!code->empty()) {
				if (!result.empty()) strAppend(result, ' ');
				strAppend(result, *code);
			}
		}
		if (result.empty()) result = config;
	}
	return result;
}

} // namespace openmsx
