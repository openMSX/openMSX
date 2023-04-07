#include "ImGuiDebugger.hh"

#include "ImGuiBitmapViewer.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "CPURegs.hh"
#include "Dasm.hh"
#include "Debugger.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <cstdint>
#include <vector>


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
static const char icons_compressed_base85[2390+1] =
	"7])#######8esT%'/###W),##/l:$#Q6>##SExF>J:9*kB5)=-LNE/1:]n420E`k'9,>>#ErEn/aNV=Bxwn=u5)m<-LN.U.Ya(*HAs:T.55YY#AARm/#-0%JXFW],2euH2Id68%--d<B"
	"dPIG]Wumo%5>oY-TT$=(OO+M$=*m<-h8Bk0<_[FHahdF#n(35&2FC'#j2JuBreIL&4*m<-:xNn/EI[^I].4&#n)m<-jrEn/+>00FD)&,IAqEn/'7YY#[$S+HT@&$MSW:Tn?iW>-HP^8&"
	"-DP31GuI=BddUS%le4R*uIbA#$i0B#m<v<B/%P&#7CpgL;=5,MYkLH--XW/17:r$#%*,##0f1$#X,QP0Gke%#.jd(#i:4gLoqm##;Yu##2fG<-Hk.U.9.`$#C?S>-K&l?-1;o^.>rC$#"
	"fU3B-#YlS.:/5##G`O_.hQvV#.@qW.l+vj#L*m<-e/Vp.CbAD3j+[w'(PkA#Ob5-MF-l)#s1Lv#OYU7#'_L7#PL:kbFLt-$@j9I$uxefL$8T;-cW@fM%.gfLN4$##hEII-pR'8/Q.`$#"
	"betgL;>H&#:&*)#?0f-#Ust.#b;L/#B$Biq%vF:vHG?9/C9F&#lC@fNY3:SMtk1iqTh5&5^-Z7vp[E5vp^r0v^'v/vWqc/v'NNh'3HvD-*5A8TJ:-##(mVE-i&nQNmF6##nv3MM.bI5("
	"F?pi'GULS70nfc2%)###Vc?`a$#nIL;htIL6S@X(bUPk++ub&#Z:4gL@CuM(vQj-$aDSS7O9F`a&rT#$T%Uk+UI$>#)GY##=&nO$>^L7#S<M,#4b.-#Xl?0#+NF:.%QOZ6PX<9/c3rI3"
	"ER7lLjl+G4N]4R*xco>,Ek[s$nbK+*>;2H*`Y0i)#&?)4*Y6R*Z%nG*vcO%6N:GH3JI,>%+str-?@i?#+5gF4x]DD3KU%],4j?T.Jc7C#t3xQ,Z$TE+fMBA,nvPo/U[@>-PM0+*du^.*"
	"O<WR8?/YV.ciBi)6N_V%qMU+mu(`ImieH4o*NrQ0+]eW.W]JL2C)Wp/3%pQ0vqR29EaAIG=3^o8j19'HmsGp8]xf#IjoJp&iF7j:]%ZI)#ZtD#4j1-r%%ej#/U``5+thNMBdaI3Sc(=:"
	"(Qc`5Sj]&?Rt^%8o(k>Hk/u4J9(1E-5TYI+[c?Q/X_J#0npAo8LfIk0xFke)@)juHRtvC+ld:G+Yj$wHcxPN)Bisb3BMte)wB>_7m4Y31js,T%Hl+^.*-.51*7:D7<$oFmv(`ImsU/m8"
	"EdSeG<-To8j+taGC,;N0*HH3'(xY$I14:IV@=<G`olEYRt0lq2AW*i2tDS-=o19;8.Sp((jbISS:-4<-R-6&%f-[uPKvSoRAY^#/[@i?#_N3L#Glih)MM>c4%IuD#KjE.3qSID*F_3T."
	"KkRP/PSs+Mrl(B#:G$98r%Z1ieYV60h''b,h/4D+e<Vg,`6)f)g>xF4;l1T.Nl=8Jn&qo/U4J60$kY59:^_V%nGovR,IxOC&c&q/FOPq&O`@q9OD9t@Fv/f3gtZs0W&nQ:LwOK1_4&q/"
	"rP$L4Y+1#,)o7xt9dG/^E+([.uw-s:jtE*,lF:2FsRM0*E1Mp;-NQ3'/LiwI>?\?L-rjZ0%i1x-)8w@A,%^Qs.Z%?h)cmBu$MXXJ*0Zuk0c;.<.mDBA,#,fh(HVYYuf^3'tq0`E8FsK#$"
	"l)3CEwE]5/s[NT/dW+T8p/T;.>ZY20a8<a46aX:.vohJsvrWv8n]N/2cjw^$,::D7eRvFmuuC.mfxkJsBJ,E*]2_ZdkLufAeI1=#a%f+M'+%V>B)tF%c&nO$OMt$M]em##0lA,MKC-##"
	"<ah3v3Ih>$sH#hL=>d5MV7$##v;GF%kJXO4C%/5#qm[caOCn2L))###sKn-$4JX#$PaW1#b9*$#U-4&#(Q?(#PuJ*##CV,#Kgb.#t4n0#FX#3#.;1R#JSR[#EM,/$?nRfLiQrhLL]-mS"
	"75N$#F`4?-v@g;-g>FgMoH6##CQCANbL?###5<3N*4pfL'1YCNWXQ##Lne/N,@,gL1$A:Nc3*$#'GW)#+MC;$54)=-qK.fMTF6##Q&.BNbL?##OeY5NVRH##>OZ*N+:#gLDgu0N,@,gL"
	"SS_<NmIo9&[73;?,gpKFT@4W1prhaHuo1eG7M#hFulH]FN]8JCTIh%Feg=SI`3q.C9F+QBU(p+D.1kMC)&@X(D1jQaVXHe-*nGSC7;ZhFgLqk1,nXoL_wXrLb@]qLO4d.#uJ6(#Cm[*b"
	"[^es/c:@bHdN4/#d>?L-NNei.Fn@-#afh]%mxtLF//5UCvs.>B,(rE-i<cM:^0[eF/1u'#.mk.#[5C/#-)IqLR`4rL;wXrLX.lrL1x.qLf.K-#J#`5/%$S-#c7]nLNM-X.Q####F/LMT"
	"#]8=Y4$S(#";
