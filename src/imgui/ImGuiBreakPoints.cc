#include "ImGuiDebugger.hh"

#include "ImGuiBitmapViewer.hh"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "BreakPoint.hh"
#include "CPURegs.hh"
#include "Dasm.hh"
#include "Debugger.hh"
#include "Interpreter.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXMemoryMapperBase.hh"
#include "MSXMotherBoard.hh"
#include "RomPlain.hh"

#include "narrow.hh"
#include "ranges.hh"
#include "stl.hh"
#include "strCat.hh"
#include "unreachable.hh"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include <cstdint>
#include <vector>


namespace openmsx {

ImGuiBreakPoints::ImGuiBreakPoints(ImGuiManager& manager_)
	: manager(manager_)
{
}

void ImGuiBreakPoints::save(ImGuiTextBuffer& buf)
{
	buf.appendf("show=%d\n", show);
}

void ImGuiBreakPoints::loadLine(std::string_view name, zstring_view value)
{
	if (name == "show") {
		show = StringOp::stringToBool(value);
	}
}

void ImGuiBreakPoints::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	im::Window("Breakpoints", &show, [&]{
		im::TabBar("tabs", [&]{
			auto& cpuInterface = motherBoard->getCPUInterface();
			im::TabItem("Breakpoints", [&]{
				paintTab(cpuInterface);
			});
			im::TabItem("Watchpoints", [&]{
				ImGui::TextUnformatted("TODO");
			});
			im::TabItem("Conditions", [&]{
				ImGui::TextUnformatted("TODO");
			});
		});
	});
}

ParsedSlotCond::ParsedSlotCond(std::string_view cond)
	: rest(cond) // for when we fail to match a slot-expression
{
	size_t pos = 0;
	auto end = cond.size();
	std::optional<int> o_ps;
	std::optional<int> o_ss;
	std::optional<int> o_seg;

	auto next = [&](std::string_view s) {
		if (cond.substr(pos).starts_with(s)) {
			pos += s.size();
			return true;
		}
		return false;
	};
	auto done = [&]{
		bool stop = cond.substr(pos) == "]";
		if (stop || (next("] && (") && cond.ends_with(')'))) {
			if (stop) {
				rest.clear();
			} else {
				rest = cond.substr(pos, cond.size() - pos - 1);
			}
			if (o_ps)  { hasPs  = true; ps  = *o_ps;  }
			if (o_ss)  { hasSs  = true; ss  = *o_ss;  }
			if (o_seg) { hasSeg = true; seg = narrow<uint8_t>(*o_seg); }
			return true;
		}
		return false;
	};
	auto isDigit = [](char c) { return ('0' <= c) && (c <= '9'); };
	auto getInt = [&](unsigned max) -> std::optional<int> {
		unsigned i = 0;
		if ((pos == end) || !isDigit(cond[pos])) return {};
		while ((pos != end) && isDigit(cond[pos])) {
			i = 10 * i + (cond[pos] - '0');
			++pos;
		}
		if (i >= max) return {};
		return i;
	};

	if (!next("[pc_in_slot ")) return; // no slot
	o_ps = getInt(4);
	if (!o_ps) return; // invalid ps
	if (done()) return; // ok, only ps
	if (!next(" ")) return; // invalid separator
	if (!next("X")) {
		o_ss = getInt(4);
		if (!o_ss) return; // invalid ss
	}
	if (done()) return; // ok, ps + ss
	if (!next(" ")) return; // invalid separator
	o_seg = getInt(256);
	if (!o_seg) return; // invalid seg
	if (done()) return; // ok, ps + ss + seg
	// invalid terminator
}

std::string ParsedSlotCond::toTclExpression() const
{
	if (!hasPs) return rest;
	std::string result = strCat("[pc_in_slot ", ps);
	if (hasSs) {
		strAppend(result, ' ', ss);
	} else {
		if (hasSeg) strAppend(result, " X");
	}
	if (hasSeg) {
		strAppend(result, ' ', seg);
	}
	strAppend(result, ']');
	if (!rest.empty()) {
		strAppend(result, " && (", rest, ')');
	}
	return result;
}

std::string ParsedSlotCond::toDisplayString() const
{
	if (!hasPs) return rest;
	std::string result = strCat("Slot:", ps, '-');
	if (hasSs) {
		strAppend(result, ss);
	} else {
		strAppend(result, 'X');
	}
	if (hasSeg) {
		strAppend(result, ',', seg);
	}
	if (!rest.empty()) {
		strAppend(result, " && ", rest);
	}
	return result;
}

