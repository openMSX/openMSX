#include "ImGuiBreakPoints.hh"

#include "ImGuiCpp.hh"
#include "ImGuiDebugger.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "BreakPoint.hh"
#include "DebugCondition.hh"
#include "Debugger.hh"
#include "Interpreter.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "SymbolManager.hh"
#include "WatchPoint.hh"

#include "narrow.hh"
#include "one_of.hh"
#include "strCat.hh"
#include "unreachable.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cstdint>
#include <tuple>
#include <vector>

using namespace std::literals;

namespace openmsx {

ImGuiBreakPoints::ImGuiBreakPoints(ImGuiManager& manager_)
	: ImGuiPart(manager_)
	, symbolManager(manager.getReactor().getSymbolManager())
{
}

void ImGuiBreakPoints::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiBreakPoints::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiBreakPoints::paint(MSXMotherBoard* motherBoard)
{
	if (!motherBoard || !show) return;

	ImGui::SetNextWindowSize(gl::vec2{25, 14} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Breakpoints", &show, [&]{
		im::TabBar("tabs", [&]{
			auto& cpuInterface = motherBoard->getCPUInterface();
			im::TabItem("Breakpoints", [&]{
				paintTab<BreakPoint>(cpuInterface);
			});
			im::TabItem("Watchpoints", [&]{
				paintTab<WatchPoint>(cpuInterface);
			});
			im::TabItem("Conditions", [&]{
				paintTab<DebugCondition>(cpuInterface);
			});
		});
	});
}

template<typename T> static std::string_view getCheckCmd();
template<> std::string_view getCheckCmd<BreakPoint>()     { return "pc_in_slot"; }
template<> std::string_view getCheckCmd<WatchPoint>()     { return "watch_in_slot"; }
template<> std::string_view getCheckCmd<DebugCondition>() { return "pc_in_slot"; }

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

template<typename> struct BaseBpType;
template<> struct BaseBpType<BreakPoint>                  { using type = BreakPoint; };
template<> struct BaseBpType<std::shared_ptr<WatchPoint>> { using type = WatchPoint; };
template<> struct BaseBpType<DebugCondition>              { using type = DebugCondition; };
template<typename T> using BaseBpType_t = typename BaseBpType<T>::type;

template<typename Item> struct HasAddress : std::true_type {};
template<> struct HasAddress<DebugCondition> : std::false_type {};

template<typename Item> struct AllowEmptyCond : std::true_type {};
template<> struct AllowEmptyCond<DebugCondition> : std::false_type {};

static std::vector<BreakPoint>& getItems(BreakPoint*, MSXCPUInterface&)
{
	return MSXCPUInterface::getBreakPoints();
}
static std::vector<std::shared_ptr<WatchPoint>>& getItems(WatchPoint*, MSXCPUInterface& cpuInterface)
{
	return cpuInterface.getWatchPoints();
}
static std::vector<DebugCondition>& getItems(DebugCondition*, MSXCPUInterface&)
{
	return MSXCPUInterface::getConditions();
}

template<typename T> static void createNew(MSXCPUInterface& cpuInterface, Interpreter& interp, std::optional<uint16_t> addr);
template<> void createNew<BreakPoint>(MSXCPUInterface& cpuInterface, Interpreter& interp, std::optional<uint16_t> addr)
{
	BreakPoint bp;
	if (addr) {
		bp.setAddress(interp, TclObject(tmpStrCat("0x", hex_string<4>(*addr))));
	}
	cpuInterface.insertBreakPoint(std::move(bp));
}
template<> void createNew<WatchPoint>(MSXCPUInterface& cpuInterface, Interpreter&, std::optional<uint16_t>)
{
	cpuInterface.setWatchPoint(std::make_shared<WatchPoint>());
}
template<> void createNew<DebugCondition>(MSXCPUInterface& cpuInterface, Interpreter&, std::optional<uint16_t>)
{
	cpuInterface.setCondition(DebugCondition());
}

