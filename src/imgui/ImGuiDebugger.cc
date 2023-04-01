#include "ImGuiDebugger.hh"

#include "ImGuiBitmapViewer.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "CPURegs.hh"
#include "Dasm.hh"
#include "Debugger.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <cstdint>
#include <vector>


namespace openmsx {

struct DebuggableEditor : public MemoryEditor
{
	DebuggableEditor() {
		Open = false;
		ReadFn = [](const ImU8* userdata, size_t offset) -> ImU8 {
			auto* debuggable = reinterpret_cast<Debuggable*>(const_cast<ImU8*>(userdata));
			return debuggable->read(narrow<unsigned>(offset));
		};
		WriteFn = [](ImU8* userdata, size_t offset, ImU8 data) -> void {
			auto* debuggable = reinterpret_cast<Debuggable*>(userdata);
			debuggable->write(narrow<unsigned>(offset), data);
		};
	}

	void DrawWindow(const char* title, Debuggable& debuggable, size_t base_display_addr = 0x0000) {
		MemoryEditor::DrawWindow(title, &debuggable, debuggable.getSize(), base_display_addr);
	}
};


ImGuiDebugger::ImGuiDebugger(ImGuiManager& manager_)
	: manager(manager_)
{
}

ImGuiDebugger::~ImGuiDebugger() = default;

void ImGuiDebugger::signalBreak()
{
	syncDisassemblyWithPC = true;
}

void ImGuiDebugger::save(ImGuiTextBuffer& buf)
{
	buf.appendf("showDisassembly=%d\n", showDisassembly);
	buf.appendf("showRegisters=%d\n", showRegisters);
	buf.appendf("showStack=%d\n", showStack);
	buf.appendf("showFlags=%d\n", showFlags);
	buf.appendf("showXYFlags=%d\n", showXYFlags);
	buf.appendf("flagsLayout=%d\n", flagsLayout);
	for (const auto& [name, editor] : debuggables) {
		buf.appendf("showDebuggable.%s=%d\n", name.c_str(), editor->Open);
	}
}

void ImGuiDebugger::loadLine(std::string_view name, zstring_view value)
{
	static constexpr std::string_view prefix = "showDebuggable.";

	if (name == "showDisassembly") {
		showDisassembly = StringOp::stringToBool(value);
	} else if (name == "showRegisters") {
		showRegisters = StringOp::stringToBool(value);
	} else if (name == "showStack") {
		showStack = StringOp::stringToBool(value);
	} else if (name == "showFlags") {
		showFlags = StringOp::stringToBool(value);
	} else if (name == "showXYFlags") {
		showXYFlags = StringOp::stringToBool(value);
	} else if (name == "flagsLayout") {
		if (auto r = StringOp::stringTo<unsigned>(value)) {
			if (*r <= 1) flagsLayout = *r;
		}
	} else if (name.starts_with(prefix)) {
		auto debuggableName = name.substr(prefix.size());
		auto [it, inserted] = debuggables.try_emplace(std::string(debuggableName));
		auto& editor = it->second;
		if (inserted) {
			editor = std::make_unique<DebuggableEditor>();
		}
		editor->Open = StringOp::stringToBool(value);
	}
}

void ImGuiDebugger::showMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Debugger", motherBoard != nullptr)) {
		return;
	}
	assert(motherBoard);

	ImGui::MenuItem("disassembly", nullptr, &showDisassembly);
	ImGui::MenuItem("CPU registers", nullptr, &showRegisters);
	ImGui::MenuItem("CPU flags", nullptr, &showFlags);
	ImGui::MenuItem("stack", nullptr, &showStack);
	ImGui::MenuItem("memory TODO");
	ImGui::Separator();
	ImGui::MenuItem("VDP bitmap viewer", nullptr, &manager.bitmap.showBitmapViewer);
	ImGui::MenuItem("TODO several more");
	ImGui::Separator();
	if (ImGui::BeginMenu("All debuggables")) {
		auto& debugger = motherBoard->getDebugger();
		for (auto& [name, editor] : debuggables) {
			if (debugger.findDebuggable(name)) {
				ImGui::MenuItem(name.c_str(), nullptr, &editor->Open);
			}
		}
		ImGui::EndMenu();
	}
	ImGui::EndMenu();
}

