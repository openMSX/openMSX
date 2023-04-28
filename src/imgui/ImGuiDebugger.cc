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

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include <cstdint>
#include <vector>

using namespace std::literals;


namespace openmsx {

struct DebuggableEditor : public MemoryEditor
{
	DebuggableEditor() {
		Open = false;
		ReadFn = [](const ImU8* userdata, size_t offset) -> ImU8 {
			auto* debuggable = reinterpret_cast<Debuggable*>(const_cast<ImU8*>(userdata));
			return debuggable->read(narrow<unsigned>(offset));
		};
		WriteFn = [](ImU8* userdata, size_t offset, ImU8 data) -> void {
			auto* debuggable = reinterpret_cast<Debuggable*>(userdata);
			debuggable->write(narrow<unsigned>(offset), data);
		};
	}

	void DrawWindow(const char* title, Debuggable& debuggable, size_t base_display_addr = 0x0000) {
		MemoryEditor::DrawWindow(title, &debuggable, debuggable.getSize(), base_display_addr);
	}
};


ImGuiDebugger::ImGuiDebugger(ImGuiManager& manager_)
	: manager(manager_)
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
	buf.appendf("showControl=%d\n", showControl);
	buf.appendf("showDisassembly=%d\n", showDisassembly);
	buf.appendf("followPC=%d\n", followPC);
	buf.appendf("showRegisters=%d\n", showRegisters);
	buf.appendf("showSlots=%d\n", showSlots);
	buf.appendf("showStack=%d\n", showStack);
	buf.appendf("showFlags=%d\n", showFlags);
	buf.appendf("showXYFlags=%d\n", showXYFlags);
	buf.appendf("flagsLayout=%d\n", flagsLayout);
	for (const auto& [name, editor] : debuggables) {
		buf.appendf("showDebuggable.%s=%d\n", name.c_str(), editor->Open);
	}
}

void ImGuiDebugger::loadLine(std::string_view name, zstring_view value)
{
	static constexpr std::string_view prefix = "showDebuggable.";

	if (name == "showDisassembly") {
		showDisassembly = StringOp::stringToBool(value);
	} else if (name == "followPC") {
		followPC = StringOp::stringToBool(value);
	} else if (name == "showControl") {
		showControl = StringOp::stringToBool(value);
	} else if (name == "showRegisters") {
		showRegisters = StringOp::stringToBool(value);
	} else if (name == "showSlots") {
		showSlots = StringOp::stringToBool(value);
	} else if (name == "showStack") {
		showStack = StringOp::stringToBool(value);
	} else if (name == "showFlags") {
		showFlags = StringOp::stringToBool(value);
	} else if (name == "showXYFlags") {
		showXYFlags = StringOp::stringToBool(value);
	} else if (name == "flagsLayout") {
		if (auto r = StringOp::stringTo<unsigned>(value)) {
			if (*r <= 1) flagsLayout = *r;
		}
	} else if (name.starts_with(prefix)) {
		auto debuggableName = name.substr(prefix.size());
		auto [it, inserted] = debuggables.try_emplace(std::string(debuggableName));
		auto& editor = it->second;
		if (inserted) {
			editor = std::make_unique<DebuggableEditor>();
		}
		editor->Open = StringOp::stringToBool(value);
	}
}

void ImGuiDebugger::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Debugger", motherBoard != nullptr, [&]{
		ImGui::MenuItem("Tool bar", nullptr, &showControl);
		ImGui::MenuItem("Disassembly", nullptr, &showDisassembly);
		ImGui::MenuItem("CPU registers", nullptr, &showRegisters);
		ImGui::MenuItem("CPU flags", nullptr, &showFlags);
		ImGui::MenuItem("Slots", nullptr, &showSlots);
		ImGui::MenuItem("Stack", nullptr, &showStack);
		if (auto* editor = lookup(debuggables, "memory")) {
			ImGui::MenuItem("Memory", nullptr, &(*editor)->Open);
		}
		ImGui::Separator();
		ImGui::MenuItem("Breakpoints", nullptr, &manager.breakPoints.show);
		ImGui::MenuItem("Symbol manager", nullptr, &manager.symbols.show);
		ImGui::Separator();
		ImGui::MenuItem("VDP bitmap viewer", nullptr, &manager.bitmap.showBitmapViewer);
		ImGui::MenuItem("VDP tile viewer", nullptr, &manager.character.show);
		ImGui::MenuItem("Palette editor", nullptr, &manager.palette.show);
		ImGui::MenuItem("TODO several more");
		ImGui::Separator();
		im::Menu("All debuggables", [&]{
			auto& debugger = motherBoard->getDebugger();
			for (auto& [name, editor] : debuggables) {
				if (debugger.findDebuggable(name)) {
					ImGui::MenuItem(name.c_str(), nullptr, &editor->Open);
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

	// Show the enabled 'debuggables'
	for (auto& [name, debuggable] : debugger.getDebuggables()) {
		auto [it, inserted] = debuggables.try_emplace(name);
		auto& editor = it->second;
		if (inserted) {
			editor = std::make_unique<DebuggableEditor>();
			editor->Open = false;
		}
		if (editor->Open) {
			editor->DrawWindow(name.c_str(), *debuggable);
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
				syncDisassemblyWithPC = true;
				manager.executeDelayed(TclObject("step_back"));
			}
		});
	});
}