template<typename T> static void remove(MSXCPUInterface& cpuInterface, unsigned id);
template<> void remove<BreakPoint>(MSXCPUInterface& cpuInterface, unsigned id) {
	cpuInterface.removeBreakPoint(id);
}
template<> void remove<WatchPoint>(MSXCPUInterface& cpuInterface, unsigned id) {
	cpuInterface.removeWatchPoint(id);
}
template<> void remove<DebugCondition>(MSXCPUInterface& cpuInterface, unsigned id) {
	cpuInterface.removeCondition(id);
}

[[nodiscard]] static unsigned getId(const BreakPoint& bp)                  { return bp.getId(); }
[[nodiscard]] static unsigned getId(const std::shared_ptr<WatchPoint>& wp) { return wp->getId(); }
[[nodiscard]] static unsigned getId(const DebugCondition& cond)            { return cond.getId(); }

[[nodiscard]] static bool getEnabled(const BreakPoint& bp)                  { return bp.isEnabled(); }
[[nodiscard]] static bool getEnabled(const std::shared_ptr<WatchPoint>& wp) { return wp->isEnabled(); }
[[nodiscard]] static bool getEnabled(const DebugCondition& cond)            { return cond.isEnabled(); }
static void setEnabled(BreakPoint& bp,                  bool e) { bp.setEnabled(e); }
static void setEnabled(std::shared_ptr<WatchPoint>& wp, bool e) { wp->setEnabled(e); }
static void setEnabled(DebugCondition& cond,            bool e) { cond.setEnabled(e); }

[[nodiscard]] static std::optional<uint16_t> getAddress(const BreakPoint& bp)                  { return bp.getAddress(); }
[[nodiscard]] static std::optional<uint16_t> getAddress(const std::shared_ptr<WatchPoint>& wp) { return wp->getBeginAddress(); }
[[nodiscard]] static std::optional<uint16_t> getAddress(const DebugCondition&)                 { return {}; }

[[nodiscard]] static TclObject getAddressString(const BreakPoint& bp)                  { return bp.getAddressString(); }
[[nodiscard]] static TclObject getAddressString(const std::shared_ptr<WatchPoint>& wp) { return wp->getBeginAddressString(); }
[[nodiscard]] static TclObject getAddressString(const DebugCondition&)                 { return {}; }

[[nodiscard]] static TclObject getEndAddressString(const BreakPoint&)                     { return {}; }
[[nodiscard]] static TclObject getEndAddressString(const std::shared_ptr<WatchPoint>& wp) { return wp->getEndAddressString(); }

[[nodiscard]] static TclObject getCondition(const BreakPoint& bp)                  { return bp.getCondition(); }
[[nodiscard]] static TclObject getCondition(const std::shared_ptr<WatchPoint>& wp) { return wp->getCondition(); }
[[nodiscard]] static TclObject getCondition(const DebugCondition& cond)            { return cond.getCondition(); }
static void setCondition(BreakPoint& bp,                  const TclObject& c) { bp.setCondition(c); }
static void setCondition(std::shared_ptr<WatchPoint>& wp, const TclObject& c) { wp->setCondition(c); }
static void setCondition(DebugCondition& cond,            const TclObject& c) { cond.setCondition(c); }

[[nodiscard]] static TclObject getCommand(const BreakPoint& bp)                  { return bp.getCommand(); }
[[nodiscard]] static TclObject getCommand(const std::shared_ptr<WatchPoint>& wp) { return wp->getCommand(); }
[[nodiscard]] static TclObject getCommand(const DebugCondition& cond)            { return cond.getCommand(); }
static void setCommand(BreakPoint& bp,                  const TclObject& c) { bp.setCommand(c); }
static void setCommand(std::shared_ptr<WatchPoint>& wp, const TclObject& c) { wp->setCommand(c); }
static void setCommand(DebugCondition& cond,            const TclObject& c) { cond.setCommand(c); }

[[nodiscard]] static bool getOnce(const BreakPoint& bp)                  { return bp.onlyOnce(); }
[[nodiscard]] static bool getOnce(const std::shared_ptr<WatchPoint>& wp) { return wp->onlyOnce(); }
[[nodiscard]] static bool getOnce(const DebugCondition& cond)            { return cond.onlyOnce(); }
static void setOnce(BreakPoint& bp,                  bool o) { bp.setOnce(o); }
static void setOnce(std::shared_ptr<WatchPoint>& wp, bool o) { wp->setOnce(o); }
static void setOnce(DebugCondition& cond,            bool o) { cond.setOnce(o); }