void ImGuiDebugger::paint(MSXMotherBoard* motherBoard)
{
	if (!motherBoard) return;

	auto& regs = motherBoard->getCPU().getRegisters();
	auto& cpuInterface = motherBoard->getCPUInterface();
	auto time = motherBoard->getCurrentTime();
	drawDisassembly(regs, cpuInterface, time);
	drawStack(regs, cpuInterface, time);
	drawRegisters(regs);
	drawFlags(regs);

	// Show the enabled 'debuggables'
	for (auto& [name, debuggable] : motherBoard->getDebugger().getDebuggables()) {
		auto [it, inserted] = debuggables.try_emplace(name);
		auto& editor = it->second;
		if (inserted) {
			editor = std::make_unique<DebuggableEditor>();
			editor->Open = false;
		}
		if (editor->Open) {
			editor->DrawWindow(name.c_str(), *debuggable);
		}
	}
}

void ImGuiDebugger::drawDisassembly(CPURegs& regs, MSXCPUInterface& cpuInterface, EmuTime::param time)
{
	if (!showDisassembly) return;
	if (!ImGui::Begin("Disassembly", &showDisassembly)) {
		ImGui::End();
		return;
	}

	auto pc = regs.getPC();

	// TODO can this be optimized/avoided?
	std::vector<uint16_t> startAddresses;
	unsigned startAddr = 0;
	while (startAddr < 0x10000) {
		auto addr = narrow<uint16_t>(startAddr);
		startAddresses.push_back(addr);
		startAddr += instructionLength(cpuInterface, addr, time);
	}

	int flags = ImGuiTableFlags_RowBg |
	            ImGuiTableFlags_BordersV |
	            ImGuiTableFlags_BordersOuterV |
	            ImGuiTableFlags_Resizable |
	            ImGuiTableFlags_ScrollX;
	if (ImGui::BeginTable("table", 5, flags)) {
		ImGui::TableSetupColumn("bp", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("label");
		ImGui::TableSetupColumn("address");
		ImGui::TableSetupColumn("opcode");
		ImGui::TableSetupColumn("mnemonic", ImGuiTableColumnFlags_WidthStretch);

		std::string mnemonic;
		std::string opcodesStr;
		std::array<uint8_t, 4> opcodes;
		ImGuiListClipper clipper; // only draw the actually visible rows
		clipper.Begin(0x10000);
		while (clipper.Step()) {
			auto it = ranges::lower_bound(startAddresses, clipper.DisplayStart);
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
				ImGui::TableNextRow();
				if (it == startAddresses.end()) continue;
				auto addr = *it++;

				ImGui::TableSetColumnIndex(0); // bp
				if (!syncDisassemblyWithPC && (addr == pc)) {
					ImGui::Selectable("##row", true,
					                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap);
				}

				mnemonic.clear();
				auto len = dasm(cpuInterface, addr, opcodes, mnemonic, time);
				assert(len >= 1);

				ImGui::TableSetColumnIndex(2); // addr
				auto addrStr = tmpStrCat(hex_string<4>(addr));
				ImGui::TextUnformatted(addrStr.c_str());

				ImGui::TableSetColumnIndex(3); // opcode
				opcodesStr.clear();
				for (auto i : xrange(len)) {
					strAppend(opcodesStr, hex_string<2>(opcodes[i]), ' ');
				}
				ImGui::TextUnformatted(opcodesStr.data(), opcodesStr.data() + 3 * len - 1);

				ImGui::TableSetColumnIndex(4); // mnemonic
				ImGui::TextUnformatted(mnemonic.c_str());
			}
		}
		if (syncDisassemblyWithPC) {
			syncDisassemblyWithPC = false; // only once

			auto itemHeight = ImGui::GetTextLineHeightWithSpacing();
			auto winHeight = ImGui::GetWindowHeight();
			auto lines = size_t(winHeight / itemHeight); // approx

			auto n = std::distance(startAddresses.begin(), ranges::lower_bound(startAddresses, pc));
			auto n2 = std::max(size_t(0), n - lines / 4);
			auto topAddr = startAddresses[n2];

			ImGui::SetScrollY(topAddr * itemHeight);
		}
		ImGui::EndTable();
	}

	ImGui::End();
}