static constexpr ImWchar DEBUGGER_ICON_MIN       = 0xea1c;
static constexpr ImWchar DEBUGGER_ICON_MAX       = 0xf203;
static constexpr ImWchar DEBUGGER_ICON_RUN       = 0xea1c;
static constexpr ImWchar DEBUGGER_ICON_BREAK     = 0xea1d;
static constexpr ImWchar DEBUGGER_ICON_STEP_IN   = 0xf200;
static constexpr ImWchar DEBUGGER_ICON_STEP_OUT  = 0xf201;
static constexpr ImWchar DEBUGGER_ICON_STEP_OVER = 0xf202;
static constexpr ImWchar DEBUGGER_ICON_STEP_BACK = 0xf203;

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
	buf.appendf("showRegisters=%d\n", showRegisters);
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
	} else if (name == "showControl") {
		showControl = StringOp::stringToBool(value);
	} else if (name == "showRegisters") {
		showRegisters = StringOp::stringToBool(value);
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
	if (!ImGui::BeginMenu("Debugger", motherBoard != nullptr)) {
		return;
	}
	assert(motherBoard);

	ImGui::MenuItem("Tool bar", nullptr, &showControl);
	ImGui::MenuItem("Disassembly", nullptr, &showDisassembly);
	ImGui::MenuItem("CPU registers", nullptr, &showRegisters);
	ImGui::MenuItem("CPU flags", nullptr, &showFlags);
	ImGui::MenuItem("Stack", nullptr, &showStack);
	ImGui::MenuItem("Memory TODO");
	ImGui::Separator();
	ImGui::MenuItem("VDP bitmap viewer", nullptr, &manager.bitmap.showBitmapViewer);
	ImGui::MenuItem("TODO several more");
	ImGui::Separator();
	if (ImGui::BeginMenu("All debuggables")) {
		auto& debugger = motherBoard->getDebugger();
		for (auto& [name, editor] : debuggables) {
			if (debugger.findDebuggable(name)) {
				ImGui::MenuItem(name.c_str(), nullptr, &editor->Open);
			}
		}
		ImGui::EndMenu();
	}
	ImGui::EndMenu();
}

void ImGuiDebugger::paint(MSXMotherBoard* motherBoard)
{
	if (!motherBoard) return;

	auto& regs = motherBoard->getCPU().getRegisters();
	auto& cpuInterface = motherBoard->getCPUInterface();
	auto time = motherBoard->getCurrentTime();
	drawControl(cpuInterface);
	drawDisassembly(regs, cpuInterface, time);
	drawStack(regs, cpuInterface, time);
	drawRegisters(regs);
	drawFlags(regs);

	// Show the enabled 'debuggables'
	for (auto& [name, debuggable] : motherBoard->getDebugger().getDebuggables()) {
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
	if (!ImGui::Begin("Debugger tool bar", &showControl)) {
		ImGui::End();
		return;
	}

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

	ImGui::BeginDisabled(!breaked);

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
		manager.executeDelayed(TclObject("step_back"));
	}

	ImGui::EndDisabled();

	ImGui::End();
}