struct DummyScopedChange {};
[[nodiscard]] static DummyScopedChange getScopedChange(BreakPoint&, MSXCPUInterface&) { return {}; }
[[nodiscard]] static auto getScopedChange(std::shared_ptr<WatchPoint>& wp, MSXCPUInterface& cpuInterface) {
	return cpuInterface.getScopedChangeWatchpoint(wp);
}
[[nodiscard]] static DummyScopedChange getScopedChange(DebugCondition&, MSXCPUInterface&) { return {}; }

template<typename Item>
static void checkSort(std::vector<Item>& items)
{
	using Type = BaseBpType_t<Item>;
	constexpr bool isWatchPoint = std::is_same_v<Type, WatchPoint>;
	constexpr bool hasAddress = HasAddress<Type>{};

	auto* sortSpecs = ImGui::TableGetSortSpecs();
	if (!sortSpecs->SpecsDirty) return;

	sortSpecs->SpecsDirty = false;
	assert(sortSpecs->SpecsCount == 1);
	assert(sortSpecs->Specs);
	assert(sortSpecs->Specs->SortOrder == 0);

	switch (sortSpecs->Specs->ColumnIndex) {
	case 0: // enable
		sortUpDown_T(items, sortSpecs, [](const auto& item) { return getEnabled(item); });
		break;
	case 1: // type
		if constexpr (isWatchPoint) {
			sortUpDown_T(items, sortSpecs, &WatchPoint::getType);
		}
		break;
	case 2: // addr
		if constexpr (hasAddress) {
			sortUpDown_T(items, sortSpecs, [](const auto& item) {
				return std::tuple(getAddressString   (item).getString(),
				                  getEndAddressString(item).getString());
			});
		}
		break;
	case 3: // cond
		sortUpDown_String(items, sortSpecs, [](const auto& item) { return getCondition(item).getString(); });
		break;
	case 4: // action
		sortUpDown_String(items, sortSpecs, [](const auto& item) { return getCommand(item).getString(); });
		break;
	default:
		UNREACHABLE;
	}
}

template<typename Type>
[[nodiscard]] static std::string isValidCond(std::string_view cond, Interpreter& interp)
{
	if (cond.empty()) {
		return AllowEmptyCond<Type>{} ? std::string{}
		                              : std::string("cannot be empty");
	}
	return interp.parseExpressionError(cond);
}

[[nodiscard]] static std::string isValidCmd(std::string_view cmd, Interpreter& interp)
{
	if (cmd.empty()) return {}; // ok
	return interp.parseCommandError(cmd);
}

std::string ImGuiBreakPoints::displayAddr(const TclObject& addr) const
{
	auto str = addr.getString();
	if (auto symbol = [&] -> std::optional<std::string_view> {
		if (str.ends_with(')')) {
			if (str.starts_with("$sym("))   return str.substr(5, str.size() - 6);
			if (str.starts_with("$::sym(")) return str.substr(7, str.size() - 8);
		}
		return {};
	}()) {
		if (symbolManager.lookupSymbol(*symbol)) {
			return std::string(*symbol);
		}
	}
	return std::string(str);
}

std::string ImGuiBreakPoints::parseDisplayAddress(std::string_view str) const
{
	if (symbolManager.lookupSymbol(str)) {
		return strCat("$sym(", str, ')');
	}
	return std::string(str);
}

