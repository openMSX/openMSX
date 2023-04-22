#include "ImGuiDebugger.hh"

#include "ImGuiBitmapViewer.hh"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "BreakPoint.hh"
#include "CPURegs.hh"
#include "Dasm.hh"
#include "DebugCondition.hh"
#include "Debugger.hh"
#include "Interpreter.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXMemoryMapperBase.hh"
#include "MSXMotherBoard.hh"
#include "RomPlain.hh"
#include "WatchPoint.hh"

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
				paintTab<BreakPoint>(cpuInterface);
			});
			im::TabItem("Watchpoints", [&]{
				ImGui::TextUnformatted("TODO");
			});
			im::TabItem("Conditions", [&]{
				paintTab<DebugCondition>(cpuInterface);
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

template<typename Item> struct HasAddress : std::true_type {};
template<> struct HasAddress<DebugCondition> : std::false_type {};

template<typename Item> struct AllowEmptyCond : std::true_type {};
template<> struct AllowEmptyCond<DebugCondition> : std::false_type {};

static void remove(BreakPoint*, MSXCPUInterface& cpuInterface, unsigned id)
{
	cpuInterface.removeBreakPoint(id);
}
static void remove(DebugCondition*, MSXCPUInterface& cpuInterface, unsigned id)
{
	cpuInterface.removeCondition(id);
}

template<typename Item>
void ImGuiBreakPoints::paintTab(MSXCPUInterface& cpuInterface)
{
	constexpr bool hasAddress = HasAddress<Item>{};
	Item* tag = nullptr;
	auto& items = getItems(tag);

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
	im::Table("items", 4, flags, {-60, 0}, [&]{
		synchronize<Item>(items, cpuInterface);

		ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
		ImGui::TableSetupColumn("Enable", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
		int addressFlags = hasAddress ? ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_DefaultSort
		                              : ImGuiTableColumnFlags_Disabled;
		ImGui::TableSetupColumn("Address", addressFlags);
		ImGui::TableSetupColumn("Condition");
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_DefaultHide);
		ImGui::TableHeadersRow();

		checkSort(items);

		int row = 0;
		for (auto& item : items) { // TODO c++23 enumerate
			im::ID(row, [&]{
				drawRow<Item>(cpuInterface, row, item);
			});
			++row;
		}
	});
	ImGui::SameLine();
	im::Group([&] {
		if (ImGui::Button("Add")) {
			--idCounter;
			items.push_back(GuiItem{
				idCounter, true, {}, {}, {}, makeTclList("debug", "break")});
			selectedRow = -1;
		}
		im::Disabled(selectedRow < 0 || selectedRow >= narrow<int>(items.size()), [&]{
			if (ImGui::Button("Remove")) {
				auto it = items.begin() + selectedRow;
				if (it->id > 0) {
					remove(tag, cpuInterface, it->id);
				}
				items.erase(it);
				selectedRow = -1;
			}
		});
	});
}

static const std::vector<BreakPoint>& getOpenMSXItems(BreakPoint*, MSXCPUInterface& cpuInterface)
{
	return cpuInterface.getBreakPoints();
}
/*static const std::vector<WatchPoint>& getOpenMSXItems(WatchPoint*, MSXCPUInterface& cpuInterface)
{
	return cpuInterface.getWatchPoints();
}*/
static const std::vector<DebugCondition>& getOpenMSXItems(DebugCondition*, MSXCPUInterface& cpuInterface)
{
	return cpuInterface.getConditions();
}

[[nodiscard]] static unsigned getId(const BreakPoint& bp) { return bp.getId(); }
//[[nodiscard]] static unsigned getId(const WatchPoint& wp) { return wp.getId(); }
[[nodiscard]] static unsigned getId(const DebugCondition& cond) { return cond.getId(); }

template<typename Item>
void ImGuiBreakPoints::synchronize(std::vector<GuiItem>& items, MSXCPUInterface& cpuInterface)
{
	constexpr bool hasAddress = HasAddress<Item>{};
	Item* tag = nullptr;

	// remove items that no longer exist on the openMSX side
	const auto& openMsxItems = getOpenMSXItems(tag, cpuInterface);
	std::erase_if(items, [&](auto& item) {
		int id = item.id;
		if (id < 0) return false; // keep disabled bp
		bool remove = !contains(openMsxItems, unsigned(id), [](const auto& i) { return getId(i); });
		if (remove) {
			selectedRow = -1;
		}
		return remove;
	});
	for (const auto& item : openMsxItems) {
		if (auto it = ranges::find(items, narrow<int>(getId(item)), &GuiItem::id);
			it != items.end()) {
			// item exists on the openMSX side, make sure it's in sync
			if constexpr (hasAddress) {
				assert(it->addr);
				if (auto addr = item.getAddress(); *it->addr != addr) {
					it->addr = addr;
					it->addrStr = tmpStrCat("0x", hex_string<4>(addr));
				}
			} else {
				assert(!it->addr);
			}
			it->cond = item.getConditionObj();
			it->cmd  = item.getCommandObj();
		} else {
			// item was added on the openMSX side, copy to the GUI side
			std::optional<uint16_t> addr;
			TclObject addrStr;
			if constexpr (hasAddress) {
				addr = item.getAddress();
				addrStr = tmpStrCat("0x", hex_string<4>(*addr));
			}
			items.push_back(GuiItem{
				narrow<int>(item.getId()), true, addr, std::move(addrStr), item.getConditionObj(), item.getCommandObj()});
			selectedRow = -1;
		}
	}
}

void ImGuiBreakPoints::checkSort(std::vector<GuiItem>& items)
{
	auto* sortSpecs = ImGui::TableGetSortSpecs();
	if (!sortSpecs->SpecsDirty) return;

	sortSpecs->SpecsDirty = false;
	assert(sortSpecs->SpecsCount == 1);
	assert(sortSpecs->Specs);
	assert(sortSpecs->Specs->SortOrder == 0);

	auto sortUpDown = [&](auto proj) {
		if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Descending) {
			ranges::stable_sort(items, std::greater<>{}, proj);
		} else {
			ranges::stable_sort(items, std::less<>{}, proj);
		}
	};
	switch (sortSpecs->Specs->ColumnIndex) {
	case 1: // addr
		sortUpDown(&GuiItem::addr);
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

static void create(BreakPoint*, MSXCPUInterface& cpuInterface, ImGuiBreakPoints::GuiItem& item)
{
	BreakPoint newBp(*item.addr, item.cmd, item.cond, false);
	item.id = narrow<int>(newBp.getId());
	cpuInterface.insertBreakPoint(std::move(newBp));
}
static void create(DebugCondition*, MSXCPUInterface& cpuInterface, ImGuiBreakPoints::GuiItem& item)
{
	DebugCondition newCond(item.cmd, item.cond, false);
	item.id = narrow<int>(newCond.getId());
	cpuInterface.setCondition(std::move(newCond));
}

template<typename Item>
void ImGuiBreakPoints::drawRow(MSXCPUInterface& cpuInterface, int row, GuiItem& item)
{
	constexpr bool hasAddress = HasAddress<Item>{};
	Item* tag = nullptr;

	auto& interp = manager.getInterpreter();
	const auto& style = ImGui::GetStyle();
	float rowHeight = 2.0f * style.FramePadding.y + ImGui::GetTextLineHeight();

	auto setRedBg = [](bool valid) {
		if (valid) return;
		ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0x400000ff);
	};
	auto isValidCond = [&](std::string_view cond) -> bool {
		if (cond.empty()) return AllowEmptyCond<Item>{};
		return interp.validExpression(cond);
	};
	auto isValidCmd = [&](std::string_view cmd) {
		return !cmd.empty() && interp.validCommand(cmd);
	};

	bool needSync = false;

	std::string addr{item.addrStr.getString()};
	std::string cond{item.cond.getString()};
	std::string cmd {item.cmd .getString()};
	bool validAddr = !hasAddress || item.addr.has_value();
	bool validCond = isValidCond(cond);
	bool validCmd  = isValidCmd(cmd);

	if (ImGui::TableNextColumn()) { // enable
		auto pos = ImGui::GetCursorPos();
		if (ImGui::Selectable("##selection", selectedRow == row,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
				{0.0f, rowHeight})) {
			selectedRow = row;
		}
		ImGui::SetCursorPos(pos);
		im::Disabled(!validAddr || !validCond || !validCmd, [&]{
			if (ImGui::Checkbox("##enabled", &item.wantEnable)) {
				needSync = true;
			}
			if (ImGui::IsItemActive()) selectedRow = row;
		});
	}
	if (ImGui::TableNextColumn()) { // address
		setRedBg(validAddr);
		im::ItemWidth(-FLT_MIN, [&]{
			if (ImGui::InputText("##addr", &addr)) {
				item.addrStr = addr;
				auto newAddr = item.addrStr.getOptionalInt();
				if (newAddr && ((*newAddr < 0) || (*newAddr > 0xffff))) {
					newAddr.reset();
				}
				item.addr = newAddr;
				validAddr = item.addr.has_value();
				needSync = true;
			}
			if (ImGui::IsItemActive()) selectedRow = row;
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
			if (ImGui::IsItemActive()) selectedRow = row;
			im::Popup("cond-popup", [&]{
				if (editCondition(slot)) {
					cond = slot.toTclExpression();
					validCond = isValidCond(cond);
					item.cond = cond;
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
				item.cmd = cmd;
				needSync = true;
			}
			if (ImGui::IsItemActive()) selectedRow = row;
		});
	}
	if (needSync) {
		if (item.id > 0) {
			// (temporarily) remove it from the openMSX side
			remove(tag, cpuInterface, item.id); // temp remove it
			item.id = --idCounter;
		}
		if (item.wantEnable && validAddr && validCond && validCmd) {
			// (re)create on the openMSX side
			create(tag, cpuInterface, item);
			assert(item.id > 0);
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
