#ifndef IMGUI_BREAKPOINT_HH
#define IMGUI_BREAKPOINT_HH

#include "ImGuiPart.hh"

#include "TclObject.hh"

#include <optional>
#include <vector>

namespace openmsx {

class BreakPoint;
class DebugCondition;
class Debugger;
class ImGuiManager;
class MSXCPUInterface;
class WatchPoint;

struct ParsedSlotCond {
	ParsedSlotCond(std::string_view checkCmd, std::string_view cond);
	[[nodiscard]] std::string toTclExpression(std::string_view checkCmd) const;
	[[nodiscard]] std::string toDisplayString() const;

	std::string rest;

	int ps = 0;
	int ss = 0;
	uint8_t seg = 0;
	bool hasPs = false; // if false then the whole slot-substructure is invalid
	bool hasSs = false;
	bool hasSeg = false;
};

class ImGuiBreakPoints final : public ImGuiPart
{
public:
	struct GuiItem {
		int id; // id > 0: exists also on the openMSX side
		        // id < 0: only exists on the GUI side
		bool wantEnable; // only really enabled if it's also valid
		int wpType; // only used for Watchpoint
		std::optional<uint16_t> addr; // not used for DebugCondition
		std::optional<uint16_t> endAddr; // (inclusive) only used for WatchPoint
		TclObject addrStr;
		TclObject endAddrStr;
		TclObject cond;
		TclObject cmd;
	};

public:
	ImGuiBreakPoints(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "breakpoints"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	template<typename Item> void paintTab(MSXCPUInterface& cpuInterface, Debugger& debugger);
	template<typename Item> void synchronize(std::vector<GuiItem>& items, MSXCPUInterface& cpuInterface);
	void checkSort(std::vector<GuiItem>& items);
	template<typename Item> void drawRow(MSXCPUInterface& cpuInterface, Debugger& debugger, int row, GuiItem& item);
	bool editRange(std::string& begin, std::string& end);
	bool editCondition(ParsedSlotCond& slot);

	[[nodiscard]] auto& getItems(BreakPoint*) { return guiBps; }
	[[nodiscard]] auto& getItems(WatchPoint*) { return guiWps; }
	[[nodiscard]] auto& getItems(DebugCondition*) { return guiConditions; }

public:
	bool show = false;

private:
	ImGuiManager& manager;

	static inline int idCounter = 0;
	std::vector<GuiItem> guiBps;
	std::vector<GuiItem> guiWps;
	std::vector<GuiItem> guiConditions;
	int selectedRow = -1;
};

} // namespace openmsx

#endif
