#ifndef IMGUI_DEBUGGER_HH
#define IMGUI_DEBUGGER_HH

#include "ImGuiPart.hh"

#include "Debuggable.hh"
#include "EmuTime.hh"

#include <imgui_memory_editor.h>

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace openmsx {

class CPURegs;
class Debugger;
class ImGuiManager;
class MSXCPUInterface;

class DebuggableEditor : public MemoryEditor
{
	struct CallbackInfo {
		Debuggable* debuggable;
		DebuggableEditor* editor;
	};

public:
	DebuggableEditor() {
		ReadFn = [](const ImU8* userdata, size_t offset) -> ImU8 {
			auto* debuggable = reinterpret_cast<CallbackInfo*>(const_cast<ImU8*>(userdata))->debuggable;
			return debuggable->read(narrow<unsigned>(offset));
		};
		WriteFn = [](ImU8* userdata, size_t offset, ImU8 data) -> void {
			auto* debuggable = reinterpret_cast<CallbackInfo*>(userdata)->debuggable;
			debuggable->write(narrow<unsigned>(offset), data);
		};
		HighlightFn = [](const ImU8* userdata, size_t offset) -> bool {
			// Also highlight preview-region when preview is not active.
			// Including when this editor has lost focus.
			const auto* editor = reinterpret_cast<const CallbackInfo*>(userdata)->editor;
			auto begin = editor->DataPreviewAddr;
			auto end = begin + editor->DataTypeGetSize(editor->PreviewDataType);
			return (begin <= offset) && (offset < end);
		};
		PreviewDataType = ImGuiDataType_U8;
	}

	void DrawWindow(const char* title, Debuggable& debuggable, size_t base_display_addr = 0x0000) {
		CallbackInfo info{&debuggable, this};
		MemoryEditor::DrawWindow(title, &info, debuggable.getSize(), base_display_addr);
	}
};


class ImGuiDebugger final : public ImGuiPart
{
public:
	ImGuiDebugger(ImGuiManager& manager);
	~ImGuiDebugger();

	void loadIcons();

	void signalBreak();

	[[nodiscard]] zstring_view iniName() const override { return "debugger"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadStart() override;
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void drawControl(MSXCPUInterface& cpuInterface);
	void drawDisassembly(CPURegs& regs, MSXCPUInterface& cpuInterface, Debugger& debugger, EmuTime::param time);
	void drawSlots(MSXCPUInterface& cpuInterface, Debugger& debugger);
	void drawStack(CPURegs& regs, MSXCPUInterface& cpuInterface, EmuTime::param time);
	void drawRegisters(CPURegs& regs);
	void drawFlags(CPURegs& regs);

private:
	ImGuiManager& manager;
	struct EditorInfo {
		explicit EditorInfo(const std::string& name_)
			: name(name_) {}
		std::string name;
		DebuggableEditor editor;
	};
	std::vector<EditorInfo> hexEditors;

	std::string gotoAddr;
	std::optional<unsigned> gotoTarget;
	bool followPC = false;

	bool showControl = false;
	bool showDisassembly = false;
	bool showSlots = false;
	bool showStack = false;
	bool showRegisters = false;
	bool showFlags = false;
	bool showXYFlags = false;
	int flagsLayout = 1;

	bool syncDisassemblyWithPC = false;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"showControl",     &ImGuiDebugger::showControl},
		PersistentElement{"showDisassembly", &ImGuiDebugger::showDisassembly},
		PersistentElement{"followPC",        &ImGuiDebugger::followPC},
		PersistentElement{"showRegisters",   &ImGuiDebugger::showRegisters},
		PersistentElement{"showSlots",       &ImGuiDebugger::showSlots},
		PersistentElement{"showStack",       &ImGuiDebugger::showStack},
		PersistentElement{"showFlags",       &ImGuiDebugger::showFlags},
		PersistentElement{"showXYFlags",     &ImGuiDebugger::showXYFlags},
		PersistentElementMax{"flagsLayout",  &ImGuiDebugger::flagsLayout, 2}
		// manually handle "showDebuggable.xxx"
	};
};

} // namespace openmsx

#endif
