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
#include "Reactor.hh"
#include "RomPlain.hh"
#include "WatchPoint.hh"

#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "stl.hh"
#include "strCat.hh"
#include "unreachable.hh"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include <cstdint>
#include <tuple>
#include <vector>

using namespace std::literals;


namespace openmsx {

ImGuiBreakPoints::ImGuiBreakPoints(ImGuiManager& manager_)
	: manager(manager_)
	, symbolManager(manager.getReactor().getSymbolManager())
{
}

static void saveItems(zstring_view label, const std::vector<ImGuiBreakPoints::GuiItem>& items, ImGuiTextBuffer& buf)
{
	auto saveAddr = [](std::optional<uint16_t> addr) {
		return addr ? TclObject(*addr) : TclObject();
	};
	for (const auto& item : items) {
		auto list = makeTclList(
			item.wantEnable,
			item.wpType,
			saveAddr(item.addr),
			saveAddr(item.endAddr),
			item.addrStr,
			item.endAddrStr,
			item.cond,
			item.cmd);
		buf.appendf("%s=%s\n", label.c_str(), list.getString().c_str());
	}
}

void ImGuiBreakPoints::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	saveItems("breakpoint", guiBps, buf);
	saveItems("watchpoint", guiWps, buf);
	saveItems("condition", guiConditions, buf);
}

template<typename Item>
void ImGuiBreakPoints::loadItem(zstring_view value)
{
	auto& interp = manager.getInterpreter();

	auto loadAddr = [&](const TclObject& o) -> std::optional<uint16_t> {
		if (o.getString().empty()) return {};
		return o.getInt(interp);
	};

	try {
		TclObject list(value);
		if (list.getListLength(interp) != 8) return; // ignore
		GuiItem item {
			.id = --idCounter,
			.wantEnable = list.getListIndex(interp, 0).getBoolean(interp),
			.wpType     = list.getListIndex(interp, 1).getInt(interp),
			.addr    = loadAddr(list.getListIndex(interp, 2)),
			.endAddr = loadAddr(list.getListIndex(interp, 3)),
			.addrStr    = list.getListIndex(interp, 4),
			.endAddrStr = list.getListIndex(interp, 5),
			.cond       = list.getListIndex(interp, 6),
			.cmd        = list.getListIndex(interp, 7),
		};
		if (item.wpType < 0 || item.wpType > 3) return;

		Item* tag = nullptr;
		auto& items = getItems(tag);
		items.push_back(std::move(item));

		if (auto* motherBoard = manager.getReactor().getMotherBoard()) {
			auto& cpuInterface = motherBoard->getCPUInterface();
			auto& debugger = motherBoard->getDebugger();
			syncToOpenMsx<Item>(cpuInterface, debugger, interp, items.back());
		}
	} catch (CommandException&) {
		// ignore
	}
}

void ImGuiBreakPoints::loadLine(std::string_view name, zstring_view value)
{
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name == "breakpoint"sv) {
		loadItem<BreakPoint>(value);
	} else if (name == "watchpoint"sv) {
		loadItem<WatchPoint>(value);
	} else if (name == "condition"sv) {
		loadItem<DebugCondition>(value);
	}
}

void ImGuiBreakPoints::loadEnd()
{
	refreshSymbols(); // after both breakpoints and symbols have been loaded
}

void ImGuiBreakPoints::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	ImGui::SetNextWindowSize(gl::vec2{25, 14} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Breakpoints", &show, [&]{
		im::TabBar("tabs", [&]{
			auto& cpuInterface = motherBoard->getCPUInterface();
			auto& debugger = motherBoard->getDebugger();
			im::TabItem("Breakpoints", [&]{
				paintTab<BreakPoint>(cpuInterface, debugger);
			});
			im::TabItem("Watchpoints", [&]{
				paintTab<WatchPoint>(cpuInterface, debugger);
			});
			im::TabItem("Conditions", [&]{
				paintTab<DebugCondition>(cpuInterface, debugger);
			});
		});
	});
}

static std::string_view getCheckCmd(BreakPoint*)     { return "pc_in_slot"; }
static std::string_view getCheckCmd(WatchPoint*)     { return "watch_in_slot"; }
static std::string_view getCheckCmd(DebugCondition*) { return "pc_in_slot"; }

ParsedSlotCond::ParsedSlotCond(std::string_view checkCmd, std::string_view cond)
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

	if (!next(tmpStrCat('[', checkCmd, ' '))) return; // no slot
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

