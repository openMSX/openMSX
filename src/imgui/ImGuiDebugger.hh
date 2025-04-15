#ifndef IMGUI_DEBUGGER_HH
#define IMGUI_DEBUGGER_HH

#include "ImGuiPart.hh"

#include "CPURegs.hh"
#include "EmuTime.hh"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace openmsx {

class DebuggableEditor;
class Debugger;
class ImGuiBitmapViewer;
class ImGuiCharacter;
class ImGuiDisassembly;
class ImGuiSpriteViewer;
class MSXCPUInterface;

class ImGuiDebugger final : public ImGuiPart
{
public:
	struct PageInfo {
		uint8_t ps = 0;
		std::optional<uint8_t> ss;
		std::string segment;
	};
	using SlotInfo = std::array<PageInfo, 4>;

public:
	explicit ImGuiDebugger(ImGuiManager& manager);
	~ImGuiDebugger();

	static void loadIcons();

	[[nodiscard]] zstring_view iniName() const override { return "debugger"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadStart() override;
	void loadEnd() override;
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

	void signalBreak();
	void signalContinue();
	void signalQuit();
	void setGotoTarget(uint16_t target);
	void checkShortcuts(MSXCPUInterface& cpuInterface, MSXMotherBoard& motherBoard,
	                    ImGuiDisassembly* disassembly);

	[[nodiscard]] bool needSnapshot() const { return showChanges == SHOW_ALWAYS; }
	[[nodiscard]] bool needDrawChanges() const;
	[[nodiscard]] auto getChangesColor() const { return ImGui::ColorConvertFloat4ToU32(changesColor); }
	void configureChangesMenu();

private:
	SlotInfo getSlotInfo(MSXCPUInterface& cpuInterface, Debugger& debugger) const;

	void drawControl(MSXCPUInterface& cpuInterface, MSXMotherBoard& motherBoard);
	void drawSlots(MSXCPUInterface& cpuInterface, Debugger& debugger);
	void drawStack(const CPURegs& regs, const MSXCPUInterface& cpuInterface, EmuTime::param time);
	void drawRegisters(CPURegs& regs);
	void drawFlags(CPURegs& regs);

	void actionBreakContinue(MSXCPUInterface& cpuInterface);
	void actionStepIn(MSXCPUInterface& cpuInterface);
	void actionStepOver();
	void actionStepOut();
	void actionStepBack();

	enum LoadSave { LOAD, SAVE };
	void loadSaveBreakpoints(LoadSave loadSave);

private:
	std::vector<std::unique_ptr<ImGuiDisassembly>> disassemblyViewers;
	std::vector<std::unique_ptr<ImGuiBitmapViewer>> bitmapViewers;
	std::vector<std::unique_ptr<ImGuiCharacter>> tileViewers;
	std::vector<std::unique_ptr<ImGuiSpriteViewer>> spriteViewers;
	std::vector<std::unique_ptr<DebuggableEditor>> hexEditors; // sorted on 'getDebuggableName()'

	std::optional<SlotInfo> slotSnapshot;
	std::optional<CPURegs> cpuRegsSnapshot;
	int showChangesFrameCounter = 0;
	enum ShowChanges : int { SHOW_NEVER, SHOW_DURING_BREAK, SHOW_ALWAYS };
	int showChanges = SHOW_DURING_BREAK;
	gl::vec4 changesColor{1.0f, 0.0f, 0.0f, 1.0f}; // RGBA

	bool showControl = false;
	bool showSlots = false;
	bool showStack = false;
	bool showRegisters = false;
	bool showFlags = false;
	bool showXYFlags = false;
	int flagsLayout = 1;
	std::string breakpointFile;
	bool loadOnStartup = false;
	bool saveOnExit = false;

        std::string confirmText;
        std::function<void()> confirmAction;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"showControl",     &ImGuiDebugger::showControl},
		PersistentElement{"showRegisters",   &ImGuiDebugger::showRegisters},
		PersistentElement{"showSlots",       &ImGuiDebugger::showSlots},
		PersistentElement{"showStack",       &ImGuiDebugger::showStack},
		PersistentElement{"showFlags",       &ImGuiDebugger::showFlags},
		PersistentElement{"showXYFlags",     &ImGuiDebugger::showXYFlags},
		PersistentElementMax{"flagsLayout",  &ImGuiDebugger::flagsLayout, 2},
		PersistentElement{"showChanges",     &ImGuiDebugger::showChanges},
		PersistentElement{"changesColor",    &ImGuiDebugger::changesColor},
		PersistentElement{"breakpointFile",  &ImGuiDebugger::breakpointFile},
		PersistentElement{"loadOnStartup",   &ImGuiDebugger::loadOnStartup},
		PersistentElement{"saveOnExit",      &ImGuiDebugger::saveOnExit},

		// manually handle "showDebuggable.xxx"
	};
};

} // namespace openmsx

#endif
