#include "ImGuiDebugger.hh"

#include "ImGuiBitmapViewer.hh"
#include "ImGuiBreakPoints.hh"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

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
#include "StringOp.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cstdint>
#include <vector>

using namespace std::literals;


namespace openmsx {

ImGuiDebugger::ImGuiDebugger(ImGuiManager& manager_)
	: manager(manager_)
	, symbolManager(manager.getReactor().getSymbolManager())
{
}

ImGuiDebugger::~ImGuiDebugger() = default;

// TODO quick and dirty, doesn't look right yet
// Created from a .ttf file via:
//    binary_to_compressed_c openmsx-debugger-icons.ttf myIcons > myfont.cpp
//    https://github.com/ocornut/imgui/blob/master/misc/fonts/binary_to_compressed_c.cpp
static const char icons_compressed_base85[1815+1] =
    "7])#######;9u/('/###W),##.f1$#Q6>##%[n42)[KU%G5)=-<NE/1aNV=BZrPSb]->>#ICi9.o`(*HGsO2(b6O3L+lQS%,5LsCC,H3AnAMeNA*&#Gb';9Cs3BMNvSN`sr1dD4Eo3R/"
    "w/)8eXHkG2V3dW.;h^`I@cj`W)a4&>ZW%q/o6Q<Bou@GMQuuoJH`;-*0iR/Go%fr6jMWVRReK?-qmnUCC'Hv'<J]p]`;`Y5@C=GH?o6O+j#)*Mp]G&#70lQ#;0xfLQ7wj#O?DX-@1>F%"
    "5[)<#8JFgL6SgV-xew@'uB5X7If]8QR3O%b'kS`a)%vW-iXx6*BfW>-'lB#$1fs-$R'Mk+(lB#$ckf&,peu.:J/PQ'-MYY#jFs9)*lXJgaDe8Qw(Y:v47Y&#*r[w'WQ1x'1/V:m^U4+3"
    "7GJcM604,NJ:-##O]-x-e:k(NX@$##]jP]4ACh^X0(^fLHl/o#g]L7#N'+&#L)Qv$t@0+*I5^+4]77<.aQ9k0$g`,3$+h,3Pqji06rs-$=uG=3tq$>MOAQY>@'.`&K8P>#g>AvL9q*<#"
    ")EEjLr^PA#m.Us-KRb'4g)Cb4%IuD#GKRL2KkRP/N7R20t3wK#-8/$jJA5U;0viS&[5AC>uIVS%3^oh24GbM(hIL>#xP%U#WE323lpd].B[A30GK_+,sX/D=dn5q&0####GBO&#.1d7$"
    "3g1$#prgo.-<Tv->;gF4LZ>x#<)]L(/p^I*^nr?#&Y@C#cF&@'wvPJ(?nU^#pRpQ0gF*j0EPtdVN'+9%k*l)*0qi<%>=+dVKrdx0Iu(kLgk4DWX?/YGZv9dMb/q8/+fMO#W+WBOM>#`Q"
    "xR--3bO1F*.D,G4rFuG-hwhH-AI7q.3B3I):[(cJ[pl6&]3KI%)B1se0fv]M6T3kuGSoBJNCZY#q+er?Y,8v,5cNY5*`$s$#]YV-[@i?#=@[s$G+TC4P#l8./=]s$1Pev6jlje3&-Xf-"
    "e](jr145d3?;Rv$ZUvC%h5%fqxC$Y@8^CY$O,@H),W'Z-W-Rh27<C[Krsf;$.wg<L9br,4%.6v,>=+dVIIx:/@JJH)Z:Nu.iUkJ2tpPm:t0(Q$@OEp%Upn;%j&5QCa)iS0%5YY#IX$_]"
    "2^x^]Zrju5Fhf-*i+YV-6'PA#(cK+*mtq;/qwkj1?f6<.0MbI),VW/23>%&4m$ie3%&AA4l'eq7+jkA#dlfj-&$+&^S8VTd.^AN-,CeM0l,'hoodPir`IofLX=$##5C.%T=UYF%^Yk*$"
    "/)m<-]'LS-&%.20'>uu#OYU7#euQ['GPSw8P`&:)AF14+&YF&#WW`>$Mbk-$I&l-$Bp0.$eo4wLW(I3#Ys%(#U-4&#MXN1#EIc>#ik*$M0xSfLRq)$#@>[A-$Mg;-'Spr-gN<;Ni;-##"
    "Jer=--`/,Mv_d##tg`=-5X`=-8sG<-*fG<-+C`T.3f1$#mrG<-8X`=-/#XN0Ht*A#e5>##_RFgLX%1kLF=`T.3xL$#)6xU.R,_'#RA`T.T5$C#<rG<-M2(@-xrG<-DKx>-#EfT%8nT`3"
    "]kU.G-i@+H=DpTCuAo6D<B?hD-rI+HXhdxFL7-AFY'auGt$EJ1m,(@'un@VHdt6L#s[DYGB0-gD.X`'/4+auGpkn+H5-xF-^%/(#B_=?-kQU@-QRt7/uJ6(#Cm[*b[^es/c:@bHdN4/#"
    "d>?L-NNei.Fn@-#afh]%mxtLF//5UCvs.>B,(rE-i<cM:^0[eF/1u'#.mk.#[5C/#-)IqLR`4rL;wXrLX.lrL1x.qLf.K-#J#`5/%$S-#c7]nLNM-X.Q####F/LMTxS&=u<7L8#";

static constexpr ImWchar DEBUGGER_ICON_MIN       = 0xead1;
static constexpr ImWchar DEBUGGER_ICON_MAX       = 0xeb8f;
static constexpr ImWchar DEBUGGER_ICON_RUN       = 0xead3;
static constexpr ImWchar DEBUGGER_ICON_BREAK     = 0xead1;
static constexpr ImWchar DEBUGGER_ICON_STEP_IN   = 0xead4;
static constexpr ImWchar DEBUGGER_ICON_STEP_OUT  = 0xead5;
static constexpr ImWchar DEBUGGER_ICON_STEP_OVER = 0xead6;
static constexpr ImWchar DEBUGGER_ICON_STEP_BACK = 0xeb8f;

void ImGuiDebugger::loadIcons()
{
	auto& io = ImGui::GetIO();
	ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;
	static const ImWchar ranges[] = { DEBUGGER_ICON_MIN, DEBUGGER_ICON_MAX, 0 };
	io.Fonts->AddFontFromMemoryCompressedBase85TTF(icons_compressed_base85, 20.0f, &icons_config, ranges);
}

void ImGuiDebugger::signalBreak()
{
	syncDisassemblyWithPC = true;
}

void ImGuiDebugger::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	for (const auto& [name, editor] : hexEditors) {
		if (editor.Open) {
			buf.appendf("hexEditor.%s=1\n", name.c_str());
		}
	}
}