/** Parses the given string as a hexadecimal integer.
  * TODO move this to StringOp? */
[[nodiscard]] static constexpr std::optional<unsigned> parseHex(std::string_view str)
{
	if (str.empty()) {
		return {};
	}
	unsigned value = 0;
	for (const char c : str) {
		value *= 16;
		if ('0' <= c && c <= '9') {
			value += c - '0';
		} else if ('A' <= c && c <= 'F') {
			value += c - 'A' + 10;
		} else if ('a' <= c && c <= 'f') {
			value += c - 'a' + 10;
		} else {
			return {};
		}
	}
	return value;
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
	im::Window("Disassembly", &showDisassembly, [&]{
		auto pc = regs.getPC();
		if (followPC && !cpuInterface.isBreaked()) {
			gotoTarget = pc;
		}

		// TODO can this be optimized/avoided?
		std::vector<uint16_t> startAddresses;
		unsigned startAddr = 0;
		while (startAddr < 0x10000) {
			auto addr = narrow<uint16_t>(startAddr);
			startAddresses.push_back(addr);
			startAddr += instructionLength(cpuInterface, addr, time);
		}

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
			ImGui::TableSetupColumn("opcode");
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
				clipper.ForceDisplayRangeByIndices(*gotoTarget, *gotoTarget + 1);
			}
			std::optional<unsigned> nextGotoTarget;
			while (clipper.Step()) {
				auto bpIt = ranges::lower_bound(breakPoints, clipper.DisplayStart, {}, &BreakPoint::getAddress);
				auto it = ranges::lower_bound(startAddresses, clipper.DisplayStart);
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
					ImGui::TableNextRow();
					if (it == startAddresses.end()) continue;
					auto addr = *it++;
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
										cpuInterface.removeBreakPoint(bpIt->getId());
									}
								} else {
									// create bp
									auto slot = getCurrentSlot(cpuInterface, debugger, addr);
									BreakPoint newBp(addr, TclObject("debug break"), toTclExpression(slot), false);
									cpuInterface.insertBreakPoint(std::move(newBp));
								}
							}
						}

						mnemonic.clear();
						std::optional<uint16_t> mnemonicAddr;
						bool mnemonicLabel = false;
						auto len = dasm(cpuInterface, addr, opcodes, mnemonic, time,
							[&](std::string& output, uint16_t a) {
								mnemonicAddr = a;
								if (auto label = manager.symbols.lookupValue(a); !label.empty()) {
									output += label;
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
									ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap);
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
								if (ImGui::InputText("##goto", &gotoAddr, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
									if (auto a = parseHex(gotoAddr)) {
										nextGotoTarget = *a;
									}
								}
							});

							auto addrLabel = manager.symbols.lookupValue(addr);
							std::string_view displayAddr = addrLabel.empty() ? addrStr : addrLabel;
							ImGui::SetCursorPos(pos);
							ImGui::TextUnformatted(displayAddr);
							if (!addrLabel.empty()) {
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
									auto str = tmpStrCat('#', hex_string<4>(*mnemonicAddr));
									simpleToolTip(str);
								}
							}
						}
					});
				}
			}
			gotoTarget = nextGotoTarget;
			if (syncDisassemblyWithPC) {
				syncDisassemblyWithPC = false; // only once

				auto itemHeight = ImGui::GetTextLineHeightWithSpacing();
				auto winHeight = ImGui::GetWindowHeight();
				auto lines = size_t(winHeight / itemHeight); // approx

				auto n = std::distance(startAddresses.begin(), ranges::lower_bound(startAddresses, pc));
				auto n2 = std::max(size_t(0), n - lines / 4);
				auto topAddr = startAddresses[n2];

				ImGui::SetScrollY(topAddr * itemHeight);
			}
		});
	});
}

void ImGuiDebugger::drawSlots(MSXCPUInterface& cpuInterface, Debugger& debugger)
{
	if (!showStack) return;
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

			ImGuiListClipper clipper; // only draw the actually visible rows
			clipper.Begin(std::min(128, (0x10000 - sp) / 2));
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
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
				}
			}
		});
	});
}

void ImGuiDebugger::drawRegisters(CPURegs& regs)
{
	if (!showRegisters) return;
	im::Window("CPU registers", &showRegisters, [&]{
		const auto& style = ImGui::GetStyle();
		auto padding = 2 * style.FramePadding.x;
		auto width16 = ImGui::CalcTextSize("FFFF").x + padding;
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
		auto sizeH1 = ImGui::CalcTextSize("NC");
		auto sizeH2 = ImGui::CalcTextSize("X:0");
		auto sizeV = ImGui::CalcTextSize("C 0 (NC)");

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