void ImGuiBreakPoints::paintTab(MSXCPUInterface& cpuInterface)
{
	int flags = ImGuiTableFlags_RowBg |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_Sortable |
		ImGuiTableFlags_Hideable |
		ImGuiTableFlags_Reorderable |
		ImGuiTableFlags_ContextMenuInBody |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_SizingStretchProp;
	im::Table("breakpoints", 4, flags, {-60, 0}, [&]{
		synchronize(cpuInterface);

		ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
		ImGui::TableSetupColumn("Enable", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupColumn("Condition");
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_DefaultHide);
		ImGui::TableHeadersRow();
		checkSort();

		int row = 0;
		for (auto& bp : guiBps) { // TODO c++23 enumerate
			im::ID(row, [&]{
				drawRow(cpuInterface, row, bp);
			});
			++row;
		}
	});
	ImGui::SameLine();
	im::Group([&] {
		if (ImGui::Button("Add")) {
			--idCounter;
			guiBps.push_back(GuiBreakPoint{
				idCounter, true, {}, {}, {}, makeTclList("debug", "break")});
			selectedBpRow = -1;
		}
		im::Disabled(selectedBpRow < 0 || selectedBpRow >= narrow<int>(guiBps.size()), [&]{
			if (ImGui::Button("Remove")) {
				auto it = guiBps.begin() + selectedBpRow;
				if (it->id > 0) {
					cpuInterface.removeBreakPoint(it->id);
				}
				guiBps.erase(it);
				selectedBpRow = -1;
			}
		});
	});
}

void ImGuiBreakPoints::synchronize(MSXCPUInterface& cpuInterface)
{
	// remove bp's that no longer exist on the openMSX side
	const auto& openMsxBps = cpuInterface.getBreakPoints();
	std::erase_if(guiBps, [&](auto& bp) {
		int id = bp.id;
		if (id < 0) return false; // keep disabled bp
		bool remove = !contains(openMsxBps, unsigned(id), &BreakPoint::getId);
		if (remove) {
			selectedBpRow = -1;
		}
		return remove;
	});
	for (const auto& bp : openMsxBps) {
		if (auto it = ranges::find(guiBps, narrow<int>(bp.getId()), &GuiBreakPoint::id);
			it != guiBps.end()) {
			// bp exists on the openMSX side, make sure it's in sync
			assert(it->addr);
			if (auto addr = bp.getAddress(); *it->addr != addr) {
				it->addr = addr;
				it->addrStr = tmpStrCat("0x", hex_string<4>(addr));
			}
			it->cond = bp.getConditionObj();
			it->cmd  = bp.getCommandObj();
		} else {
			// bp was added on the openMSX side, copy the GUI side
			auto addr = bp.getAddress();
			TclObject addrStr{tmpStrCat("0x", hex_string<4>(addr))};
			guiBps.push_back(GuiBreakPoint{
				narrow<int>(bp.getId()), true, addr, addrStr, bp.getConditionObj(), bp.getCommandObj()});
			selectedBpRow = -1;
		}
	}
}

void ImGuiBreakPoints::checkSort()
{
	auto* sortSpecs = ImGui::TableGetSortSpecs();
	if (!sortSpecs->SpecsDirty) return;

	sortSpecs->SpecsDirty = false;
	assert(sortSpecs->SpecsCount == 1);
	assert(sortSpecs->Specs);
	assert(sortSpecs->Specs->SortOrder == 0);
	auto sortUpDown = [&](auto proj) {
		if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Descending) {
			ranges::stable_sort(guiBps, std::greater<>{}, proj);
		} else {
			ranges::stable_sort(guiBps, std::less<>{}, proj);
		}
	};
	switch (sortSpecs->Specs->ColumnIndex) {
	case 1: // addr
		sortUpDown(&GuiBreakPoint::addr);
		break;
	case 2: // cond
		sortUpDown([](const auto& bp) { return bp.cond.getString(); });
		break;
	case 3: // action
		sortUpDown([](const auto& bp) { return bp.cmd.getString(); });
		break;
	default:
		UNREACHABLE;
	}
}

