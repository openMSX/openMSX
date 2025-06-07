#ifndef IMGUI_MACHINE_HH
#define IMGUI_MACHINE_HH

#include "ImGuiPart.hh"
#include "ImGuiUtils.hh"
#include "FileListWidget.hh"
#include "MSXMotherBoard.hh"

#include "circular_buffer.hh"

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
	explicit ImGuiMachine(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "machine"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

	void signalQuit();

private:
	void paintSelectMachine(const MSXMotherBoard* motherBoard);
	void paintTestHardware();
	[[nodiscard]] std::vector<MachineInfo>& getAllMachines();
	[[nodiscard]] MachineInfo* findMachineInfo(std::string_view config);
	[[nodiscard]] const std::string& getTestResult(MachineInfo& info);
	bool printConfigInfo(MachineInfo& info);
	void showSetupOverview(MSXMotherBoard& motherBoard, bool saveMode = false);

public:
	bool showSelectMachine = false;
	bool showTestHardware = false;

private:
	std::vector<MachineInfo> machineInfo; // sorted on displayName
	std::string newMachineConfig;
	std::string filterType;
	std::string filterRegion;
	std::string filterString;

	static constexpr size_t HISTORY_SIZE = 8;
	circular_buffer<std::string> recentMachines{HISTORY_SIZE};

	struct PreviewSetup {
		std::string name;
		std::string fullName;
		std::string lastExceptionMessage;
		std::shared_ptr<MSXMotherBoard> motherBoard;
	} previewSetup;
	bool saveSetupOpen = false;
	bool setSetupAsDefault = false;
	std::string saveSetupName;
	FileListWidget setupFileList;
	ConfirmDialog confirmDialog;
	SetupDepth saveSetupDepth = SetupDepth::CONNECTORS;
};

} // namespace openmsx

#endif
