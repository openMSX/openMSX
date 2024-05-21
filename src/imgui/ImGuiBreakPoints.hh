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
class MSXCPUInterface;
class SymbolManager;
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
	explicit ImGuiBreakPoints(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "breakpoints"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadStart() override;
	void loadLine(std::string_view name, zstring_view value) override;
	void loadEnd() override;
	void paint(MSXMotherBoard* motherBoard) override;

	void refreshSymbols();

	[[nodiscard]] std::vector<GuiItem>& getBps() { return guiBps; }
	void paintBpTab(MSXCPUInterface& cpuInterface, Debugger& debugger, uint16_t addr);

private:
	template<typename Item> void loadItem(zstring_view value);
	template<typename Item> void paintTab(MSXCPUInterface& cpuInterface, Debugger& debugger, std::optional<uint16_t> addr = {});
	template<typename Item> void syncFromOpenMsx(std::vector<GuiItem>& items, MSXCPUInterface& cpuInterface);
	void checkSort(std::vector<GuiItem>& items) const;
	std::optional<uint16_t> parseAddress(const TclObject& o);
	template<typename Item> void syncToOpenMsx(
		MSXCPUInterface& cpuInterface, Debugger& debugger,
		Interpreter& interp, GuiItem& item) const;
	template<typename Item> void drawRow(MSXCPUInterface& cpuInterface, Debugger& debugger, int row, GuiItem& item);
	bool editRange(std::string& begin, std::string& end);
	bool editCondition(ParsedSlotCond& slot);

	[[nodiscard]] auto& getItems(BreakPoint*) { return guiBps; }
	[[nodiscard]] auto& getItems(WatchPoint*) { return guiWps; }
	[[nodiscard]] auto& getItems(DebugCondition*) { return guiConditions; }

	void clear(BreakPoint*,     MSXCPUInterface& cpuInterface);
	void clear(WatchPoint*,     MSXCPUInterface& cpuInterface);
	void clear(DebugCondition*, MSXCPUInterface& cpuInterface);

	template<typename Item> void refresh(std::vector<GuiItem>& items);

public:
	bool show = false;

private:
	SymbolManager& symbolManager;

	static inline int idCounter = 0;
	std::vector<GuiItem> guiBps;
	std::vector<GuiItem> guiWps;
	std::vector<GuiItem> guiConditions;
	int selectedRow = -1;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiBreakPoints::show}
	};
};

} // namespace openmsx

#endif