void ImGuiDebugger::drawStack(CPURegs& regs, MSXCPUInterface& cpuInterface, EmuTime::param time)
{
	if (!showStack) return;

	auto line = ImGui::GetTextLineHeightWithSpacing();
	ImGui::SetNextWindowSize(ImVec2(0.0f, 12 * line), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("stack", &showStack)) {
		ImGui::End();
		return;
	}

	auto sp = regs.getSP();

	int flags = ImGuiTableFlags_ScrollY |
	            ImGuiTableFlags_BordersInnerV |
	            ImGuiTableFlags_Resizable |
	            ImGuiTableFlags_Reorderable |
	            ImGuiTableFlags_Hideable |
	            ImGuiTableFlags_ContextMenuInBody;
	if (ImGui::BeginTable("table", 3, flags)) {
		ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
		ImGui::TableSetupColumn("Address");
		ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_DefaultHide);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide);
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper; // only draw the actually visible rows
		clipper.Begin(std::min(128, (0x10000 - sp) / 2));
		while (clipper.Step()) {
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
				auto offset = 2 * row;
				auto addr = sp + offset;
				if (ImGui::TableNextColumn()) { // address
					ImGui::Text("%04X", addr);
				}
				if (ImGui::TableNextColumn()) { // offset
					ImGui::Text("SP+%X", offset);
				}
				if (ImGui::TableNextColumn()) { // value
					auto l = cpuInterface.peekMem(narrow_cast<uint16_t>(addr + 0), time);
					auto h = cpuInterface.peekMem(narrow_cast<uint16_t>(addr + 1), time);
					auto value = narrow<uint16_t>(256 * h + l);
					ImGui::Text("%04X", value);
				}
			}
		}
		ImGui::EndTable();
	}

	ImGui::End();
}

void ImGuiDebugger::drawRegisters(CPURegs& regs)
{
	if (!showRegisters) return;
	if (!ImGui::Begin("CPU registers", &showRegisters)) {
		ImGui::End();
		return;
	}

	const auto& style = ImGui::GetStyle();
	auto padding = 2 * style.FramePadding.x;
	auto width16 = ImGui::CalcTextSize("FFFF").x + padding;
	auto edit16 = [&](const char* label, auto getter, auto setter) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(width16);
		uint16_t value = getter();
		if (ImGui::InputScalar(tmpStrCat("##", label).c_str(), ImGuiDataType_U16, &value, nullptr, nullptr, "%04X")) {
			setter(value);
		}
	};
	auto edit8 = [&](const char* label, auto getter, auto setter) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(width16);
		uint8_t value = getter();
		if (ImGui::InputScalar(tmpStrCat("##", label).c_str(), ImGuiDataType_U8, &value, nullptr, nullptr, "%02X")) {
			setter(value);
		}
	};

	edit16("AF", [&]{ return regs.getAF(); }, [&](uint16_t value) { regs.setAF(value); });
	ImGui::SameLine(0.0f, 20.0f);
	edit16("AF'", [&]{ return regs.getAF2(); }, [&](uint16_t value) { regs.setAF2(value); });

	edit16("BC", [&]{ return regs.getBC(); }, [&](uint16_t value) { regs.setBC(value); });
	ImGui::SameLine(0.0f, 20.0f);
	edit16("BC'", [&]{ return regs.getBC2(); }, [&](uint16_t value) { regs.setBC2(value); });

	edit16("DE", [&]{ return regs.getDE(); }, [&](uint16_t value) { regs.setDE(value); });
	ImGui::SameLine(0.0f, 20.0f);
	edit16("DE'", [&]{ return regs.getDE2(); }, [&](uint16_t value) { regs.setDE2(value); });

	edit16("HL", [&]{ return regs.getHL(); }, [&](uint16_t value) { regs.setHL(value); });
	ImGui::SameLine(0.0f, 20.0f);
	edit16("HL'", [&]{ return regs.getHL2(); }, [&](uint16_t value) { regs.setHL2(value); });

	edit16("IX", [&]{ return regs.getIX(); }, [&](uint16_t value) { regs.setIX(value); });
	ImGui::SameLine(0.0f, 20.0f);
	edit16("IY ", [&]{ return regs.getIY(); }, [&](uint16_t value) { regs.setIY(value); });

	edit16("PC", [&]{ return regs.getPC(); }, [&](uint16_t value) { regs.setPC(value); });
	ImGui::SameLine(0.0f, 20.0f);
	edit16("SP ", [&]{ return regs.getSP(); }, [&](uint16_t value) { regs.setSP(value); });

	edit8("I ", [&]{ return regs.getI(); }, [&](uint8_t value) { regs.setI(value); });
	ImGui::SameLine(0.0f, 20.0f);
	edit8("R  ", [&]{ return regs.getR(); }, [&](uint8_t value) { regs.setR(value); });

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("IM");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(width16);
	uint8_t im = regs.getIM();
	if (ImGui::InputScalar("##IM", ImGuiDataType_U8, &im, nullptr, nullptr, "%d")) {
		if (im <= 2) regs.setIM(im);
	}

	ImGui::SameLine(0.0f, 20.0f);
	ImGui::AlignTextToFramePadding();
	bool ei = regs.getIFF1();
	if (ImGui::Selectable(ei ? "EI" : "DI", false, ImGuiSelectableFlags_AllowDoubleClick)) {
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			regs.setIFF1(!ei);
			regs.setIFF2(!ei);
		}
	}
	simpleToolTip("double-click to toggle");

	ImGui::End();
}