std::string ParsedSlotCond::toTclExpression(std::string_view checkCmd) const
{
	if (!hasPs) return rest;
	std::string result = strCat('[', checkCmd, ' ', ps);
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
static void remove(WatchPoint*, MSXCPUInterface& cpuInterface, unsigned id)
{
	cpuInterface.removeWatchPoint(id);
}
static void remove(DebugCondition*, MSXCPUInterface& cpuInterface, unsigned id)
{
	cpuInterface.removeCondition(id);
}

template<typename Item>
void ImGuiBreakPoints::paintTab(MSXCPUInterface& cpuInterface, Debugger& debugger)
{
	constexpr bool hasAddress = HasAddress<Item>{};
	constexpr bool isWatchPoint = std::is_same_v<Item, WatchPoint>;
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
	im::Table("items", 5, flags, {-60, 0}, [&]{
		syncFromOpenMsx<Item>(items, cpuInterface);

		ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
		ImGui::TableSetupColumn("Enable", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
		int typeFlags = isWatchPoint ? ImGuiTableColumnFlags_NoHide : ImGuiTableColumnFlags_Disabled;
		ImGui::TableSetupColumn("Type", typeFlags);
		int addressFlags = hasAddress ? ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_DefaultSort
		                              : ImGuiTableColumnFlags_Disabled;
		ImGui::TableSetupColumn("Address", addressFlags);
		ImGui::TableSetupColumn("Condition");
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_DefaultHide);
		ImGui::TableHeadersRow();

		checkSort(items);

		im::ID_for_range(items.size(), [&](int row) {
			drawRow<Item>(cpuInterface, debugger, row, items[row]);
		});
	});
	ImGui::SameLine();
	im::Group([&] {
		if (ImGui::Button("Add")) {
			--idCounter;
			items.push_back(GuiItem{
				idCounter,
				true, // enabled, but still invalid
				WatchPoint::WRITE_MEM,
				{}, {}, {}, {}, // address
				{}, // cond
				makeTclList("debug", "break")}); // cmd
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
static const std::vector<std::shared_ptr<WatchPoint>>& getOpenMSXItems(WatchPoint*, MSXCPUInterface& cpuInterface)
{
	return cpuInterface.getWatchPoints();
}
static const std::vector<DebugCondition>& getOpenMSXItems(DebugCondition*, MSXCPUInterface& cpuInterface)
{
	return cpuInterface.getConditions();
}

[[nodiscard]] static unsigned getId(const BreakPoint& bp) { return bp.getId(); }
[[nodiscard]] static unsigned getId(const std::shared_ptr<WatchPoint>& wp) { return wp->getId(); }
[[nodiscard]] static unsigned getId(const DebugCondition& cond) { return cond.getId(); }

[[nodiscard]] static unsigned getAddress(const BreakPoint& bp) { return bp.getAddress(); }
[[nodiscard]] static unsigned getAddress(const std::shared_ptr<WatchPoint>& wp) { return wp->getBeginAddress(); }
[[nodiscard]] static unsigned getAddress(const DebugCondition& cond) = delete;

[[nodiscard]] static unsigned getEndAddress(const BreakPoint& bp) { return bp.getAddress(); } // same as begin
[[nodiscard]] static unsigned getEndAddress(const std::shared_ptr<WatchPoint>& wp) { return wp->getEndAddress(); }
[[nodiscard]] static unsigned getEndAddress(const DebugCondition& cond) = delete;

[[nodiscard]] static TclObject getConditionObj(const BreakPointBase& bp) { return bp.getConditionObj(); }
[[nodiscard]] static TclObject getConditionObj(const std::shared_ptr<WatchPoint>& wp) { return wp->getConditionObj(); }

[[nodiscard]] static TclObject getCommandObj(const BreakPointBase& bp) { return bp.getCommandObj(); }
[[nodiscard]] static TclObject getCommandObj(const std::shared_ptr<WatchPoint>& wp) { return wp->getCommandObj(); }



template<typename Item>
void ImGuiBreakPoints::syncFromOpenMsx(std::vector<GuiItem>& items, MSXCPUInterface& cpuInterface)
{
	constexpr bool hasAddress = HasAddress<Item>{};
	constexpr bool isWatchPoint = std::is_same_v<Item, WatchPoint>;
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
		auto formatAddr = [&](uint16_t addr) {
			if (auto syms = symbolManager.lookupValue(addr); !syms.empty()) {
				return TclObject(syms.front()->name);
			}
			return TclObject(tmpStrCat("0x", hex_string<4>(addr)));
		};
		if (auto it = ranges::find(items, narrow<int>(getId(item)), &GuiItem::id);
			it != items.end()) {
			// item exists on the openMSX side, make sure it's in sync
			if constexpr (isWatchPoint) {
				it->wpType = item->getType();
			}
			if constexpr (hasAddress) {
				assert(it->addr);
				auto addr = getAddress(item);
				auto endAddr = getEndAddress(item);
				bool needUpdate =
					(*it->addr != addr) ||
					(it->endAddr && (it->endAddr != endAddr)) ||
					(!it->endAddr && (addr != endAddr));
				if (needUpdate) {
					it->addr = addr;
					it->endAddr = (addr != endAddr) ? std::optional<uint16_t>(endAddr) : std::nullopt;
					it->addrStr = formatAddr(addr);
					it->endAddrStr = (addr != endAddr) ? formatAddr(endAddr) : TclObject{};
				}
			} else {
				assert(!it->addr);
			}
			it->cond = getConditionObj(item);
			it->cmd  = getCommandObj(item);
		} else {
			// item was added on the openMSX side, copy to the GUI side
			WatchPoint::Type wpType = WatchPoint::WRITE_MEM;
			std::optional<uint16_t> addr;
			std::optional<uint16_t> endAddr;
			TclObject addrStr;
			TclObject endAddrStr;
			if constexpr (isWatchPoint) {
				wpType = item->getType();
			}
			if constexpr (hasAddress) {
				addr = getAddress(item);
				endAddr = getEndAddress(item);
				if (*addr == *endAddr) endAddr.reset();
				addrStr = formatAddr(*addr);
				if (endAddr) endAddrStr = formatAddr(*endAddr);
			}
			items.push_back(GuiItem{
				narrow<int>(getId(item)),
				true,
				wpType,
				addr, endAddr, std::move(addrStr), std::move(endAddrStr),
				getConditionObj(item), getCommandObj(item)});
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

	switch (sortSpecs->Specs->ColumnIndex) {
	case 1: // addr
		sortUpDown_T(items, sortSpecs, &GuiItem::wpType);
		break;
	case 2: // addr
		sortUpDown_T(items, sortSpecs, [](const auto& item) {
			return std::tuple(item.addr, item.endAddr,
			                  item.addrStr.getString(),
			                  item.endAddrStr.getString());
		});
		break;
	case 3: // cond
		sortUpDown_String(items, sortSpecs, [](const auto& item) { return item.cond.getString(); });
		break;
	case 4: // action
		sortUpDown_String(items, sortSpecs, [](const auto& item) { return item.cmd.getString(); });
		break;
	default:
		UNREACHABLE;
	}
}

std::optional<uint16_t> ImGuiBreakPoints::parseAddress(const TclObject& o)
{
	return symbolManager.parseSymbolOrValue(o.getString());
}

template<typename Item>
static bool isValidAddr(const ImGuiBreakPoints::GuiItem& i)
{
	constexpr bool hasAddress = HasAddress<Item>{};
	constexpr bool isWatchPoint = std::is_same_v<Item, WatchPoint>;

	if (!hasAddress) return true;
	if (!i.addr) return false;
	if (isWatchPoint) {
		if (i.endAddr && (*i.endAddr < *i.addr)) return false;
		if ((i.wpType == one_of(WatchPoint::READ_IO, WatchPoint::WRITE_IO)) &&
			((*i.addr >= 0x100) || (i.endAddr && (*i.endAddr >= 0x100)))) {
			return false;
		}
	}
	return true;
}

template<typename Item>
static bool isValidCond(std::string_view cond, Interpreter& interp)
{
	if (cond.empty()) return AllowEmptyCond<Item>{};
	return interp.validExpression(cond);
}

static bool isValidCmd(std::string_view cmd, Interpreter& interp)
{
	return !cmd.empty() && interp.validCommand(cmd);
}

static void create(BreakPoint*, MSXCPUInterface& cpuInterface, Debugger&, ImGuiBreakPoints::GuiItem& item)
{
	BreakPoint newBp(*item.addr, item.cmd, item.cond, false);
	item.id = narrow<int>(newBp.getId());
	cpuInterface.insertBreakPoint(std::move(newBp));
}
static void create(WatchPoint*, MSXCPUInterface&, Debugger& debugger, ImGuiBreakPoints::GuiItem& item)
{
	item.id = debugger.setWatchPoint(
		item.cmd, item.cond,
		static_cast<WatchPoint::Type>(item.wpType),
		*item.addr, (item.endAddr ? *item.endAddr : *item.addr),
		false);
}
static void create(DebugCondition*, MSXCPUInterface& cpuInterface, Debugger&, ImGuiBreakPoints::GuiItem& item)
{
	DebugCondition newCond(item.cmd, item.cond, false);
	item.id = narrow<int>(newCond.getId());
	cpuInterface.setCondition(std::move(newCond));
}

template<typename Item>
void ImGuiBreakPoints::syncToOpenMsx(
	MSXCPUInterface& cpuInterface, Debugger& debugger,
	Interpreter& interp, GuiItem& item)
{
	Item* tag = nullptr;

	if (item.id > 0) {
		// (temporarily) remove it from the openMSX side
		remove(tag, cpuInterface, item.id); // temp remove it
		item.id = --idCounter;
	}
	if (item.wantEnable &&
	    isValidAddr<Item>(item) &&
	    isValidCond<Item>(item.cond.getString(), interp) &&
	    isValidCmd(item.cmd.getString(), interp)) {
		// (re)create on the openMSX side
		create(tag, cpuInterface, debugger, item);
		assert(item.id > 0);
	}
}

template<typename Item>
void ImGuiBreakPoints::drawRow(MSXCPUInterface& cpuInterface, Debugger& debugger, int row, GuiItem& item)
{
	constexpr bool isWatchPoint = std::is_same_v<Item, WatchPoint>;
	Item* tag = nullptr;

	auto& interp = manager.getInterpreter();
	const auto& style = ImGui::GetStyle();
	float rowHeight = 2.0f * style.FramePadding.y + ImGui::GetTextLineHeight();

	auto setRedBg = [](bool valid) {
		if (valid) return;
		ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, getColor(imColor::RED_BG));
	};

	bool needSync = false;
	std::string cond{item.cond.getString()};
	std::string cmd {item.cmd .getString()};
	bool validAddr = isValidAddr<Item>(item);
	bool validCond = isValidCond<Item>(cond, interp);
	bool validCmd  = isValidCmd(cmd, interp);

	if (ImGui::TableNextColumn()) { // enable
		auto pos = ImGui::GetCursorPos();
		if (ImGui::Selectable("##selection", selectedRow == row,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
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
	if (ImGui::TableNextColumn()) { // type
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::Combo("##type", &item.wpType, "read IO\000write IO\000read memory\000write memory\000")) {
			validAddr = isValidAddr<Item>(item);
			needSync = true;
		}
		if (ImGui::IsItemActive()) selectedRow = row;
	}
	if (ImGui::TableNextColumn()) { // address
		auto addrToolTip = [&]{
			if (!item.addr) return;
			simpleToolTip([&]{
				auto tip = strCat("0x", hex_string<4>(*item.addr));
				if (item.endAddr) {
					strAppend(tip, "...0x", hex_string<4>(*item.endAddr));
				}
				return tip;
			});
		};
		setRedBg(validAddr);
		bool addrChanged = false;
		std::string addr{item.addrStr.getString()};
		std::string endAddr{item.endAddrStr.getString()};
		ImGui::SetNextItemWidth(-FLT_MIN);
		if constexpr (isWatchPoint) {
			auto pos = ImGui::GetCursorPos();
			std::string displayAddr = addr;
			if (!endAddr.empty()) {
				strAppend(displayAddr, "...", endAddr);
			}
			ImGui::TextUnformatted(displayAddr);
			addrToolTip();
			ImGui::SetCursorPos(pos);
			if (ImGui::InvisibleButton("##range-button", {-FLT_MIN, rowHeight})) {
				ImGui::OpenPopup("range-popup");
			}
			if (ImGui::IsItemActive()) selectedRow = row;
			im::Popup("range-popup", [&]{
				addrChanged |= editRange(addr, endAddr);
			});
		} else {
			assert(endAddr.empty());
			addrChanged |= ImGui::InputText("##addr", &addr);
			addrToolTip();
			if (ImGui::IsItemActive()) selectedRow = row;
		}
		if (addrChanged) {
			item.addrStr = addr;
			item.endAddrStr = endAddr;
			item.addr    = parseAddress(item.addrStr);
			item.endAddr = parseAddress(item.endAddrStr);
			if (item.endAddr && !item.addr) item.endAddr.reset();
			needSync = true;
		}
	}
	if (ImGui::TableNextColumn()) { // condition
		setRedBg(validCond);
		auto checkCmd = getCheckCmd(tag);
		ParsedSlotCond slot(checkCmd, cond);
		auto pos = ImGui::GetCursorPos();
		ImGui::TextUnformatted(slot.toDisplayString());
		ImGui::SetCursorPos(pos);
		if (ImGui::InvisibleButton("##cond-button", {-FLT_MIN, rowHeight})) {
			ImGui::OpenPopup("cond-popup");
		}
		if (ImGui::IsItemActive()) selectedRow = row;
		im::Popup("cond-popup", [&]{
			if (editCondition(slot)) {
				cond = slot.toTclExpression(checkCmd);
				item.cond = cond;
				needSync = true;
			}
		});
	}
	if (ImGui::TableNextColumn()) { // action
		setRedBg(validCmd);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputText("##cmd", &cmd)) {
			item.cmd = cmd;
			needSync = true;
		}
		if (ImGui::IsItemActive()) selectedRow = row;
	}
	if (needSync) {
		syncToOpenMsx<Item>(cpuInterface, debugger, interp, item);
	}
}

bool ImGuiBreakPoints::editRange(std::string& begin, std::string& end)
{
	bool changed = false;
	ImGui::TextUnformatted("address range"sv);
	im::Indent([&]{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("begin:  "sv);
		ImGui::SameLine();
		changed |= ImGui::InputText("##begin", &begin);

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("end:"sv);
		HelpMarker("End address is included in the range.\n"
		           "Leave empty for a single address.");
		ImGui::SameLine();
		changed |= ImGui::InputText("##end", &end);
	});
	return changed;
}

bool ImGuiBreakPoints::editCondition(ParsedSlotCond& slot)
{
	bool changed = false;
	ImGui::TextUnformatted("slot"sv);
	im::Indent([&]{
		uint8_t one = 1;
		changed |= ImGui::Checkbox("primary  ", &slot.hasPs);
		ImGui::SameLine();
		im::Disabled(!slot.hasPs, [&]{
			changed |= ImGui::Combo("##ps", &slot.ps, "0\0001\0002\0003\000");

			changed |= ImGui::Checkbox("secondary", &slot.hasSs);
			ImGui::SameLine();
			im::Disabled(!slot.hasSs, [&]{
				changed |= ImGui::Combo("##ss", &slot.ss, "0\0001\0002\0003\000");
			});

			changed |= ImGui::Checkbox("segment  ", &slot.hasSeg);
			ImGui::SameLine();
			im::Disabled(!slot.hasSeg, [&]{
				changed |= ImGui::InputScalar("##seg", ImGuiDataType_U8, &slot.seg, &one);
			});
		});
	});
	ImGui::TextUnformatted("Tcl expression"sv);
	im::Indent([&]{
		ImGui::SetNextItemWidth(-FLT_MIN);
		changed |= ImGui::InputText("##cond", &slot.rest);
	});
	return changed;
}

void ImGuiBreakPoints::refreshSymbols()
{
	refresh<BreakPoint>(guiBps);
	refresh<WatchPoint>(guiWps);
	//refresh<DebugCondition>(guiConditions); // not needed, doesn't have an address
}

template<typename Item>
void ImGuiBreakPoints::refresh(std::vector<GuiItem>& items)
{
	assert(HasAddress<Item>::value);
	constexpr bool isWatchPoint = std::is_same_v<Item, WatchPoint>;

	auto* motherBoard = manager.getReactor().getMotherBoard();
	if (!motherBoard) return;

	for (auto& item : items) {
		bool sync = false;
		auto adjust = [&](std::optional<uint16_t>& addr, TclObject& str) {
			if (addr) {
				// was valid
				if (auto newAddr = parseAddress(str)) {
					// and is still valid
					if (*newAddr != *addr) {
						// but the value changed
						addr = newAddr;
						sync = true;
					} else {
						// heuristic: try to replace strings of the form "0x...." with a symbol name
						auto s = str.getString();
						if ((s.size() == 6) && s.starts_with("0x")) {
							if (auto newSyms = symbolManager.lookupValue(*addr); !newSyms.empty()) {
								str = newSyms.front()->name;
								// no need to sync with openMSX
							}
						}
					}
				} else {
					// but now no longer valid, revert to hex string
					str = TclObject(tmpStrCat("0x", hex_string<4>(*addr)));
					// no need to sync with openMSX
				}
			} else {
				// was invalid, (re-)try to resolve symbol
				if (auto newAddr = parseAddress(str)) {
					// and now became valid
					addr = newAddr;
					sync = true;
				}
			}
		};
		adjust(item.addr, item.addrStr);
		if (isWatchPoint) {
			adjust(item.endAddr, item.endAddrStr);
		}
		if (sync) {
			auto& cpuInterface = motherBoard->getCPUInterface();
			auto& debugger = motherBoard->getDebugger();
			auto& interp = manager.getInterpreter();
			syncToOpenMsx<Item>(cpuInterface, debugger, interp, item);
		}
	}
}

} // namespace openmsx
