#include "ImGuiDebugger.hh"

#include "ImGuiBitmapViewer.hh"
#include "ImGuiBreakPoints.hh"
#include "ImGuiCharacter.hh"
#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiPalette.hh"
#include "ImGuiSpriteViewer.hh"
#include "ImGuiSymbols.hh"
#include "ImGuiUtils.hh"
#include "ImGuiVdpRegs.hh"
#include "ImGuiWatchExpr.hh"

#include "CPURegs.hh"
#include "Dasm.hh"
#include "Debuggable.hh"
#include "Debugger.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXMemoryMapperBase.hh"
#include "MSXMotherBoard.hh"
#include "RomPlain.hh"
#include "RomInfo.hh"
#include "SymbolManager.hh"

#include "narrow.hh"
#include "join.hh"
#include "ranges.hh"
#include "stl.hh"
#include "strCat.hh"
#include "StringOp.hh"
#include "view.hh"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cstdint>
#include <vector>

using namespace std::literals;

namespace openmsx {

ImGuiDebugger::ImGuiDebugger(ImGuiManager& manager_)
	: ImGuiPart(manager_)
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

	// TODO in the future use c++23 std::chunk_by
	auto it = hexEditors.begin();
	auto et = hexEditors.end();
	while (it != et) {
		int count = 0;
		auto name = std::string((*it)->getDebuggableName());
		do {
			++it;
			++count;
		} while (it != et && (*it)->getDebuggableName() == name);
		buf.appendf("hexEditor.%s=%d\n", name.c_str(), count);
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
		if (auto r = StringOp::stringTo<unsigned>(value)) {
			auto debuggableName = std::string(name.substr(prefix.size()));
			auto [b, e] = ranges::equal_range(hexEditors, debuggableName, {}, &DebuggableEditor::getDebuggableName);
			auto index = std::distance(b, e); // expected to be 0, but be robust against imgui.ini changes
			for (auto i : xrange(*r)) {
				e = hexEditors.insert(e, std::make_unique<DebuggableEditor>(manager, debuggableName, index + i));
				++e;
			}
		}
	}
}