void ImGuiDebugger::drawFlags(CPURegs& regs)
{
	if (!showFlags) return;
	if (!ImGui::Begin("CPU flags", &showFlags)) {
		ImGui::End();
		return;
	}

	auto sizeH1 = ImGui::CalcTextSize("NC");
	auto sizeH2 = ImGui::CalcTextSize("X:0");
	auto sizeV = ImGui::CalcTextSize("C 0 (NC)");

	auto f = regs.getF();

	auto draw = [&](const char* name, uint8_t bit, const char* val0 = nullptr, const char* val1 = nullptr) {
		std::string s;
		ImVec2 sz;
		if (flagsLayout == 0) {
			// horizontal
			if (val0) {
				s = (f & bit) ? val1 : val0;
				sz = sizeH1;
			} else {
				s = strCat(name, ':', (f & bit) ? '1' : '0');
				sz = sizeH2;
			}
		} else {
			// vertical
			s = strCat(name, ' ', (f & bit) ? '1' : '0');
			if (val0) {
				strAppend(s, " (", (f & bit) ? val1 : val0, ')');
			}
			sz = sizeV;
		}
		if (ImGui::Selectable(s.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick, sz)) {
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				regs.setF(f ^ bit);
			}
		}
		simpleToolTip("double-click to toggle");
		if (flagsLayout == 0) {
			// horizontal
			ImGui::SameLine(0.0f, sizeH1.x);
		}
	};

	draw("S", 0x80, " P", " M");
	draw("Z", 0x40, "NZ", " Z");
	if (showXYFlags) draw("Y", 0x20);
	draw("H", 0x10);
	if (showXYFlags) draw("X", 0x08);
	draw("P", 0x04, "PO", "PE");
	draw("N", 0x02);
	draw("C", 0x01, "NC", " C");

	if (ImGui::BeginPopupContextWindow()) {
		ImGui::TextUnformatted("Layout");
		ImGui::RadioButton("horizontal", &flagsLayout, 0);
		ImGui::RadioButton("vertical", &flagsLayout, 1);
		ImGui::Separator();
		ImGui::Checkbox("show undocumented", &showXYFlags);
		ImGui::EndPopup();
	}

	ImGui::End();
}

} // namespace openmsx