void ImGuiDebugger::loadStart()
{
	hexEditors.clear();
}

void ImGuiDebugger::loadLine(std::string_view name, zstring_view value)
{
	static constexpr std::string_view prefix = "hexEditor.";

	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name.starts_with(prefix)) {
		auto debuggableName = name.substr(prefix.size());
		auto it = ranges::upper_bound(hexEditors, debuggableName, {}, &EditorInfo::name);
		hexEditors.emplace(it, std::string(debuggableName));
	}
}

void ImGuiDebugger::showMenu(MSXMotherBoard* motherBoard)
{
	auto createHexEditor = [&](const std::string& name) {
		// prefer to reuse a previously closed editor
		bool found = false;
		auto [b, e] = ranges::equal_range(hexEditors, name, {}, &EditorInfo::name);
		for (auto it = b; it != e; ++it) {
			if (!it->editor.Open) {
				it->editor.Open = true;
				found = true;
				break;
			}
		}
		// or create a new one
		if (!found) {
			auto it = ranges::upper_bound(hexEditors, name, {}, &EditorInfo::name);
			hexEditors.emplace(it, name);
		}
	};

	im::Menu("Debugger", motherBoard != nullptr, [&]{
		ImGui::MenuItem("Tool bar", nullptr, &showControl);
		ImGui::MenuItem("Disassembly", nullptr, &showDisassembly);
		ImGui::MenuItem("CPU registers", nullptr, &showRegisters);
		ImGui::MenuItem("CPU flags", nullptr, &showFlags);
		ImGui::MenuItem("Slots", nullptr, &showSlots);
		ImGui::MenuItem("Stack", nullptr, &showStack);
		auto it = ranges::lower_bound(hexEditors, "memory", {}, &EditorInfo::name);
		bool memoryOpen = (it != hexEditors.end()) && it->editor.Open;
		if (ImGui::MenuItem("Memory", nullptr, &memoryOpen)) {
			if (memoryOpen) {
				createHexEditor("memory");
			} else {
				assert(it != hexEditors.end());
				it->editor.Open = false;
			}
		}
		ImGui::Separator();
		ImGui::MenuItem("Breakpoints", nullptr, &manager.breakPoints.show);
		ImGui::MenuItem("Symbol manager", nullptr, &manager.symbols.show);
		ImGui::MenuItem("Watch expression", nullptr, &manager.watchExpr.show);
		ImGui::Separator();
		ImGui::MenuItem("VDP bitmap viewer", nullptr, &manager.bitmap.showBitmapViewer);
		ImGui::MenuItem("VDP tile viewer", nullptr, &manager.character.show);
		ImGui::MenuItem("VDP sprite viewer", nullptr, &manager.sprite.show);
		ImGui::MenuItem("VDP register viewer", nullptr, &manager.vdpRegs.show);
		ImGui::MenuItem("Palette editor", nullptr, &manager.palette.show);
		ImGui::Separator();
		im::Menu("Add hex editor", [&]{
			auto& debugger = motherBoard->getDebugger();
			auto debuggables = to_vector<std::pair<std::string, Debuggable*>>(debugger.getDebuggables());
			ranges::sort(debuggables, StringOp::caseless{}, [](const auto& p) { return p.first; }); // sort on name
			for (const auto& [name, debuggable] : debuggables) {
				if (ImGui::Selectable(strCat(name, " ...").c_str())) {
					createHexEditor(name);
				}
			}
		});
	});
}

