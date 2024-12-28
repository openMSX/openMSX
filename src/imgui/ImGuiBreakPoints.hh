#ifndef IMGUI_BREAKPOINT_HH
#define IMGUI_BREAKPOINT_HH

#include "ImGuiPart.hh"

#include "TclObject.hh"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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
	explicit ImGuiBreakPoints(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "breakpoints"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

	void refreshSymbols();

	void paintBpTab(MSXCPUInterface& cpuInterface, uint16_t addr);

private:
	template<typename Type> void paintTab(MSXCPUInterface& cpuInterface, std::optional<uint16_t> addr = {});
	template<typename Item> void drawRow(MSXCPUInterface& cpuInterface, int row, Item& item);
	[[nodiscard]] std::string displayAddr(const TclObject& addr) const;
	[[nodiscard]] std::string parseDisplayAddress(std::string_view str) const;
	void editRange(MSXCPUInterface& cpuInterface, std::shared_ptr<WatchPoint>& wp, std::string& begin, std::string& end);
	bool editCondition(ParsedSlotCond& slot);

public:
	bool show = false;

private:
	SymbolManager& symbolManager;

	int selectedRow = -1;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiBreakPoints::show}
	};
};

} // namespace openmsx

#endif