void ImGuiDebugger::drawDisassembly(CPURegs& regs, MSXCPUInterface& cpuInterface, EmuTime::param time)
{
	if (!showDisassembly) return;
	if (!ImGui::Begin("Disassembly", &showDisassembly)) {
		ImGui::End();
		return;
	}

	auto pc = regs.getPC();

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
	            ImGuiTableFlags_ScrollX;
	if (ImGui::BeginTable("table", 5, flags)) {
		ImGui::TableSetupColumn("bp", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("label");
		ImGui::TableSetupColumn("address");
		ImGui::TableSetupColumn("opcode");
		ImGui::TableSetupColumn("mnemonic", ImGuiTableColumnFlags_WidthStretch);

		std::string mnemonic;
		std::string opcodesStr;
		std::array<uint8_t, 4> opcodes;
		ImGuiListClipper clipper; // only draw the actually visible rows
		clipper.Begin(0x10000);
		while (clipper.Step()) {
			auto it = ranges::lower_bound(startAddresses, clipper.DisplayStart);
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
				ImGui::TableNextRow();
				if (it == startAddresses.end()) continue;
				auto addr = *it++;

				ImGui::TableSetColumnIndex(0); // bp
				if (!syncDisassemblyWithPC && (addr == pc)) {
					ImGui::Selectable("##row", true,
					                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap);
				}

				mnemonic.clear();
				auto len = dasm(cpuInterface, addr, opcodes, mnemonic, time);
				assert(len >= 1);

				ImGui::TableSetColumnIndex(2); // addr
				auto addrStr = tmpStrCat(hex_string<4>(addr));
				ImGui::TextUnformatted(addrStr.c_str());

				ImGui::TableSetColumnIndex(3); // opcode
				opcodesStr.clear();
				for (auto i : xrange(len)) {
					strAppend(opcodesStr, hex_string<2>(opcodes[i]), ' ');
				}
				ImGui::TextUnformatted(opcodesStr.data(), opcodesStr.data() + 3 * len - 1);

				ImGui::TableSetColumnIndex(4); // mnemonic
				ImGui::TextUnformatted(mnemonic.c_str());
			}
		}
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
		ImGui::EndTable();
	}

	ImGui::End();
}

void ImGuiDebugger::drawStack(CPURegs& regs, MSXCPUInterface& cpuInterface, EmuTime::param time)
{
	if (!showStack) return;

	auto line = ImGui::GetTextLineHeightWithSpacing();
	ImGui::SetNextWindowSize(ImVec2(0.0f, 12 * line), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("stack", &showStack)) {
		ImGui::End();
		return;
	}

	auto sp = regs.getSP();

	int flags = ImGuiTableFlags_ScrollY |
	            ImGuiTableFlags_BordersInnerV |
	            ImGuiTableFlags_Resizable |
	            ImGuiTableFlags_Reorderable |
	            ImGuiTableFlags_Hideable |
	            ImGuiTableFlags_ContextMenuInBody;
	if (ImGui::BeginTable("table", 3, flags)) {
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
					ImGui::Text("%04X", addr);
				}
				if (ImGui::TableNextColumn()) { // offset
					ImGui::Text("SP+%X", offset);
				}
				if (ImGui::TableNextColumn()) { // value
					auto l = cpuInterface.peekMem(narrow_cast<uint16_t>(addr + 0), time);
					auto h = cpuInterface.peekMem(narrow_cast<uint16_t>(addr + 1), time);
					auto value = narrow<uint16_t>(256 * h + l);
					ImGui::Text("%04X", value);
				}
			}
		}
		ImGui::EndTable();
	}

	ImGui::End();
}

void ImGuiDebugger::drawRegisters(CPURegs& regs)
{
	if (!showRegisters) return;
	if (!ImGui::Begin("CPU registers", &showRegisters)) {
		ImGui::End();
		return;
	}

	const auto& style = ImGui::GetStyle();
	auto padding = 2 * style.FramePadding.x;
	auto width16 = ImGui::CalcTextSize("FFFF").x + padding;
	auto edit16 = [&](const char* label, auto getter, auto setter) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(width16);
		uint16_t value = getter();
		if (ImGui::InputScalar(tmpStrCat("##", label).c_str(), ImGuiDataType_U16, &value, nullptr, nullptr, "%04X")) {
			setter(value);
		}
	};
	auto edit8 = [&](const char* label, auto getter, auto setter) {
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
	ImGui::TextUnformatted("IM");
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

	ImGui::End();
}

void ImGuiDebugger::drawFlags(CPURegs& regs)
{
	if (!showFlags) return;
	if (!ImGui::Begin("CPU flags", &showFlags)) {
		ImGui::End();
		return;
	}

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

	if (ImGui::BeginPopupContextWindow()) {
		ImGui::TextUnformatted("Layout");
		ImGui::RadioButton("horizontal", &flagsLayout, 0);
		ImGui::RadioButton("vertical", &flagsLayout, 1);
		ImGui::Separator();
		ImGui::Checkbox("show undocumented", &showXYFlags);
		ImGui::EndPopup();
	}

	ImGui::End();
}

} // namespace openmsx
