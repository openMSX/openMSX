#ifndef IMGUI_DEBUGGER_HH
#define IMGUI_DEBUGGER_HH

#include "ImGuiBitmapViewer.hh"
#include "ImGuiReadHandler.hh"

#include "EmuTime.hh"

#include <map>
#include <memory>
#include <string>

struct ImGuiTextBuffer;

namespace openmsx {

class CPURegs;
class DebuggableEditor;
class MSXCPUInterface;
class MSXMotherBoard;

class ImGuiDebugger : public ImGuiReadHandler
{
public:
	ImGuiDebugger();
	~ImGuiDebugger();

	void signalBreak();
	void save(ImGuiTextBuffer& buf);
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard);
	void paint(MSXMotherBoard& motherBoard);

private:
	void drawDisassembly(CPURegs& regs, MSXCPUInterface& cpuInterface, EmuTime::param time);
	void drawStack(CPURegs& regs, MSXCPUInterface& cpuInterface, EmuTime::param time);
	void drawRegisters(CPURegs& regs);
	void drawFlags(CPURegs& regs);

public: // TODO
	ImGuiBitmapViewer bitmap;
private:
	std::map<std::string, std::unique_ptr<DebuggableEditor>> debuggables;

	bool showDisassembly;
	bool showStack;
	bool showRegisters;
	bool showFlags;
	bool showXYFlags;
	int flagsLayout;

	bool syncDisassemblyWithPC = false;
};

} // namespace openmsx

#endif
