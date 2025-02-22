#include "ImGuiDisassembly.hh"

#include "ImGuiBreakPoints.hh"
#include "ImGuiCpp.hh"
#include "ImGuiDebugger.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "CPURegs.hh"
#include "Dasm.hh"
#include "Debugger.hh"
#include "Display.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXMemoryMapperBase.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "RomBlockDebuggable.hh"
#include "RomPlain.hh"
#include "SymbolManager.hh"
#include "VideoSystem.hh"

#include "narrow.hh"
#include "join.hh"
#include "strCat.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cstdint>
#include <ranges>
#include <vector>

using namespace std::literals;

namespace openmsx {

ImGuiDisassembly::ImGuiDisassembly(ImGuiManager& manager_, size_t index)
	: ImGuiPart(manager_)
	, symbolManager(manager.getReactor().getSymbolManager())
	, title("Disassembly")
{
	if (index) {
		strAppend(title, " (", index + 1, ')');
	}
	scrollToPcOnBreak = index == 0;
}

void ImGuiDisassembly::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiDisassembly::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiDisassembly::loadEnd()
{
	setDisassemblyScrollY = disassemblyScrollY;
}

void ImGuiDisassembly::setGotoTarget(uint16_t target)
{
	gotoTarget = target;
	show = true;
	setDisassemblyScrollY.reset(); // don't restore initial scroll position
}

std::pair<const MSXRom*, RomBlockDebuggableBase*>
	ImGuiDisassembly::getRomBlocks(Debugger& debugger, const MSXDevice* device)
{
	RomBlockDebuggableBase* debuggable = nullptr;
	const auto* rom = dynamic_cast<const MSXRom*>(device);
	if (rom && !dynamic_cast<const RomPlain*>(rom)) {
		debuggable = dynamic_cast<RomBlockDebuggableBase*>(
			debugger.findDebuggable(rom->getName() + " romblocks"));
	}
	return {rom, debuggable};
}

struct CurrentSlot {
	int ps = 0;
	std::optional<int> ss;
	std::optional<int> seg;
};
[[nodiscard]] static CurrentSlot getCurrentSlot(
	MSXCPUInterface& cpuInterface, Debugger& debugger,
	uint16_t addr, bool wantSs = true, bool wantSeg = true)
{
	CurrentSlot result;
	int page = addr / 0x4000;
	result.ps = cpuInterface.getPrimarySlot(page);

	if (wantSs && cpuInterface.isExpanded(result.ps)) {
		result.ss = cpuInterface.getSecondarySlot(page);
	}
	if (wantSeg) {
		const auto* device = cpuInterface.getVisibleMSXDevice(page);
		if (const auto* mapper = dynamic_cast<const MSXMemoryMapperBase*>(device)) {
			result.seg = mapper->getSelectedSegment(narrow<uint8_t>(page));
		} else if (auto [_, romBlocks] = ImGuiDisassembly::getRomBlocks(debugger, device); romBlocks) {
			result.seg = romBlocks->readExt(addr);
		}
	}
	return result;
}

[[nodiscard]] static TclObject toTclExpression(const CurrentSlot& slot)
{
	std::string result = strCat("[pc_in_slot ", slot.ps);
	if (slot.ss) {
		strAppend(result, ' ', *slot.ss);
	} else {
		if (slot.seg) strAppend(result, " X");
	}
	if (slot.seg) strAppend(result, ' ', *slot.seg);
	strAppend(result, ']');
	return TclObject(result);
}

[[nodiscard]] static bool addrInSlot(
	const ParsedSlotCond& slot, MSXCPUInterface& cpuInterface, Debugger& debugger, uint16_t addr)
{
	if (!slot.hasPs) return true; // no ps specified -> always ok

	auto current = getCurrentSlot(cpuInterface, debugger, addr, slot.hasSs, slot.hasSeg);
	if (slot.ps != current.ps) return false;
	if (slot.hasSs && current.ss && (slot.ss != current.ss)) return false;
	if (slot.hasSeg && current.seg && (slot.seg != current.seg)) return false;
	return true;
}

struct BpLine {
	int count = 0;
	int idx = -1; // only valid when count=1
	bool anyEnabled = false;
	bool anyDisabled = false;
	bool anyInSlot = false;
	bool anyComplex = false;
};
static BpLine examineBpLine(uint16_t addr, std::span<BreakPoint> bps, MSXCPUInterface& cpuInterface, Debugger& debugger)
{
	BpLine result;
	for (auto [i, bp] : enumerate(bps)) {
		if (bp.getAddress() != addr) continue;
		++result.count;
		result.idx = int(i);

		bool enabled = bp.isEnabled();
		result.anyEnabled |= enabled;
		result.anyDisabled |= !enabled;

		ParsedSlotCond bpSlot("pc_in_slot", bp.getCondition().getString());
		result.anyInSlot |= addrInSlot(bpSlot, cpuInterface, debugger, addr);

		result.anyComplex |= !bpSlot.rest.empty() || (bp.getCommand().getString() != "debug break");
	}
	return result;
}

static void toggleBp(uint16_t addr, const BpLine& bpLine, std::span<BreakPoint> bps,
                     MSXCPUInterface& cpuInterface, Debugger& debugger,
                     std::optional<BreakPoint>& addBp, std::optional<unsigned>& removeBpId)
{
	if (bpLine.count != 0) {
		// only allow to remove single breakpoints,
		// others can be edited via the breakpoint viewer
		if (bpLine.count == 1) {
			auto& bp = bps[bpLine.idx];
			removeBpId = bp.getId(); // schedule removal
		}
	} else {
		// schedule creation of new bp
		addBp.emplace();
		addBp->setAddress(debugger.getInterpreter(), TclObject(tmpStrCat("0x", hex_string<4>(addr))));
		addBp->setCondition(toTclExpression(getCurrentSlot(cpuInterface, debugger, addr)));
	}
}
void ImGuiDisassembly::actionToggleBp(MSXMotherBoard& motherBoard)
{
	auto pc = motherBoard.getCPU().getRegisters().getPC();
	auto& cpuInterface = motherBoard.getCPUInterface();
	auto& debugger = motherBoard.getDebugger();
	auto& bps = cpuInterface.getBreakPoints();

	auto bpLine = examineBpLine(pc, bps, cpuInterface, debugger);

	std::optional<BreakPoint> addBp;
	std::optional<unsigned> removeBpId;
	toggleBp(pc, bpLine, bps, cpuInterface, debugger, addBp, removeBpId);
	if (addBp) {
		cpuInterface.insertBreakPoint(std::move(*addBp));
	}
	if (removeBpId) {
		cpuInterface.removeBreakPoint(*removeBpId);
	}
}

void ImGuiDisassembly::paint(MSXMotherBoard* motherBoard)
{
	if (!motherBoard || !show) return;

	ImGui::SetNextWindowSize({340, 540}, ImGuiCond_FirstUseEver);
	im::Window(title.c_str(), &show, [&]{
		auto& regs = motherBoard->getCPU().getRegisters();
		auto& cpuInterface = motherBoard->getCPUInterface();
		auto& debugger = motherBoard->getDebugger();
		auto time = motherBoard->getCurrentTime();

		manager.debugger->checkShortcuts(cpuInterface, *motherBoard);

		std::optional<BreakPoint> addBp;
		std::optional<unsigned> removeBpId;

		auto pc = regs.getPC();
		if (followPC && !MSXCPUInterface::isBreaked()) {
			gotoTarget = pc;
		}

		auto widthOpcode = ImGui::CalcTextSize("12 34 56 78"sv).x;
		int flags = ImGuiTableFlags_RowBg |
			ImGuiTableFlags_BordersV |
			ImGuiTableFlags_BordersOuterV |
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_Hideable |
			ImGuiTableFlags_Reorderable |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_ScrollX;
		im::Table("table", 4, flags, [&]{
			ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
			ImGui::TableSetupColumn("bp", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("address", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn("opcode", ImGuiTableColumnFlags_WidthFixed, widthOpcode);
			ImGui::TableSetupColumn("mnemonic", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide);
			ImGui::TableHeadersRow();

			auto& bps = cpuInterface.getBreakPoints();
			auto textSize = ImGui::GetTextLineHeight();

			std::string mnemonic;
			std::string opcodesStr;
			std::vector<std::string_view> candidates;
			std::array<uint8_t, 4> opcodes;
			ImGuiListClipper clipper; // only draw the actually visible rows
			clipper.Begin(0x10000, ImGui::GetTextLineHeightWithSpacing());
			if (gotoTarget) {
				clipper.IncludeItemsByIndex(narrow<int>(*gotoTarget), narrow<int>(*gotoTarget + 4));
			}
			std::optional<unsigned> nextGotoTarget;
			std::optional<unsigned> minAddr;
			std::optional<unsigned> maxAddr;
			bool toClipboard = false;
			while (clipper.Step()) {
				// Note this while loop can iterate multiple times, though we mitigate this by passing the
				// row height to the ImGuiListClipper constructor. Another reason is because we called
				// clipper.IncludeItemsByIndex(), but that only happens when 'gotoTarget' is set.
				// Because of this it's acceptable to just record the min and max address in the for loop below.
				auto addr16 = instructionBoundary(cpuInterface, narrow<uint16_t>(clipper.DisplayStart), time);
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
					unsigned addr = addr16;
					minAddr = std::min(addr, minAddr.value_or(std::numeric_limits<unsigned>::max()));
					maxAddr = std::max(addr, maxAddr.value_or(std::numeric_limits<unsigned>::min()));
					ImGui::TableNextRow();
					if (addr >= 0x10000) continue;
					im::ID(narrow<int>(addr), [&]{
						if (gotoTarget && addr >= *gotoTarget) {
							gotoTarget = {};
							ImGui::SetScrollHereY(0.25f);
						}

						if (bool rowAtPc = !syncDisassemblyWithPC && (addr == pc);
						    rowAtPc) {
							ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, getColor(imColor::YELLOW_BG));
						}
						bool bpRightClick = false;
						if (ImGui::TableNextColumn()) { // bp
							auto bpLine = examineBpLine(addr16, bps, cpuInterface, debugger);
							bool hasBp = bpLine.count != 0;
							bool multi = bpLine.count > 1;
							if (hasBp) {
								auto calcColor = [](bool enabled, bool inSlot) {
									auto [r,g,b] = enabled ? std::tuple{0xE0, 0x00, 0x00}
									                       : std::tuple{0x70, 0x70, 0x70};
									auto a = inSlot ? 0xFF : 0x60;
									return IM_COL32(r, g, b, a);
								};
								auto colIn = calcColor(bpLine.anyEnabled, bpLine.anyInSlot);
								auto colOut = ImGui::GetColorU32(ImGuiCol_WindowBg);

								auto* drawList = ImGui::GetWindowDrawList();
								gl::vec2 scrn = ImGui::GetCursorScreenPos();
								auto center = scrn + textSize * gl::vec2(multi ? 0.3f : 0.5f, 0.5f);
								auto radiusIn = 0.4f * textSize;
								auto radiusOut = 0.5f * textSize;

								if (multi) {
									auto center2 = center + textSize * gl::vec2(0.4f, 0.0f);
									drawList->AddCircleFilled(center2, radiusOut, colOut);
									auto colIn2 = calcColor(!bpLine.anyDisabled, bpLine.anyInSlot);
									drawList->AddCircleFilled(center2, radiusIn, colIn2);
								}
								drawList->AddCircleFilled(center, radiusOut, colOut);
								drawList->AddCircleFilled(center, radiusIn, colIn);
								if (bpLine.anyComplex) {
									auto d = 0.3f * textSize;
									auto c = IM_COL32(0, 0, 0, 192);
									auto t = 0.2f * textSize;
									drawList->AddLine(center - gl::vec2(d, 0.0f), center + gl::vec2(d, 0.0f), c, t);
									drawList->AddLine(center - gl::vec2(0.0f, d), center + gl::vec2(0.0f, d), c, t);
								}
							}
							if (ImGui::InvisibleButton("##bp-button", {-FLT_MIN, textSize})) {
								toggleBp(addr16, bpLine, bps, cpuInterface, debugger, addBp, removeBpId);
							} else {
								bpRightClick = hasBp && ImGui::IsItemClicked(ImGuiMouseButton_Right);
								if (bpRightClick) ImGui::OpenPopup("bp-context");
								im::Popup("bp-context", [&]{
									manager.breakPoints->paintBpTab(cpuInterface, addr16);
								});
							}
						}

						std::optional<uint16_t> mnemonicAddr;
						std::span<const Symbol* const> mnemonicLabels;
						auto len = disassemble(cpuInterface, addr, pc, time,
							opcodes, mnemonic, mnemonicAddr, mnemonicLabels);

						if (ImGui::TableNextColumn()) { // addr
							bool focusScrollToAddress = false;
							bool focusRunToAddress = false;

							// do the full-row-selectable stuff in a column that cannot be hidden
							auto pos = ImGui::GetCursorPos();
							ImGui::Selectable("##row", false,
									ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
							using enum Shortcuts::ID;
							auto& shortcuts = manager.getShortcuts();
							if (shortcuts.checkShortcut(DISASM_GOTO_ADDR)) {
								ImGui::OpenPopup("disassembly-context");
								focusScrollToAddress = true;
							}
							if (shortcuts.checkShortcut(DISASM_RUN_TO_ADDR)) {
								ImGui::OpenPopup("disassembly-context");
								focusRunToAddress = true;
							}
							if (!bpRightClick && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
								ImGui::OpenPopup("disassembly-context");
							}

							auto addrStr = tmpStrCat(hex_string<4>(addr));
							im::Popup("disassembly-context", [&]{
								auto addrToolTip = [&](const std::string& str) {
									simpleToolTip([&]{
										if (auto a = symbolManager.parseSymbolOrValue(str)) {
											return strCat("0x", hex_string<4>(*a));
										}
										return std::string{};
									});
								};

								if (ImGui::MenuItem("Scroll to PC")) {
									nextGotoTarget = pc;
								}

								im::Indent([&]{
									ImGui::Checkbox("Follow PC while running", &followPC);
									ImGui::Checkbox("Scroll to PC on break", &scrollToPcOnBreak);
								});

								ImGui::AlignTextToFramePadding();
								ImGui::TextUnformatted("Scroll to address:"sv);
								ImGui::SameLine();
								if (focusScrollToAddress) ImGui::SetKeyboardFocusHere();
								if (ImGui::InputText("##goto", &gotoAddr, ImGuiInputTextFlags_EnterReturnsTrue)) {
									if (auto a = symbolManager.parseSymbolOrValue(gotoAddr)) {
										nextGotoTarget = *a;
									}
								}
								addrToolTip(gotoAddr);

								ImGui::Separator();

								auto runTo = strCat("Run to here (address 0x", addrStr, ')');
								if (ImGui::MenuItem(runTo.c_str())) {
									manager.executeDelayed(makeTclList("run_to", addr));
								}

								ImGui::AlignTextToFramePadding();
								ImGui::TextUnformatted("Run to address:"sv);
								ImGui::SameLine();
								if (focusRunToAddress) ImGui::SetKeyboardFocusHere();
								if (ImGui::InputText("##run", &runToAddr, ImGuiInputTextFlags_EnterReturnsTrue)) {
									if (auto a = symbolManager.parseSymbolOrValue(runToAddr)) {
										manager.executeDelayed(makeTclList("run_to", *a));
									}
								}
								addrToolTip(runToAddr);

								ImGui::Separator();

								auto setPc = strCat("Set PC to 0x", addrStr);
								if (ImGui::MenuItem(setPc.c_str())) {
									regs.setPC(addr16);
								}

								ImGui::Separator();

								if (ImGui::MenuItem("Copy instructions to clipboard")) {
									toClipboard = true;
								}
							});

							enum class Priority {
								MISSING_BOTH = 0, // from lowest to highest
								MISSING_ONE,
								SLOT_AND_SEGMENT
							};
							using enum Priority;
							Priority currentPriority = MISSING_BOTH;
							candidates.clear();
							auto add = [&](const Symbol* sym, Priority newPriority) {
								if (newPriority < currentPriority) return; // we already have a better candidate
								if (newPriority > currentPriority) candidates.clear(); // drop previous candidates, we found a better one
								currentPriority = newPriority;
								candidates.push_back(sym->name); // cycle symbols in the same priority level
							};

							auto slot = getCurrentSlot(cpuInterface, debugger, addr16);
							auto psSs = (slot.ss.value_or(0) << 2) + slot.ps;
							auto addrLabels = symbolManager.lookupValue(addr16);
							for (const Symbol* symbol: addrLabels) {
								// skip symbols with any mismatch
								if (symbol->slot && *symbol->slot != psSs) continue;
								if (symbol->segment && *symbol->segment != slot.seg) continue;
								// the info that's present does match
								if (symbol->slot && symbol->segment == slot.seg) {
									add(symbol, SLOT_AND_SEGMENT);
								} else if (!symbol->slot && !symbol->segment) {
									add(symbol, MISSING_BOTH);
								} else {
									add(symbol, MISSING_ONE);
								}
							}
							ImGui::SetCursorPos(pos);
							im::Font(manager.fontMono, [&]{
								ImGui::TextUnformatted(candidates.empty() ? addrStr : candidates[cycleLabelsCounter % candidates.size()]);
							});
							if (!addrLabels.empty()) {
								simpleToolTip([&]{
									std::string tip(addrStr);
									if (addrLabels.size() > 1) {
										strAppend(tip, "\nmultiple possibilities (click to cycle):\n",
											join(std::views::transform(addrLabels, &Symbol::name), ' '));
									}
									return tip;
								});
								ImGui::SetCursorPos(pos);
								if (ImGui::InvisibleButton("##addrButton", {-FLT_MIN, textSize})) {
									++cycleLabelsCounter;
								}
							}
						}

						if (ImGui::TableNextColumn()) { // opcode
							opcodesStr.clear();
							for (auto i : xrange(len)) {
								strAppend(opcodesStr, hex_string<2>(opcodes[i]), ' ');
							}
							im::Font(manager.fontMono, [&]{
								ImGui::TextUnformatted(opcodesStr.data(), opcodesStr.data() + 3 * size_t(len) - 1);
							});
						}

						if (ImGui::TableNextColumn()) { // mnemonic
							auto pos = ImGui::GetCursorPos();
							im::Font(manager.fontMono, [&]{
								ImGui::TextUnformatted(mnemonic);
							});
							if (mnemonicAddr) {
								if (!mnemonicLabels.empty()) {
									ImGui::SetCursorPos(pos);
									if (ImGui::InvisibleButton("##mnemonicButton", {-FLT_MIN, textSize})) {
										++cycleLabelsCounter;
									}
									simpleToolTip([&]{
										auto tip = strCat('#', hex_string<4>(*mnemonicAddr));
										if (mnemonicLabels.size() > 1) {
											strAppend(tip, "\nmultiple possibilities (click to cycle):\n",
												join(std::views::transform(mnemonicLabels, &Symbol::name), ' '));
										}
										return tip;
									});
								}
								if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
									nextGotoTarget = *mnemonicAddr;
								}
							}
						}
						addr16 += len;
					});
				}
			}
			gotoTarget = nextGotoTarget;
			if (syncDisassemblyWithPC) {
				syncDisassemblyWithPC = false; // only once

				auto itemHeight = ImGui::GetTextLineHeightWithSpacing();
				auto winHeight = ImGui::GetWindowHeight();
				auto lines = std::max(1, int(winHeight / itemHeight) - 1); // approx

				auto topAddr = nInstructionsBefore(cpuInterface, pc, time, narrow<int>(lines / 4) + 1);

				ImGui::SetScrollY(float(topAddr) * itemHeight);
			} else if (setDisassemblyScrollY) {
				ImGui::SetScrollY(*setDisassemblyScrollY);
				setDisassemblyScrollY.reset();
			}
			disassemblyScrollY = ImGui::GetScrollY();

			if (toClipboard && minAddr && maxAddr) {
				disassembleToClipboard(cpuInterface, pc, time, *minAddr, *maxAddr);
			}
		});
		// only add/remove bp's after drawing (can't change list of bp's while iterating over it)
		if (addBp) {
			cpuInterface.insertBreakPoint(std::move(*addBp));
		}
		if (removeBpId) {
			cpuInterface.removeBreakPoint(*removeBpId);
		}
	});
}

unsigned ImGuiDisassembly::disassemble(
	const MSXCPUInterface& cpuInterface, unsigned addr, unsigned pc, EmuTime::param time,
	std::span<uint8_t, 4> opcodes, std::string& mnemonic,
	std::optional<uint16_t>& mnemonicAddr, std::span<const Symbol* const>& mnemonicLabels)
{
	mnemonic.clear();
	auto len = dasm(cpuInterface, narrow<uint16_t>(addr), opcodes, mnemonic, time,
		[&](std::string& output, uint16_t a) {
			mnemonicAddr = a;
			mnemonicLabels = symbolManager.lookupValue(a);
			if (!mnemonicLabels.empty()) {
				strAppend(output, mnemonicLabels[cycleLabelsCounter % mnemonicLabels.size()]->name);
			} else {
				appendAddrAsHex(output, a);
			}
		});
	assert(len >= 1);
	if ((addr < pc) && (pc < (addr + len))) {
		// pc is strictly inside current instruction,
		// replace the just disassembled instruction with "db #..."
		mnemonicAddr.reset();
		mnemonicLabels = {};
		len = pc - addr;
		assert((1 <= len) && (len <= 3));
		mnemonic = strCat("db     ", join(
			std::views::transform(xrange(len),
				[&](unsigned i) { return strCat('#', hex_string<2>(opcodes[i])); }),
			','));
	}
	return len;
}

void ImGuiDisassembly::disassembleToClipboard(
	const MSXCPUInterface& cpuInterface, unsigned pc, EmuTime::param time,
	unsigned minAddr, unsigned maxAddr)
{
	std::string mnemonic;
	std::array<uint8_t, 4> opcodes;
	std::optional<uint16_t> mnemonicAddr;
	std::span<const Symbol* const> mnemonicLabels;

	std::string result;

	unsigned addr = minAddr;
	while (addr <= maxAddr) {
		mnemonic.clear();
		auto len = disassemble(cpuInterface, addr, pc, time,
			opcodes, mnemonic, mnemonicAddr, mnemonicLabels);
		strAppend(result , '\t', mnemonic, '\n');
		addr += len;
	}

	manager.getReactor().getDisplay().getVideoSystem().setClipboardText(result);
}

} // namespace openmsx
