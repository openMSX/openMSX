#include "ImGuiDebugger.hh"

#include "DebuggableEditor.hh"
#include "ImGuiBitmapViewer.hh"
#include "ImGuiBreakPoints.hh"
#include "ImGuiCharacter.hh"
#include "ImGuiCpp.hh"
#include "ImGuiDisassembly.hh"
#include "ImGuiManager.hh"
#include "ImGuiPalette.hh"
#include "ImGuiSpriteViewer.hh"
#include "ImGuiSymbols.hh"
#include "ImGuiUtils.hh"
#include "ImGuiVdpRegs.hh"
#include "ImGuiWatchExpr.hh"

#include "Debuggable.hh"
#include "Debugger.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXMemoryMapperBase.hh"
#include "MSXMotherBoard.hh"
#include "MSXRom.hh"
#include "RomBlockDebuggable.hh"
#include "RomInfo.hh"

#include "narrow.hh"
#include "stl.hh"
#include "strCat.hh"
#include "StringOp.hh"

#include <imgui.h>

#include <algorithm>

using namespace std::literals;

namespace openmsx {

ImGuiDebugger::ImGuiDebugger(ImGuiManager& manager_)
	: ImGuiPart(manager_)
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
	static constexpr std::array<ImWchar, 3> ranges = {DEBUGGER_ICON_MIN, DEBUGGER_ICON_MAX, 0};
	io.Fonts->AddFontFromMemoryCompressedBase85TTF(icons_compressed_base85, 20.0f, &icons_config, ranges.data());
}

void ImGuiDebugger::signalBreak()
{
	for (auto& d : disassemblyViewers) {
		d->signalBreak();
	}
}

void ImGuiDebugger::signalContinue()
{
	auto* motherBoard = manager.getReactor().getMotherBoard();
	if (!motherBoard) return;

	cpuRegsSnapshot = motherBoard->getCPU().getRegisters();
	for (auto& h : hexEditors) {
		h->makeSnapshot(*motherBoard);
	}

	showChangesFrameCounter = 10;
}

template<typename T>
static void openOrCreate(ImGuiManager& manager, std::vector<std::unique_ptr<T>>& viewers)
{
	// prefer to reuse a previously closed viewer
	if (auto it = std::ranges::find(viewers, false, &T::show); it != viewers.end()) {
		(*it)->show = true;
		return;
	}
	// or create a new one
	viewers.push_back(std::make_unique<T>(manager, viewers.size()));
}

void ImGuiDebugger::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);

	buf.appendf("disassemblyViewers=%d\n", narrow<int>(disassemblyViewers.size()));
	buf.appendf("bitmapViewers=%d\n", narrow<int>(bitmapViewers.size()));
	buf.appendf("tileViewers=%d\n", narrow<int>(tileViewers.size()));
	buf.appendf("spriteViewers=%d\n", narrow<int>(spriteViewers.size()));

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
	disassemblyViewers.clear();
	bitmapViewers.clear();
	tileViewers.clear();
	spriteViewers.clear();
	hexEditors.clear();
}