template<typename Item>
void ImGuiBreakPoints::drawRow(MSXCPUInterface& cpuInterface, int row, Item& item)
{
	using Type = BaseBpType_t<Item>;
	constexpr bool isWatchPoint = std::is_same_v<Type, WatchPoint>;
	constexpr bool isBreakPoint = std::is_same_v<Type, BreakPoint>;

	auto& interp = manager.getInterpreter();
	const auto& style = ImGui::GetStyle();
	float rowHeight = 2.0f * style.FramePadding.y + ImGui::GetTextLineHeight();

	auto setRedBg = [](bool valid) {
		if (valid) return;
		ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, getColor(imColor::RED_BG));
	};

	if (ImGui::TableNextColumn()) { // enable
		auto pos = ImGui::GetCursorPos();
		if (ImGui::Selectable("##selection", selectedRow == row,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
				{0.0f, rowHeight})) {
			selectedRow = row;
		}
		ImGui::SetCursorPos(pos);

		bool enabled = getEnabled(item);
		if (ImGui::Checkbox("##enabled", &enabled)) {
			auto sc = getScopedChange(item, cpuInterface); (void)sc;
			setEnabled(item, enabled);
		}
		if (ImGui::IsItemActive()) selectedRow = row;
	}
	if (ImGui::TableNextColumn()) { // type
		if constexpr (isWatchPoint) {
			ImGui::SetNextItemWidth(-FLT_MIN);
			auto wpType = static_cast<int>(item->getType());
			if (ImGui::Combo("##type", &wpType, "read IO\000write IO\000read memory\000write memory\000")) {
				auto sc = getScopedChange(item, cpuInterface); (void)sc;
				item->setType(interp, static_cast<WatchPoint::Type>(wpType));
			}
			if (ImGui::IsItemActive()) selectedRow = row;
		}
	}
	if (ImGui::TableNextColumn()) { // address
		std::string addrStr = displayAddr(getAddressString(item));
		ImGui::SetNextItemWidth(-FLT_MIN);
		if constexpr (isWatchPoint) {
			std::string parseError = item->parseAddressError(interp);
			setRedBg(parseError.empty());

			auto pos = ImGui::GetCursorPos();
			std::string endAddrStr = displayAddr(item->getEndAddressString());
			std::string displayAddr = addrStr;
			if (!endAddrStr.empty()) {
				strAppend(displayAddr, "...", endAddrStr);
			}
			im::Font(manager.fontMono, [&]{
				ImGui::TextUnformatted(displayAddr);
			});
			ImGui::SetCursorPos(pos);
			if (ImGui::InvisibleButton("##range-button", {-FLT_MIN, rowHeight})) {
				ImGui::OpenPopup("range-popup");
			}

			auto addr = item->getBeginAddress();
			if (parseError.empty() && addr) {
				simpleToolTip([&]{
					auto tip = strCat("0x", hex_string<4>(*addr));
					if (auto endAddr = item->getEndAddress(); endAddr && *endAddr != *addr) {
						strAppend(tip, "...0x", hex_string<4>(*endAddr));
					}
					return tip;
				});
			} else {
				simpleToolTip(parseError);
			}

			if (ImGui::IsItemActive()) selectedRow = row;
			im::Popup("range-popup", [&]{
				editRange(cpuInterface, item, addrStr, endAddrStr);
			});
		} else if constexpr (isBreakPoint) {
			std::string parseError = item.parseAddressError(interp);
			setRedBg(parseError.empty());

			im::Font(manager.fontMono, [&]{
				if (ImGui::InputText("##addr", &addrStr)) {
					auto sc = getScopedChange(item, cpuInterface); (void)sc;
					item.setAddress(interp, TclObject(parseDisplayAddress(addrStr)));
				}
			});

			auto addr = item.getAddress();
			if (parseError.empty() && addr) {
				simpleToolTip([&]{ return strCat("0x", hex_string<4>(*addr)); });
			} else {
				simpleToolTip(parseError);
			}

			im::PopupContextItem("context menu", [&]{
				if (ImGui::MenuItem("Show in Disassembly", nullptr, nullptr, addr.has_value())) {
					manager.debugger->setGotoTarget(*addr);
				}
			});

			if (ImGui::IsItemActive()) selectedRow = row;
		}
	}
	if (ImGui::TableNextColumn()) { // condition
		std::string cond{getCondition(item).getString()};
		std::string parseError = isValidCond<Type>(cond, interp);
		setRedBg(parseError.empty());
		auto checkCmd = getCheckCmd<Type>();
		ParsedSlotCond slot(checkCmd, cond);
		auto pos = ImGui::GetCursorPos();
		im::Font(manager.fontMono, [&]{
			ImGui::TextUnformatted(slot.toDisplayString());
		});
		ImGui::SetCursorPos(pos);
		if (ImGui::InvisibleButton("##cond-button", {-FLT_MIN, rowHeight})) {
			ImGui::OpenPopup("cond-popup");
		}
		if (ImGui::IsItemActive()) selectedRow = row;
		simpleToolTip(parseError);
		im::Popup("cond-popup", [&]{
			if (editCondition(slot)) {
				cond = slot.toTclExpression(checkCmd);
				setCondition(item, TclObject(cond));
			}
		});
	}
	if (ImGui::TableNextColumn()) { // action
		std::string cmd{getCommand(item).getString()};
		std::string parseError = isValidCmd(cmd, interp);
		setRedBg(parseError.empty());
		im::Font(manager.fontMono, [&]{
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::InputText("##cmd", &cmd)) {
				setCommand(item, TclObject(cmd));
			}
			if (ImGui::IsItemActive()) selectedRow = row;
		});
		simpleToolTip(parseError);
	}
	if (ImGui::TableNextColumn()) { // once
		bool once = getOnce(item);
		if (ImGui::Checkbox("##once", &once)) {
			setOnce(item, once);
		}
		if (ImGui::IsItemActive()) selectedRow = row;
	}
}

