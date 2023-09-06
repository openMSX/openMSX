#ifndef IMGUI_MACHINE_HH
#define IMGUI_MACHINE_HH

#include "ImGuiPart.hh"

#include <string>
#include <vector>

namespace openmsx {

class ImGuiManager;
class MSXMotherBoard;

class ImGuiMachine final : public ImGuiPart
{
public:
	struct MachineInfo {
		std::string configName;
		std::string displayName;
		std::vector<std::pair<std::string, std::string>> configInfo;
		std::optional<std::string> testResult; // lazily initialized
	};

public:
	ImGuiMachine(ImGuiManager& manager_)
		: manager(manager_) {}

	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void paintSelectMachine(MSXMotherBoard* motherBoard);
	[[nodiscard]] std::vector<MachineInfo>& getAllMachines();
	[[nodiscard]] MachineInfo* findMachineInfo(std::string_view config);
	[[nodiscard]] const std::string& getTestResult(MachineInfo& info);
	[[nodiscard]] std::vector<std::string> getAllValuesFor(std::string_view key);
	bool printConfigInfo(MachineInfo& info);
	bool printConfigInfo(const std::string& config);

public:
	bool showSelectMachine = false;

private:
	ImGuiManager& manager;
	std::vector<MachineInfo> machineInfo; // sorted on displayName
	std::string newMachineConfig;
	std::string filterType;
	std::string filterRegion;
	std::string filterString;
};

} // namespace openmsx

#endif