void ImGuiBreakPoints::drawRow(MSXCPUInterface& cpuInterface, int row, GuiBreakPoint& bp)
{
	auto& interp = manager.getInterpreter();
	const auto& style = ImGui::GetStyle();
	float rowHeight = 2.0f * style.FramePadding.y + ImGui::GetTextLineHeight();

	auto setRedBg = [](bool valid) {
		if (valid) return;
		ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0x400000ff);
	};
	auto isValidCond = [&](std::string_view cond) {
		return cond.empty() || interp.validExpression(cond);
	};
	auto isValidCmd = [&](std::string_view cmd) {
		return !cmd.empty() && interp.validCommand(cmd);
	};

	bool needSync = false;

	std::string addr{bp.addrStr.getString()};
	std::string cond{bp.cond.getString()};
	std::string cmd {bp.cmd .getString()};
	bool validAddr = bp.addr.has_value();
	bool validCond = isValidCond(cond);
	bool validCmd  = isValidCmd(cmd);

	if (ImGui::TableNextColumn()) { // enable
		auto pos = ImGui::GetCursorPos();
		if (ImGui::Selectable("##selection", selectedBpRow == row,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
				{0.0f, rowHeight})) {
			selectedBpRow = row;
		}
		ImGui::SetCursorPos(pos);
		im::Disabled(!validAddr || !validCond || !validCmd, [&]{
			if (ImGui::Checkbox("##enabled", &bp.wantEnable)) {
				needSync = true;
			}
			if (ImGui::IsItemActive()) selectedBpRow = row;
		});
	}
	if (ImGui::TableNextColumn()) { // address
		setRedBg(validAddr);
		im::ItemWidth(-FLT_MIN, [&]{
			if (ImGui::InputText("##addr", &addr)) {
				bp.addrStr = addr;
				auto newAddr = bp.addrStr.getOptionalInt();
				if (newAddr && ((*newAddr < 0) || (*newAddr > 0xffff))) {
					newAddr.reset();
				}
				bp.addr = newAddr;
				validAddr = bp.addr.has_value();
				needSync = true;
			}
			if (ImGui::IsItemActive()) selectedBpRow = row;
		});
	}
	if (ImGui::TableNextColumn()) { // condition
		setRedBg(validCond);
		im::ItemWidth(-FLT_MIN, [&]{
			ParsedSlotCond slot(cond);
			auto pos = ImGui::GetCursorPos();
			ImGui::TextUnformatted(slot.toDisplayString());
			ImGui::SetCursorPos(pos);
			if (ImGui::InvisibleButton("##cond-button", {-FLT_MIN, rowHeight})) {
				ImGui::OpenPopup("cond-popup");
			}
			if (ImGui::IsItemActive()) selectedBpRow = row;
			im::Popup("cond-popup", [&]{
				if (editCondition(slot)) {
					cond = slot.toTclExpression();
					validCond = isValidCond(cond);
					bp.cond = cond;
					needSync = true;
				}
			});
		});
	}
	if (ImGui::TableNextColumn()) { // action
		setRedBg(validCmd);
		im::ItemWidth(-FLT_MIN, [&]{
			if (ImGui::InputText("##cmd", &cmd)) {
				validCmd = isValidCmd(cmd);
				bp.cmd = cmd;
				needSync = true;
			}
			if (ImGui::IsItemActive()) selectedBpRow = row;
		});
	}
	if (needSync) {
		if (bp.id > 0) {
			// (temporarily) remove it from the openMSX side
			cpuInterface.removeBreakPoint(bp.id); // temp remove it
			bp.id = --idCounter;
		}
		if (bp.wantEnable && validAddr && validCond && validCmd) {
			// (re)create on the openMSX side
			BreakPoint newBp(*bp.addr, bp.cmd, bp.cond, false);
			bp.id = narrow<int>(newBp.getId());
			assert(bp.id > 0);
			cpuInterface.insertBreakPoint(std::move(newBp));
		}
	}
}

bool ImGuiBreakPoints::editCondition(ParsedSlotCond& slot)
{
	bool syncCond = false;
	ImGui::TextUnformatted("slot");
	im::Indent([&]{
		uint8_t one = 1;
		if (ImGui::Checkbox("primary  ", &slot.hasPs)) {
			syncCond = true;
		}
		ImGui::SameLine();
		im::Disabled(!slot.hasPs, [&]{
			if (ImGui::Combo("##ps", &slot.ps, "0\0001\0002\0003\000")) {
				syncCond = true;
			}

			if (ImGui::Checkbox("secondary", &slot.hasSs)) {
				syncCond = true;
			}
			ImGui::SameLine();
			im::Disabled(!slot.hasSs, [&]{
				if (ImGui::Combo("##ss", &slot.ss, "0\0001\0002\0003\000")) {
					syncCond = true;
				}
			});

			if (ImGui::Checkbox("segment  ", &slot.hasSeg)) {
				syncCond = true;
			}
			ImGui::SameLine();
			im::Disabled(!slot.hasSeg, [&]{
				if (ImGui::InputScalar("##seg", ImGuiDataType_U8, &slot.seg, &one)) {
					syncCond = true;
				}
			});
		});
	});
	ImGui::TextUnformatted("Tcl expression");
	im::Indent([&]{
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputText("##cond", &slot.rest)) {
			syncCond = true;
		}
	});
	return syncCond;
}

} // namespace openmsx