void ImGuiBreakPoints::paintBpTab(MSXCPUInterface& cpuInterface, uint16_t addr)
{
	paintTab<BreakPoint>(cpuInterface, addr);
}

template<typename Type>
void ImGuiBreakPoints::paintTab(MSXCPUInterface& cpuInterface, std::optional<uint16_t> addr)
{
	constexpr bool isWatchPoint = std::is_same_v<Type, WatchPoint>;
	constexpr bool isCondition  = std::is_same_v<Type, DebugCondition>;
	bool hasAddress = HasAddress<Type>{} && !addr; // don't draw address-column if filtering a specific address
	Type* tag = nullptr;
	auto& items = getItems(tag, cpuInterface);

	int flags = ImGuiTableFlags_RowBg |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_Sortable |
		ImGuiTableFlags_Hideable |
		ImGuiTableFlags_Reorderable |
		ImGuiTableFlags_ContextMenuInBody |
		ImGuiTableFlags_SizingStretchProp;
	if (!addr) {
		flags |= ImGuiTableFlags_ScrollY
		      |  ImGuiTableFlags_ScrollX;
	}
	const auto& style = ImGui::GetStyle();
	auto width = style.ItemSpacing.x + 2.0f * style.FramePadding.x + ImGui::CalcTextSize("Remove").x;
	bool disableRemove = true;
	int count = 0;
	int lastDrawnRow = -1; // should only be used when count=1
	im::Table("items", 6, flags, {-width, 0}, [&]{
		ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
		ImGui::TableSetupColumn("Enable", ImGuiTableColumnFlags_WidthFixed);
		int typeFlags = isWatchPoint ? ImGuiTableColumnFlags_NoHide : ImGuiTableColumnFlags_Disabled;
		ImGui::TableSetupColumn("Type", typeFlags);
		int addressFlags = hasAddress ? ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_DefaultSort
		                              : ImGuiTableColumnFlags_Disabled;
		ImGui::TableSetupColumn("Address", addressFlags);
		ImGui::TableSetupColumn("Condition", isCondition ? ImGuiTableColumnFlags_NoHide : 0);
		ImGui::TableSetupColumn("Action", addr ? 0 : ImGuiTableColumnFlags_DefaultHide);
		ImGui::TableSetupColumn("Once", ImGuiTableColumnFlags_DefaultHide);
		ImGui::TableHeadersRow();

		checkSort(items);

		im::ID_for_range(items.size(), [&](int row) {
			if (!addr || (getAddress(items[row]) == addr)) {
				++count; lastDrawnRow = row;
				if (row == selectedRow) disableRemove = false;
				drawRow(cpuInterface, row, items[row]);
			}
		});
	});
	if (count == 1) disableRemove = false;
	ImGui::SameLine();
	im::Group([&] {
		if (ImGui::Button("Add")) {
			createNew<Type>(cpuInterface, manager.getInterpreter(), addr);
			selectedRow = -1;
		}
		im::Disabled(disableRemove, [&]{
			if (ImGui::Button("Remove")) {
				int removeRow = (count == 1) ? lastDrawnRow : selectedRow;
				auto it = items.begin() + removeRow;
				remove<Type>(cpuInterface, getId(*it));
				selectedRow = -1;
			}
		});
		if (!addr) {
			ImGui::Spacing();
			im::Disabled(items.empty() ,[&]{
				if (ImGui::Button("Clear")) {
					while (!items.empty()) {
						remove<Type>(cpuInterface, getId(items.back()));
					}
				}
			});
		}
	});
}

