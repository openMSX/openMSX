#ifndef IMGUI_DEBUGGER_HH
#define IMGUI_DEBUGGER_HH

#include "ImGuiPart.hh"

#include "EmuTime.hh"

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace openmsx {

class CPURegs;
class DebuggableEditor;
class ImGuiManager;
class MSXCPUInterface;
class MSXMotherBoard;

class ImGuiDebugger final : public ImGuiPart
{
public:
	ImGuiDebugger(ImGuiManager& manager);
	~ImGuiDebugger();

	void loadIcons();

	void signalBreak();

	[[nodiscard]] zstring_view iniName() const override { return "debugger"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void drawControl(MSXCPUInterface& cpuInterface);
	void drawDisassembly(CPURegs& regs, MSXCPUInterface& cpuInterface, EmuTime::param time);
	void drawStack(CPURegs& regs, MSXCPUInterface& cpuInterface, EmuTime::param time);
	void drawRegisters(CPURegs& regs);
	void drawFlags(CPURegs& regs);

private:
	ImGuiManager& manager;
	std::map<std::string, std::unique_ptr<DebuggableEditor>> debuggables;

	std::string gotoAddr;
	std::optional<unsigned> gotoTarget;

	bool showControl = false;
	bool showDisassembly = false;
	bool showStack = false;
	bool showRegisters = false;
	bool showFlags = false;
	bool showXYFlags = false;
	int flagsLayout = 1;

	bool syncDisassemblyWithPC = false;
};

} // namespace openmsx

#endif