void ImGuiDebugger::loadLine(std::string_view name, zstring_view value)
{
	static constexpr std::string_view hexEditorPrefix = "hexEditor.";

	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name == "disassemblyViewers") {
		if (auto r = StringOp::stringTo<unsigned>(value)) {
			repeat(*r, [&] { openOrCreate(manager, disassemblyViewers); });
		}
	} else if (name == "bitmapViewers") {
		if (auto r = StringOp::stringTo<unsigned>(value)) {
			repeat(*r, [&] { openOrCreate(manager, bitmapViewers); });
		}
	} else if (name == "tileViewers") {
		if (auto r = StringOp::stringTo<unsigned>(value)) {
			repeat(*r, [&] { openOrCreate(manager, tileViewers); });
		}
	} else if (name == "spriteViewers") {
		if (auto r = StringOp::stringTo<unsigned>(value)) {
			repeat(*r, [&] { openOrCreate(manager, spriteViewers); });
		}
	} else if (name.starts_with(hexEditorPrefix)) {
		if (auto r = StringOp::stringTo<unsigned>(value)) {
			auto debuggableName = std::string(name.substr(hexEditorPrefix.size()));
			auto [b, e] = std::ranges::equal_range(hexEditors, debuggableName, {}, &DebuggableEditor::getDebuggableName);
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
		auto [b, e] = std::ranges::equal_range(hexEditors, name, {}, &DebuggableEditor::getDebuggableName);
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
		if (ImGui::MenuItem("Disassembly ...")) {
			openOrCreate(manager, disassemblyViewers);
		}
		ImGui::MenuItem("CPU registers", nullptr, &showRegisters);
		ImGui::MenuItem("CPU flags", nullptr, &showFlags);
		ImGui::MenuItem("Slots", nullptr, &showSlots);
		ImGui::MenuItem("Stack", nullptr, &showStack);
		auto it = std::ranges::lower_bound(hexEditors, "memory", {}, &DebuggableEditor::getDebuggableName);
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
		if (ImGui::MenuItem("VDP bitmap viewer ...")) {
			openOrCreate(manager, bitmapViewers);
		}
		if (ImGui::MenuItem("VDP tile viewer ...")) {
			openOrCreate(manager, tileViewers);
		}
		if (ImGui::MenuItem("VDP sprite viewer ...")) {
			openOrCreate(manager, spriteViewers);
		}
		ImGui::MenuItem("VDP register viewer", nullptr, &manager.vdpRegs->show);
		ImGui::MenuItem("Palette editor", nullptr, &manager.palette->window.open);
		ImGui::Separator();
		im::Menu("Add hex editor", [&]{
			const auto& debugger = motherBoard->getDebugger();
			auto debuggables = to_vector<std::pair<std::string, Debuggable*>>(debugger.getDebuggables());
			std::ranges::sort(debuggables, StringOp::caseless{}, [](const auto& p) { return p.first; }); // sort on name
			for (const auto& [name, debuggable] : debuggables) {
				if (ImGui::Selectable(strCat(name, " ...").c_str())) {
					createHexEditor(name);
				}
			}
		});
	});
}

void ImGuiDebugger::setGotoTarget(uint16_t target)
{
	if (disassemblyViewers.empty()) {
		openOrCreate(manager, disassemblyViewers);
	}
	disassemblyViewers.front()->setGotoTarget(target);
}

void ImGuiDebugger::paint(MSXMotherBoard* motherBoard)
{
	if (!motherBoard) return;

	auto& regs = motherBoard->getCPU().getRegisters();
	auto& cpuInterface = motherBoard->getCPUInterface();
	auto& debugger = motherBoard->getDebugger();
	auto time = motherBoard->getCurrentTime();
	drawControl(cpuInterface, *motherBoard);
	drawSlots(cpuInterface, debugger);
	drawStack(regs, cpuInterface, time);
	drawRegisters(regs);
	drawFlags(regs);

	showChangesFrameCounter = std::max(0, showChangesFrameCounter - 1);
}

void ImGuiDebugger::actionBreakContinue(MSXCPUInterface& cpuInterface)
{
	if (MSXCPUInterface::isBreaked()) {
		cpuInterface.doContinue();
	} else {
		cpuInterface.doBreak();
	}
}
void ImGuiDebugger::actionStepIn(MSXCPUInterface& cpuInterface)
{
	cpuInterface.doStep();
}
void ImGuiDebugger::actionStepOver()
{
	manager.executeDelayed(TclObject("step_over"));
}
void ImGuiDebugger::actionStepOut()
{
	manager.executeDelayed(TclObject("step_out"));
}
void ImGuiDebugger::actionStepBack()
{
	manager.executeDelayed(TclObject("step_back"), [&](const TclObject&) {
		for (auto& d : disassemblyViewers) d->syncWithPC();
	});
}