void ImGuiDebugger::showMenu(MSXMotherBoard* motherBoard)
{
	auto createHexEditor = [&](const std::string& name) {
		// prefer to reuse a previously closed editor
		auto [b, e] = ranges::equal_range(hexEditors, name, {}, &DebuggableEditor::getDebuggableName);
		for (auto it = b; it != e; ++it) {
			if (!(*it)->open) {
				(*it)->open = true;
				return;
			}
		}
		// or create a new one
		auto index = std::distance(b, e);
		auto it = hexEditors.insert(e, std::make_unique<DebuggableEditor>(manager, name, index));
		(*it)->open = true;
	};

	im::Menu("Debugger", motherBoard != nullptr, [&]{
		ImGui::MenuItem("Tool bar", nullptr, &showControl);
		ImGui::MenuItem("Disassembly", nullptr, &showDisassembly);
		ImGui::MenuItem("CPU registers", nullptr, &showRegisters);
		ImGui::MenuItem("CPU flags", nullptr, &showFlags);
		ImGui::MenuItem("Slots", nullptr, &showSlots);
		ImGui::MenuItem("Stack", nullptr, &showStack);
		auto it = ranges::lower_bound(hexEditors, "memory", {}, &DebuggableEditor::getDebuggableName);
		bool memoryOpen = (it != hexEditors.end()) && (*it)->open;
		if (ImGui::MenuItem("Memory", nullptr, &memoryOpen)) {
			if (memoryOpen) {
				createHexEditor("memory");
			} else {
				assert(it != hexEditors.end());
				(*it)->open = false;
			}
		}
		ImGui::Separator();
		ImGui::MenuItem("Breakpoints", nullptr, &manager.breakPoints->show);
		ImGui::MenuItem("Symbol manager", nullptr, &manager.symbols->show);
		ImGui::MenuItem("Watch expression", nullptr, &manager.watchExpr->show);
		ImGui::Separator();
		ImGui::MenuItem("VDP bitmap viewer", nullptr, &manager.bitmap->showBitmapViewer);
		ImGui::MenuItem("VDP tile viewer", nullptr, &manager.character->show);
		ImGui::MenuItem("VDP sprite viewer", nullptr, &manager.sprite->show);
		ImGui::MenuItem("VDP register viewer", nullptr, &manager.vdpRegs->show);
		ImGui::MenuItem("Palette editor", nullptr, &manager.palette->window.open);
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
			result.seg = mapper->getSelectedSegment(narrow<uint8_t>(page));
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

struct BpLine {
	int count = 0;
	int idx = -1; // only valid when count=1
	bool anyEnabled = false;
	bool anyDisabled = false;
	bool anyInSlot = false;
	bool anyComplex = false;
};
static BpLine examineBpLine(uint16_t addr, std::span<const ImGuiBreakPoints::GuiItem> bps, MSXCPUInterface& cpuInterface, Debugger& debugger)
{
	BpLine result;
	for (auto [i, bp] : enumerate(bps)) {
		if (!bp.addr || *bp.addr != addr) continue;
		++result.count;
		result.idx = int(i);

		bool enabled = (bp.id > 0) && bp.wantEnable;
		result.anyEnabled |= enabled;
		result.anyDisabled |= !enabled;

		ParsedSlotCond bpSlot("pc_in_slot", bp.cond.getString());
		result.anyInSlot |= addrInSlot(bpSlot, cpuInterface, debugger, addr);

		result.anyComplex |= !bpSlot.rest.empty() || (bp.cmd.getString() != "debug break");
	}
	return result;
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

			auto& guiBps = manager.breakPoints->getBps(cpuInterface);
			auto textSize = ImGui::GetTextLineHeight();

			std::string mnemonic;
			std::string opcodesStr;
			std::array<uint8_t, 4> opcodes;
			ImGuiListClipper clipper; // only draw the actually visible rows
			clipper.Begin(0x10000);
			if (gotoTarget) {
				clipper.IncludeItemsByIndex(*gotoTarget, *gotoTarget + 4);
			}
			std::optional<unsigned> nextGotoTarget;
			while (clipper.Step()) {
				auto addr16 = instructionBoundary(cpuInterface, narrow<uint16_t>(clipper.DisplayStart), time);
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
					unsigned addr = addr16;
					ImGui::TableNextRow();
					if (addr >= 0x10000) continue;
					im::ID(narrow<int>(addr), [&]{
						if (gotoTarget && addr >= *gotoTarget) {
							gotoTarget = {};
							ImGui::SetScrollHereY(0.25f);
						}

						bool rowAtPc = !syncDisassemblyWithPC && (addr == pc);
						if (rowAtPc) {
							ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, getColor(imColor::YELLOW_BG));
						}
						bool bpRightClick = false;
						if (ImGui::TableNextColumn()) { // bp
							auto bpLine = examineBpLine(addr16, guiBps, cpuInterface, debugger);
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
								if (hasBp) {
									// only allow to remove single breakpoints,
									// others can be edited via the breakpoint viewer
									if (!multi) {
										auto& bp = guiBps[bpLine.idx];
										if (bp.id > 0) {
											removeBpId = bp.id; // schedule removal
										} else {
											guiBps.erase(guiBps.begin() + bpLine.idx);
										}
									}
								} else {
									// schedule creation of new bp
									auto slot = getCurrentSlot(cpuInterface, debugger, addr16);
									addBp.emplace(addr, TclObject("debug break"), toTclExpression(slot), false);
								}
							} else {
								bpRightClick = hasBp && ImGui::IsItemClicked(ImGuiMouseButton_Right);
								if (bpRightClick) ImGui::OpenPopup("bp-context");
								im::Popup("bp-context", [&]{
									manager.breakPoints->paintBpTab(cpuInterface, debugger, addr16);
								});
							}
						}

						mnemonic.clear();
						std::optional<uint16_t> mnemonicAddr;
						std::span<const Symbol* const> mnemonicLabels;
						auto len = dasm(cpuInterface, addr16, opcodes, mnemonic, time,
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
								view::transform(xrange(len),
									[&](unsigned i) { return strCat('#', hex_string<2>(opcodes[i])); }),
								','));
						}

						if (ImGui::TableNextColumn()) { // addr
							// do the full-row-selectable stuff in a column that cannot be hidden
							auto pos = ImGui::GetCursorPos();
							ImGui::Selectable("##row", false,
									ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
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
								});

								ImGui::AlignTextToFramePadding();
								ImGui::TextUnformatted("Scroll to address:"sv);
								ImGui::SameLine();
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
							});

							auto addrLabels = symbolManager.lookupValue(addr16);
							std::string_view displayAddr = addrLabels.empty()
								? std::string_view(addrStr)
								: std::string_view(addrLabels[cycleLabelsCounter % addrLabels.size()]->name);
							ImGui::SetCursorPos(pos);
							im::Font(manager.fontMono, [&]{
								ImGui::TextUnformatted(displayAddr);
							});
							if (!addrLabels.empty()) {
								simpleToolTip([&]{
									std::string tip(addrStr);
									if (addrLabels.size() > 1) {
										strAppend(tip, "\nmultiple possibilities (click to cycle):\n",
											join(view::transform(addrLabels, &Symbol::name), ' '));
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
								ImGui::TextUnformatted(opcodesStr.data(), opcodesStr.data() + 3 * len - 1);
							});
						}

						if (ImGui::TableNextColumn()) { // mnemonic
							auto pos = ImGui::GetCursorPos();
							im::Font(manager.fontMono, [&]{
								ImGui::TextUnformatted(mnemonic);
							});
							if (mnemonicAddr) {
								ImGui::SetCursorPos(pos);
								if (ImGui::InvisibleButton("##mnemonicButton", {-FLT_MIN, textSize})) {
									if (!mnemonicLabels.empty()) {
										++cycleLabelsCounter;
									}
								}
								if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
									nextGotoTarget = *mnemonicAddr;
								}
								if (!mnemonicLabels.empty()) {
									simpleToolTip([&]{
										auto tip = strCat('#', hex_string<4>(*mnemonicAddr));
										if (mnemonicLabels.size() > 1) {
											strAppend(tip, "\nmultiple possibilities (click to cycle):\n",
												join(view::transform(mnemonicLabels, &Symbol::name), ' '));
										}
										return tip;
									});
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

			for (auto page : xrange(uint8_t(4))) {
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
					MSXRom *rom = nullptr;
					if (auto* mapper = dynamic_cast<MSXMemoryMapperBase*>(device)) {
						ImGui::StrCat(mapper->getSelectedSegment(page));
					} else if ((rom = dynamic_cast<MSXRom*>(device)) &&
						(romBlocks = debugger.findDebuggable(device->getName() + " romblocks"))) {
						unsigned blockSize = RomInfo::getBlockSize(rom->getRomType());
						// Mirrored ROMs are not mappers, so displaying segments doesn't make sense.
						if (rom->getRomType() != openmsx::ROM_MIRRORED && blockSize > 0) {
							std::string text;
							char separator = 'R';
							for (int offset = 0; offset < 0x4000; offset += blockSize) {
								strAppend(text, separator, romBlocks->read(addr + offset));
								separator = '/';
							}
							ImGui::TextUnformatted(text);
						} else {
							ImGui::TextUnformatted("-"sv);
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

			im::ScopedFont sf(manager.fontMono);
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
		im::ScopedFont sf(manager.fontMono);

		const auto& style = ImGui::GetStyle();
		auto padding = 2 * style.FramePadding.x;
		auto width16 = ImGui::CalcTextSize("FFFF"sv).x + padding;
		auto edit16 = [&](std::string_view label, std::string_view high, std::string_view low, auto getter, auto setter) {
			uint16_t value = getter();
			im::Group([&]{
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(width16);
				if (ImGui::InputScalar(tmpStrCat("##", label).c_str(), ImGuiDataType_U16, &value, nullptr, nullptr, "%04X")) {
					setter(value);
				}
			});
			simpleToolTip([&]{
				return strCat(
					"Bin: ", bin_string<4>(value >> 12), ' ', bin_string<4>(value >>  8), ' ',
					         bin_string<4>(value >>  4), ' ', bin_string<4>(value >>  0), "\n"
					"Dec: ", dec_string<5>(value), '\n',
					high, ": ", dec_string<3>(value >> 8), "  ", low, ": ", dec_string<3>(value & 0xff));
			});
		};
		auto edit8 = [&](std::string_view label, auto getter, auto setter) {
			uint8_t value = getter();
			im::Group([&]{
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(width16);
				if (ImGui::InputScalar(tmpStrCat("##", label).c_str(), ImGuiDataType_U8, &value, nullptr, nullptr, "%02X")) {
					setter(value);
				}
			});
			simpleToolTip([&]{
				return strCat(
					"Bin: ", bin_string<4>(value >> 4), ' ', bin_string<4>(value >> 0), "\n"
					"Dec: ", dec_string<3>(value), '\n');
			});
		};

		edit16("AF", "A", "F", [&]{ return regs.getAF(); }, [&](uint16_t value) { regs.setAF(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("AF'", "A'", "F'", [&]{ return regs.getAF2(); }, [&](uint16_t value) { regs.setAF2(value); });

		edit16("BC", "B", "C", [&]{ return regs.getBC(); }, [&](uint16_t value) { regs.setBC(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("BC'", "B'", "C'", [&]{ return regs.getBC2(); }, [&](uint16_t value) { regs.setBC2(value); });

		edit16("DE", "D", "E", [&]{ return regs.getDE(); }, [&](uint16_t value) { regs.setDE(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("DE'", "D'", "E'", [&]{ return regs.getDE2(); }, [&](uint16_t value) { regs.setDE2(value); });

		edit16("HL", "H", "L", [&]{ return regs.getHL(); }, [&](uint16_t value) { regs.setHL(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("HL'", "H'", "L'", [&]{ return regs.getHL2(); }, [&](uint16_t value) { regs.setHL2(value); });

		edit16("IX", "IXh", "IXl", [&]{ return regs.getIX(); }, [&](uint16_t value) { regs.setIX(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("IY ", "IYh", "IYl", [&]{ return regs.getIY(); }, [&](uint16_t value) { regs.setIY(value); });

		edit16("PC", "PCh", "PCl", [&]{ return regs.getPC(); }, [&](uint16_t value) { regs.setPC(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("SP ", "SPh", "SPl", [&]{ return regs.getSP(); }, [&](uint16_t value) { regs.setSP(value); });

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
		auto [sizeH1_, sizeH2_, sizeV_] = [&]{
			im::ScopedFont sf(manager.fontMono);
			return std::tuple{
				ImGui::CalcTextSize("NC"sv),
				ImGui::CalcTextSize("X:0"sv),
				ImGui::CalcTextSize("C 0 (NC)"sv)
			};
		}();
		// clang workaround
		auto sizeH1 = sizeH1_; auto sizeH2 = sizeH2_; auto sizeV = sizeV_;

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
			im::Font(manager.fontMono, [&]{
				if (ImGui::Selectable(s.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick, sz)) {
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
						regs.setF(f ^ bit);
					}
				}
			});
			simpleToolTip("double-click to toggle, right-click to configure");
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