void ImGuiDebugger::paint(MSXMotherBoard* motherBoard)
{
	if (!motherBoard) return;

	auto& regs = motherBoard->getCPU().getRegisters();
	auto& cpuInterface = motherBoard->getCPUInterface();
	auto& debugger = motherBoard->getDebugger();
	auto time = motherBoard->getCurrentTime();
	drawControl(cpuInterface);
	drawDisassembly(regs, cpuInterface, debugger, time);
	drawSlots(cpuInterface, debugger);
	drawStack(regs, cpuInterface, time);
	drawRegisters(regs);
	drawFlags(regs);

	// Show the enabled 'hexEditors'
	std::string previousName = "";
	int duplicateNameCount = 0;
	for (auto& [name, editor] : hexEditors) {
		if (name == previousName) {
			++duplicateNameCount;
		} else {
			previousName = name;
			duplicateNameCount = 1;
		}
		if (editor.Open) {
			if (auto* debuggable = debugger.findDebuggable(name)) {
				std::string title = (duplicateNameCount == 1)
					? name
					: strCat(name, "(", duplicateNameCount, ')');
				editor.DrawWindow(title.c_str(), *debuggable);
			}
		}
	}
}

void ImGuiDebugger::drawControl(MSXCPUInterface& cpuInterface)
{
	if (!showControl) return;
	im::Window("Debugger tool bar", &showControl, [&]{
		auto ButtonGlyph = [](const char* id, ImWchar c) {
			const auto* font = ImGui::GetFont();
			auto texId = font->ContainerAtlas->TexID;
			const auto* g = font->FindGlyph(c);
			bool result = ImGui::ImageButton(id, texId, {g->X1 - g->X0, g->Y1 - g->Y0}, {g->U0, g->V0}, {g->U1, g->V1});
			simpleToolTip(id);
			return result;
		};

		bool breaked = cpuInterface.isBreaked();
		if (breaked) {
			if (ButtonGlyph("run", DEBUGGER_ICON_RUN)) {
				cpuInterface.doContinue();
			}
		} else {
			if (ButtonGlyph("break", DEBUGGER_ICON_BREAK)) {
				cpuInterface.doBreak();
			}
		}
		ImGui::SameLine();
		ImGui::SetCursorPosX(50.0f);

		im::Disabled(!breaked, [&]{
			if (ButtonGlyph("step-in", DEBUGGER_ICON_STEP_IN)) {
				cpuInterface.doStep();
			}
			ImGui::SameLine();

			if (ButtonGlyph("step-over", DEBUGGER_ICON_STEP_OVER)) {
				manager.executeDelayed(TclObject("step_over"));
			}
			ImGui::SameLine();

			if (ButtonGlyph("step-out",  DEBUGGER_ICON_STEP_OUT)) {
				manager.executeDelayed(TclObject("step_out"));
			}
			ImGui::SameLine();

			if (ButtonGlyph("step-back", DEBUGGER_ICON_STEP_BACK)) {
				manager.executeDelayed(TclObject("step_back"),
				                       [&](const TclObject&) { syncDisassemblyWithPC = true; });
			}
		});
	});
}