void ImGuiDebugger::checkShortcuts(MSXCPUInterface& cpuInterface, MSXMotherBoard& motherBoard)
{
	using enum Shortcuts::ID;
	const auto& shortcuts = manager.getShortcuts();

	if (shortcuts.checkShortcut(DEBUGGER_BREAK_CONTINUE)) {
		actionBreakContinue(cpuInterface);
	} else if (shortcuts.checkShortcut(DEBUGGER_STEP_IN)) {
		actionStepIn(cpuInterface);
	} else if (shortcuts.checkShortcut(DEBUGGER_STEP_OVER)) {
		actionStepOver();
	} else if (shortcuts.checkShortcut(DEBUGGER_STEP_OUT)) {
		actionStepOut();
	} else if (shortcuts.checkShortcut(DEBUGGER_STEP_BACK)) {
		actionStepBack();
	} else if (shortcuts.checkShortcut(DISASM_TOGGLE_BREAKPOINT)) {
		ImGuiDisassembly::actionToggleBp(motherBoard);
	}
}

void ImGuiDebugger::drawControl(MSXCPUInterface& cpuInterface, MSXMotherBoard& motherBoard)
{
	if (!showControl) return;
	im::Window("Debugger tool bar", &showControl, [&]{
		checkShortcuts(cpuInterface, motherBoard);

		gl::vec2 maxIconSize;
		auto* font = ImGui::GetFont();
		for (auto c : {
			DEBUGGER_ICON_RUN, DEBUGGER_ICON_BREAK,
			DEBUGGER_ICON_STEP_IN, DEBUGGER_ICON_STEP_OVER,
			DEBUGGER_ICON_STEP_OUT, DEBUGGER_ICON_STEP_BACK,
		}) {
			const auto* g = font->FindGlyph(c);
			maxIconSize = max(maxIconSize, gl::vec2{g->X1 - g->X0, g->Y1 - g->Y0});
		}

		auto ButtonGlyph = [&](const char* id, ImWchar glyph, Shortcuts::ID sid) {
			bool result = ButtonWithCenteredGlyph(glyph, maxIconSize);
			simpleToolTip([&]() -> std::string {
				const auto& shortcuts = manager.getShortcuts();
				const auto& shortcut = shortcuts.getShortcut(sid);
				if (shortcut.keyChord == ImGuiKey_None) return id;
				return strCat(id, " (", getKeyChordName(shortcut.keyChord), ')');
			});
			return result;
		};

		bool breaked = MSXCPUInterface::isBreaked();
		using enum Shortcuts::ID;
		if (auto breakContinueIcon = breaked ? DEBUGGER_ICON_RUN : DEBUGGER_ICON_BREAK;
		    ButtonGlyph("run", breakContinueIcon, DEBUGGER_BREAK_CONTINUE)) {
			actionBreakContinue(cpuInterface);
		}
		const auto& style = ImGui::GetStyle();
		ImGui::SameLine(0.0f, 2.0f * style.ItemSpacing.x);

		im::Disabled(!breaked, [&]{
			if (ButtonGlyph("step-in", DEBUGGER_ICON_STEP_IN, DEBUGGER_STEP_IN)) {
				actionStepIn(cpuInterface);
			}
			ImGui::SameLine();

			if (ButtonGlyph("step-over", DEBUGGER_ICON_STEP_OVER, DEBUGGER_STEP_OVER)) {
				actionStepOver();
			}
			ImGui::SameLine();

			if (ButtonGlyph("step-out",  DEBUGGER_ICON_STEP_OUT, DEBUGGER_STEP_OUT)) {
				actionStepOut();
			}
			ImGui::SameLine();

			if (ButtonGlyph("step-back", DEBUGGER_ICON_STEP_BACK, DEBUGGER_STEP_BACK)) {
				actionStepBack();
			}
		});
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
					const auto* device = cpuInterface.getVisibleMSXDevice(page);
					if (const auto* mapper = dynamic_cast<const MSXMemoryMapperBase*>(device)) {
						ImGui::StrCat(mapper->getSelectedSegment(page));
					} else if (auto [rom, romBlocks] = ImGuiDisassembly::getRomBlocks(debugger, device); romBlocks) {
						if (unsigned blockSize = RomInfo::getBlockSize(rom->getRomType())) {
							std::string text;
							char separator = 'R';
							for (int offset = 0; offset < 0x4000; offset += blockSize) {
								text += separator;
								if (auto seg = romBlocks->readExt(addr + offset); seg != unsigned(-1)) {
									strAppend(text, seg);
								} else {
									text += '-';
								}
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

void ImGuiDebugger::drawStack(const CPURegs& regs, const MSXCPUInterface& cpuInterface, EmuTime::param time)
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

bool ImGuiDebugger::needDrawChanges() const
{
	if (showChanges == SHOW_NEVER) return false;
	if (showChanges == SHOW_ALWAYS) return true;
	return MSXCPUInterface::isBreaked() || showChangesFrameCounter;
}

void ImGuiDebugger::drawRegisters(CPURegs& regs)
{
	if (!showRegisters) return;
	im::Window("CPU registers", &showRegisters, [&]{
		std::optional<im::ScopedFont> sf(manager.fontMono);

		if (!cpuRegsSnapshot && needSnapshot()) {
			cpuRegsSnapshot = regs;
		}
		bool drawChanges = cpuRegsSnapshot && needDrawChanges();
		auto color = getChangesColor();

		const auto& style = ImGui::GetStyle();
		auto padding = 2 * style.FramePadding.x;
		auto width16 = ImGui::CalcTextSize("FFFF"sv).x + padding;
		auto edit16 = [&](std::string_view label, std::string_view high, std::string_view low, auto getter, auto setter) {
			auto value = getter(regs);
			im::Group([&]{
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(width16);
				bool changed = drawChanges && (getter(*cpuRegsSnapshot) != value);
				im::StyleColor(changed, ImGuiCol_Text, color, [&]{
					if (ImGui::InputScalar(tmpStrCat("##", label).c_str(), ImGuiDataType_U16, &value, nullptr, nullptr, "%04X")) {
						setter(value);
					}
				});
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
			auto value = getter(regs);
			im::Group([&]{
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(width16);
				bool changed = drawChanges && (getter(*cpuRegsSnapshot) != value);
				im::StyleColor(changed, ImGuiCol_Text, color, [&]{
					if (ImGui::InputScalar(tmpStrCat("##", label).c_str(), ImGuiDataType_U8, &value, nullptr, nullptr, "%02X")) {
						setter(value);
					}
				});
			});
			simpleToolTip([&]{
				return strCat(
					"Bin: ", bin_string<4>(value >> 4), ' ', bin_string<4>(value >> 0), "\n"
					"Dec: ", dec_string<3>(value), '\n');
			});
		};

		edit16("AF", "A", "F", [&](const CPURegs& r) { return r.getAF(); }, [&](uint16_t value) { regs.setAF(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("AF'", "A'", "F'", [&](const CPURegs& r) { return r.getAF2(); }, [&](uint16_t value) { regs.setAF2(value); });

		edit16("BC", "B", "C", [&](const CPURegs& r) { return r.getBC(); }, [&](uint16_t value) { regs.setBC(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("BC'", "B'", "C'", [&](const CPURegs& r) { return r.getBC2(); }, [&](uint16_t value) { regs.setBC2(value); });

		edit16("DE", "D", "E", [&](const CPURegs& r) { return r.getDE(); }, [&](uint16_t value) { regs.setDE(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("DE'", "D'", "E'", [&](const CPURegs& r) { return r.getDE2(); }, [&](uint16_t value) { regs.setDE2(value); });

		edit16("HL", "H", "L", [&](const CPURegs& r) { return r.getHL(); }, [&](uint16_t value) { regs.setHL(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("HL'", "H'", "L'", [&](const CPURegs& r) { return r.getHL2(); }, [&](uint16_t value) { regs.setHL2(value); });

		edit16("IX", "IXh", "IXl", [&](const CPURegs& r) { return r.getIX(); }, [&](uint16_t value) { regs.setIX(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("IY ", "IYh", "IYl", [&](const CPURegs& r) { return r.getIY(); }, [&](uint16_t value) { regs.setIY(value); });

		edit16("PC", "PCh", "PCl", [&](const CPURegs& r) { return r.getPC(); }, [&](uint16_t value) { regs.setPC(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit16("SP ", "SPh", "SPl", [&](const CPURegs& r) { return r.getSP(); }, [&](uint16_t value) { regs.setSP(value); });

		edit8("I ", [&](const CPURegs& r) { return r.getI(); }, [&](uint8_t value) { regs.setI(value); });
		ImGui::SameLine(0.0f, 20.0f);
		edit8("R  ", [&](const CPURegs& r) { return r.getR(); }, [&](uint8_t value) { regs.setR(value); });

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("IM"sv);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(width16);
		uint8_t im = regs.getIM();
		bool imChanged = drawChanges && (im != cpuRegsSnapshot->getIM());
		im::StyleColor(imChanged, ImGuiCol_Text, color, [&]{
			if (ImGui::InputScalar("##IM", ImGuiDataType_U8, &im, nullptr, nullptr, "%d")) {
				if (im <= 2) regs.setIM(im);
			}
		});

		ImGui::SameLine(0.0f, 20.0f);
		ImGui::AlignTextToFramePadding();
		bool ei = regs.getIFF1();
		bool eiChanged = drawChanges && (ei != cpuRegsSnapshot->getIFF1());
		im::StyleColor(eiChanged, ImGuiCol_Text, color, [&]{
			auto size = ImGui::CalcTextSize("EI");
			if (ImGui::Selectable(ei ? "EI" : "DI", false, ImGuiSelectableFlags_AllowDoubleClick, size)) {
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					regs.setIFF1(!ei);
					regs.setIFF2(!ei);
				}
			}
		});

		sf.reset();
		simpleToolTip("double-click to toggle");

		HelpMarker("Click values to edit, double-click EI/DI to toggle.\n"
		           "Right-click to configure",
		           2.0f * ImGui::GetFontSize());

		im::PopupContextWindow([&]{
			configureChangesMenu();
		});
	});
}

void ImGuiDebugger::configureChangesMenu()
{
	ImGui::TextUnformatted("Highlight changes between breaks"sv);
	ImGui::RadioButton("don't show", &showChanges, SHOW_NEVER);
	ImGui::RadioButton("show during break", &showChanges, SHOW_DURING_BREAK);
	ImGui::RadioButton("always show", &showChanges, SHOW_ALWAYS);
	ImGui::ColorEdit4("highlight color", changesColor.data(),
		ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha);
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

		if (!cpuRegsSnapshot && needSnapshot()) {
			cpuRegsSnapshot = regs;
		}
		bool drawChanges = cpuRegsSnapshot && needDrawChanges();
		auto color = getChangesColor();

		auto f = regs.getF();
		auto oldF = drawChanges ? cpuRegsSnapshot->getF() : f;

		auto draw = [&](const char* name, uint8_t bit, const char* val0 = nullptr, const char* val1 = nullptr) {
			std::string s;
			ImVec2 sz;
			bool val = f & bit;
			bool oldVal = oldF & bit;

			if (flagsLayout == 0) {
				// horizontal
				if (val0) {
					s = val ? val1 : val0;
					sz = sizeH1;
				} else {
					s = strCat(name, ':', val ? '1' : '0');
					sz = sizeH2;
				}
			} else {
				// vertical
				s = strCat(name, ' ', val ? '1' : '0');
				if (val0) {
					strAppend(s, " (", val ? val1 : val0, ')');
				}
				sz = sizeV;
			}

			im::StyleColor(val != oldVal, ImGuiCol_Text, color, [&]{
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

		im::Font(manager.fontMono, [&]{
			draw("S", 0x80, " P", " M");
			draw("Z", 0x40, "NZ", " Z");
			if (showXYFlags) draw("Y", 0x20);
			draw("H", 0x10);
			if (showXYFlags) draw("X", 0x08);
			draw("P", 0x04, "PO", "PE");
			draw("N", 0x02);
			draw("C", 0x01, "NC", " C");
		});

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