void ImGuiBreakPoints::editRange(MSXCPUInterface& cpuInterface, std::shared_ptr<WatchPoint>& wp, std::string& begin, std::string& end)
{
	ImGui::TextUnformatted("address range"sv);
	im::Indent([&]{
		const auto& style = ImGui::GetStyle();
		auto pos = ImGui::GetCursorPos().x + ImGui::CalcTextSize("end: (?)").x + 2.0f * style.ItemSpacing.x;

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("begin:  "sv);
		ImGui::SameLine(pos);
		im::Font(manager.fontMono, [&]{
			if (ImGui::InputText("##begin", &begin)) {
				auto& interp = manager.getInterpreter();
				auto sc = getScopedChange(wp, cpuInterface); (void)sc;
				wp->setBeginAddressString(interp, TclObject(parseDisplayAddress(begin)));
			}
		});

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("end:"sv);
		HelpMarker("End address is included in the range.\n"
		           "Leave empty for a single address.");
		ImGui::SameLine(pos);
		im::Font(manager.fontMono, [&]{
			if (ImGui::InputText("##end", &end)) {
				auto& interp = manager.getInterpreter();
				auto sc = getScopedChange(wp, cpuInterface); (void)sc;
				wp->setEndAddressString(interp, TclObject(parseDisplayAddress(end)));
			}
		});
	});
}

bool ImGuiBreakPoints::editCondition(ParsedSlotCond& slot)
{
	bool changed = false;
	ImGui::TextUnformatted("slot"sv);
	im::Indent([&]{
		const auto& style = ImGui::GetStyle();
		auto pos = ImGui::GetCursorPos().x + ImGui::GetFrameHeight() +
		           ImGui::CalcTextSize("secondary").x + 2.0f * style.ItemSpacing.x;

		changed |= ImGui::Checkbox("primary", &slot.hasPs);
		ImGui::SameLine(pos);
		im::Disabled(!slot.hasPs, [&]{
			changed |= ImGui::Combo("##ps", &slot.ps, "0\0001\0002\0003\000");

			changed |= ImGui::Checkbox("secondary", &slot.hasSs);
			ImGui::SameLine(pos);
			im::Disabled(!slot.hasSs, [&]{
				changed |= ImGui::Combo("##ss", &slot.ss, "0\0001\0002\0003\000");
			});

			changed |= ImGui::Checkbox("segment", &slot.hasSeg);
			ImGui::SameLine(pos);
			im::Disabled(!slot.hasSeg, [&]{
				uint8_t one = 1;
				changed |= ImGui::InputScalar("##seg", ImGuiDataType_U8, &slot.seg, &one);
			});
		});
	});
	ImGui::TextUnformatted("Tcl expression"sv);
	im::Indent([&]{
		im::Font(manager.fontMono, [&]{
			ImGui::SetNextItemWidth(-FLT_MIN);
			changed |= ImGui::InputText("##cond", &slot.rest);
		});
	});
	return changed;
}

void ImGuiBreakPoints::refreshSymbols()
{
	auto& interp = manager.getInterpreter();
	for (auto& bp : MSXCPUInterface::getBreakPoints()) {
		bp.evaluateAddress(interp);
	}
	if (auto* motherBoard = manager.getReactor().getMotherBoard()) {
		auto& cpuInterface = motherBoard->getCPUInterface();
		for (auto& wp : cpuInterface.getWatchPoints()) {
			auto sc = getScopedChange(wp, cpuInterface);
			wp->evaluateAddress(interp);
		}
	}
	// nothing for DebugCondition
}

} // namespace openmsx
