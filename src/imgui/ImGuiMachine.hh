#ifndef IMGUI_MACHINE_HH
#define IMGUI_MACHINE_HH

#include "ImGuiPart.hh"

#include <string>
#include <vector>

namespace openmsx {

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
	using ImGuiPart::ImGuiPart;

	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void paintSelectMachine(MSXMotherBoard* motherBoard);
	void paintTestHardware();
	[[nodiscard]] std::vector<MachineInfo>& getAllMachines();
	[[nodiscard]] MachineInfo* findMachineInfo(std::string_view config);
	[[nodiscard]] const std::string& getTestResult(MachineInfo& info);
	bool printConfigInfo(MachineInfo& info);

public:
	bool showSelectMachine = false;
	bool showTestHardware = false;

private:
	std::vector<MachineInfo> machineInfo; // sorted on displayName
	std::string newMachineConfig;
	std::string filterType;
	std::string filterRegion;
	std::string filterString;
};

} // namespace openmsx

#endif