struct CurrentSlot {
	int ps;
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
		Debuggable* romBlocks = nullptr;
		if (const auto* mapper = dynamic_cast<const MSXMemoryMapperBase*>(device)) {
			result.seg = mapper->getSelectedSegment(page);
		} else if (!dynamic_cast<const RomPlain*>(device) &&
		           (romBlocks = debugger.findDebuggable(device->getName() + " romblocks"))) {
			result.seg = romBlocks->read(addr);
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

void ImGuiDebugger::drawDisassembly(CPURegs& regs, MSXCPUInterface& cpuInterface, Debugger& debugger, EmuTime::param time)
{
	if (!showDisassembly) return;
	ImGui::SetNextWindowSize({340, 540}, ImGuiCond_FirstUseEver);
	im::Window("Disassembly", &showDisassembly, [&]{
		std::optional<BreakPoint> addBp;
		std::optional<unsigned> removeBpId;

		auto pc = regs.getPC();
		if (followPC && !cpuInterface.isBreaked()) {
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

			const auto& breakPoints = cpuInterface.getBreakPoints();
			auto bpEt = breakPoints.end();
			auto textSize = ImGui::GetTextLineHeight();

			std::string mnemonic;
			std::string opcodesStr;
			std::array<uint8_t, 4> opcodes;
			ImGuiListClipper clipper; // only draw the actually visible rows
			clipper.Begin(0x10000);
			if (gotoTarget) {
				clipper.IncludeRangeByIndices(*gotoTarget, *gotoTarget + 1);
			}
			std::optional<unsigned> nextGotoTarget;
			while (clipper.Step()) {
				auto bpIt = ranges::lower_bound(breakPoints, clipper.DisplayStart, {}, &BreakPoint::getAddress);
				unsigned addr = instructionBoundary(cpuInterface, clipper.DisplayStart, time);
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
					ImGui::TableNextRow();
					if (addr >= 0x10000) continue;
					im::ID(narrow<int>(addr), [&]{
						if (gotoTarget && addr >= *gotoTarget) {
							gotoTarget = {};
							ImGui::SetScrollHereY(0.25f);
						}

						bool rowAtPc = !syncDisassemblyWithPC && (addr == pc);
						if (rowAtPc) {
							ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, 0x8000ffff);
						}
						if (ImGui::TableNextColumn()) { // bp
							while ((bpIt != bpEt) && (bpIt->getAddress() < addr)) ++bpIt;
							bool hasBp = (bpIt != bpEt) && (bpIt->getAddress() == addr);
							bool multiBp = hasBp && ((bpIt + 1) != bpEt) && ((bpIt + 1)->getAddress() == addr);
							bool simpleBp = !multiBp;
							if (hasBp) {
								ParsedSlotCond bpSlot("pc_in_slot", bpIt->getCondition());
								bool inSlot = addrInSlot(bpSlot, cpuInterface, debugger, addr);
								bool defaultBp = bpSlot.rest.empty() && (bpIt->getCommand() == "debug break");
								simpleBp &= defaultBp;

								auto [r,g,b] = simpleBp ? std::tuple(0xE0, 0x00, 0x00)
											: std::tuple(0xE0, 0xE0, 0x00);
								auto a = inSlot ? 0xFF : 0x80;
								auto col = IM_COL32(r, g, b, a);

								auto* drawList = ImGui::GetWindowDrawList();
								gl::vec2 scrn = ImGui::GetCursorScreenPos();
								auto center = scrn + gl::vec2(textSize * 0.5f);
								drawList->AddCircleFilled(center, textSize * 0.4f, col);
							}
							if (ImGui::InvisibleButton("##bp-button", {-FLT_MIN, textSize})) {
								if (hasBp) {
									// only allow to remove 'simple' breakpoints,
									// others can be edited via the breakpoint viewer
									if (simpleBp) {
										removeBpId = bpIt->getId(); // schedule removal
									}
								} else {
									// schedule creation of new bp
									auto slot = getCurrentSlot(cpuInterface, debugger, addr);
									addBp.emplace(addr, TclObject("debug break"), toTclExpression(slot), false);
								}
							}
						}

						mnemonic.clear();
						std::optional<uint16_t> mnemonicAddr;
						bool mnemonicLabel = false;
						auto len = dasm(cpuInterface, addr, opcodes, mnemonic, time,
							[&](std::string& output, uint16_t a) {
								mnemonicAddr = a;
								if (auto labels = symbolManager.lookupValue(a); !labels.empty()) {
									strAppend(output, labels.front()->name); // TODO cycle
									mnemonicLabel = true;
								} else {
									appendAddrAsHex(output, a);
								}
							});
						assert(len >= 1);

						if (ImGui::TableNextColumn()) { // addr
							// do the full-row-selectable stuff in a column that cannot be hidden
							auto pos = ImGui::GetCursorPos();
							ImGui::Selectable("##row", false,
									ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
							if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
								ImGui::OpenPopup("disassembly-context");
							}
							auto addrStr = tmpStrCat(hex_string<4>(addr));
							im::Popup("disassembly-context", [&]{
								auto setPc = strCat("Set PC to 0x", addrStr);
								if (ImGui::MenuItem(setPc.c_str())) {
									regs.setPC(addr);
								}
								ImGui::Separator();
								if (ImGui::MenuItem("Scroll to PC")) {
									nextGotoTarget = pc;
								}
								im::Indent([&]{
									ImGui::Checkbox("Follow PC while running", &followPC);
								});
								ImGui::AlignTextToFramePadding();
								ImGui::TextUnformatted("Scroll to address:"sv);
								ImGui::SameLine();
								// TODO also allow labels
								if (ImGui::InputText("##goto", &gotoAddr, ImGuiInputTextFlags_EnterReturnsTrue)) {
									if (auto a = symbolManager.parseSymbolOrValue(gotoAddr)) {
										nextGotoTarget = *a;
									}
								}
								simpleToolTip([&]{
									if (auto a = symbolManager.parseSymbolOrValue(gotoAddr)) {
										return strCat("0x", hex_string<4>(*a));
									}
									return std::string{};
								});
							});

							auto addrLabels = symbolManager.lookupValue(addr);
							std::string_view displayAddr = addrLabels.empty() ? std::string_view(addrStr)
							                                                  : std::string_view(addrLabels.front()->name); // TODO cycle
							ImGui::SetCursorPos(pos);
							ImGui::TextUnformatted(displayAddr);
							if (!addrLabels.empty()) {
								simpleToolTip(addrStr);
							}
						}

						if (ImGui::TableNextColumn()) { // opcode
							opcodesStr.clear();
							for (auto i : xrange(len)) {
								strAppend(opcodesStr, hex_string<2>(opcodes[i]), ' ');
							}
							ImGui::TextUnformatted(opcodesStr.data(), opcodesStr.data() + 3 * len - 1);
						}

						if (ImGui::TableNextColumn()) { // mnemonic
							auto pos = ImGui::GetCursorPos();
							ImGui::TextUnformatted(mnemonic);
							if (mnemonicAddr) {
								ImGui::SetCursorPos(pos);
								if (ImGui::InvisibleButton("##mnemonicButton", {-FLT_MIN, textSize})) {
									nextGotoTarget = *mnemonicAddr;
								}
								if (mnemonicLabel) {
									simpleToolTip([&]{ return strCat('#', hex_string<4>(*mnemonicAddr)); });
								}
							}
						}
						addr += len;
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

				ImGui::SetScrollY(topAddr * itemHeight);
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

void ImGuiDebugger::drawSlots(MSXCPUInterface& cpuInterface, Debugger& debugger)
{
	if (!showSlots) return;
	im::Window("Slots", &showSlots, [&]{
		int flags = ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_Reorderable |
			ImGuiTableFlags_Hideable |
			ImGuiTableFlags_ContextMenuInBody;
		im::Table("table", 4, flags, [&]{
			ImGui::TableSetupColumn("Page");
			ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_DefaultHide);
			ImGui::TableSetupColumn("Slot");
			ImGui::TableSetupColumn("Segment");
			ImGui::TableHeadersRow();

			for (auto page : xrange(4)) {
				auto addr = 0x4000 * page;
				if (ImGui::TableNextColumn()) { // page
					ImGui::StrCat(page);
				}
				if (ImGui::TableNextColumn()) { // address
					ImGui::StrCat("0x", hex_string<4>(addr));
				}
				if (ImGui::TableNextColumn()) { // slot
					int ps = cpuInterface.getPrimarySlot(page);
					if (cpuInterface.isExpanded(ps)) {
						int ss = cpuInterface.getSecondarySlot(page);
						ImGui::StrCat(ps, '-', ss);
					} else {
						ImGui::StrCat(' ', ps);
					}
				}
				if (ImGui::TableNextColumn()) { // segment
					auto* device = cpuInterface.getVisibleMSXDevice(page);
					Debuggable* romBlocks = nullptr;
					if (auto* mapper = dynamic_cast<MSXMemoryMapperBase*>(device)) {
						ImGui::StrCat(mapper->getSelectedSegment(page));
					} else if (!dynamic_cast<RomPlain*>(device) &&
						(romBlocks = debugger.findDebuggable(device->getName() + " romblocks"))) {
						std::array<uint8_t, 4> segments;
						for (auto sub : xrange(4)) {
							segments[sub] = romBlocks->read(addr + 0x1000 * sub);
						}
						if ((segments[0] == segments[1]) && (segments[2] == segments[3])) {
							if (segments[0] == segments[2]) { // 16kB
								ImGui::StrCat('R', segments[0]);
							} else { // 8kB
								ImGui::StrCat('R', segments[0], '/', segments[2]);
							}
						} else { // 4kB
							ImGui::StrCat('R', segments[0], '/', segments[1], '/', segments[2], '/', segments[3]);
						}
					} else {
						ImGui::TextUnformatted("-"sv);
					}
				}
			}
		});
	});
}

void ImGuiDebugger::drawStack(CPURegs& regs, MSXCPUInterface& cpuInterface, EmuTime::param time)
{
	if (!showStack) return;

	auto line = ImGui::GetTextLineHeightWithSpacing();
	ImGui::SetNextWindowSize(ImVec2(0.0f, 12 * line), ImGuiCond_FirstUseEver);
	im::Window("stack", &showStack, [&]{
		auto sp = regs.getSP();

		int flags = ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_Reorderable |
			ImGuiTableFlags_Hideable |
			ImGuiTableFlags_ContextMenuInBody;
		im::Table("table", 3, flags, [&]{
			ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
			ImGui::TableSetupColumn("Address");
			ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_DefaultHide);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide);
			ImGui::TableHeadersRow();

			im::ListClipper(std::min(128, (0x10000 - sp) / 2), [&](int row) {
				auto offset = 2 * row;
				auto addr = sp + offset;
				if (ImGui::TableNextColumn()) { // address
					ImGui::StrCat(hex_string<4>(addr));
				}
				if (ImGui::TableNextColumn()) { // offset
					ImGui::Text("SP+%X", offset);
				}
				if (ImGui::TableNextColumn()) { // value
					auto l = cpuInterface.peekMem(narrow_cast<uint16_t>(addr + 0), time);
					auto h = cpuInterface.peekMem(narrow_cast<uint16_t>(addr + 1), time);
					auto value = narrow<uint16_t>(256 * h + l);
					ImGui::StrCat(hex_string<4>(value));
				}
			});
		});
	});
}

void ImGuiDebugger::drawRegisters(CPURegs& regs)
{
	if (!showRegisters) return;
	im::Window("CPU registers", &showRegisters, [&]{
		const auto& style = ImGui::GetStyle();
		auto padding = 2 * style.FramePadding.x;
		auto width16 = ImGui::CalcTextSize("FFFF"sv).x + padding;
		auto edit16 = [&](std::string_view label, auto getter, auto setter) {
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(width16);
			uint16_t value = getter();
			if (ImGui::InputScalar(tmpStrCat("##", label).c_str(), ImGuiDataType_U16, &value, nullptr, nullptr, "%04X")) {
				setter(value);
			}
		};
		auto edit8 = [&](std::string_view label, auto getter, auto setter) {
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
		ImGui::TextUnformatted("IM"sv);
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
	});
}

void ImGuiDebugger::drawFlags(CPURegs& regs)
{
	if (!showFlags) return;
	im::Window("CPU flags", &showFlags, [&]{
		auto sizeH1 = ImGui::CalcTextSize("NC"sv);
		auto sizeH2 = ImGui::CalcTextSize("X:0"sv);
		auto sizeV = ImGui::CalcTextSize("C 0 (NC)"sv);

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

		im::PopupContextWindow([&]{
			ImGui::TextUnformatted("Layout"sv);
			ImGui::RadioButton("horizontal", &flagsLayout, 0);
			ImGui::RadioButton("vertical", &flagsLayout, 1);
			ImGui::Separator();
			ImGui::Checkbox("show undocumented", &showXYFlags);
		});
	});
}

} // namespace openmsx
