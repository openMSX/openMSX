#include "ImGuiMsxMusicViewer.hh"

#include "ImGuiPlot.hh"

#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "YM2413.hh"

#include "cstd.hh"
#include "zstring_view.hh"

#include <bit>

namespace openmsx {

using namespace std::literals;

ImGuiMsxMusicViewer::ImGuiMsxMusicViewer(ImGuiManager& manager_)
	: ImGuiPart(manager_)
{
}

bool ImGuiMsxMusicViewer::match(const HoverList& list, Hovers hovers)
{
	return std::ranges::any_of(list, [&](Hovers h){ return hovers.match(h); });
}

// Returns true if the mouse is hovering the entire current table cell.
//
// Requirements:
// - Must be called *after* ImGui::TableNextColumn()
// - Must be called *before* rendering any content in the cell
// - Assumes the cell contains only a single line of text.
// - Assumes no custom row height or multi-line widgets.
static bool IsTextCellHovered(float width = 0.0f)
{
	gl::vec2 topLeft = ImGui::GetCursorScreenPos();
	if (width <= 0.0f) width = ImGui::GetContentRegionAvail().x;
	auto height = ImGui::GetTextLineHeightWithSpacing(); // assumes single-line cell
	auto bottomRight = topLeft + gl::vec2{width, height};
	return ImGui::IsMouseHoveringRect(topLeft, bottomRight);
}

void ImGuiMsxMusicViewer::drawCell(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t> regs, Hovers hovers,
                                   std::function<void(uint32_t)> func,
                                   float highlightWidth, gl::vec2* getPos)
{
	if (!ImGui::TableNextColumn()) return;

	if (getPos) *getPos = ImGui::GetCursorScreenPos();
	if (IsTextCellHovered(highlightWidth)) {
		newHoverList.push_back(hovers);
	}
	auto color = [&]{
		if (match(cs.hoverList, hovers)) {
			const auto& style = ImGui::GetStyle();
			if (highlightWidth <= 0.0f) {
				highlightWidth = ImGui::GetContentRegionAvail().x + 2.0f * style.CellPadding.x;
			}
			gl::vec2 size = {highlightWidth, ImGui::GetTextLineHeightWithSpacing()};
			gl::vec2 tl = ImGui::GetCursorScreenPos() - gl::vec2(style.CellPadding);
			gl::vec2 br = tl + size;
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(tl, br, getColor(imColor::YELLOW));
			return getColor(imColor::BLACK);
		}
		return getColor(imColor::TEXT);
	}();

	auto extract = [&](const Hover& h) {
		return (regs[h.reg] & h.mask) >> std::countr_zero(h.mask);
	};
	uint32_t value = 0;
	if (hovers.h1.reg != 255) {
		value = extract(hovers.h1);
	}
	if (hovers.h2.reg != 255) {
		assert(hovers.h1.mask == 255);
		value |= extract(hovers.h2) << 8;
	}

	im::StyleColor(ImGuiCol_Text, color, [&]{ func(value); });
}

static constexpr std::array<zstring_view, 16> instrNames = {
	"Custom",
	"Violin",
	"Guitar",
	"Piano",
	"Flute",
	"Clarinet",
	"Oboe",
	"Trumpet",
	"Organ",
	"Horn",
	"Synthesizer",
	"Harpsichord",
	"Vibraphone",
	"Synthesizer Bass",
	"Acoustic Bass",
	"Electric Guitar",
};

void ImGuiMsxMusicViewer::paintMelodicChannel(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t, 64> regs, int ch)
{
	auto r10 = uint8_t(ch + 0x10);
	auto r20 = uint8_t(ch + 0x20);
	auto r30 = uint8_t(ch + 0x30);
	auto v10 = regs[r10];
	auto v20 = regs[r20];

	bool key = (v20 & 0x10) != 0;
	auto& kh = cs.keyHistory[ch];
	kh.back() = key ? 1.0f : 0.0f;

	auto fnum = ((v20 & 0x01) << 8) | v10;
	auto block = (v20 & 0x0e) >> 1;
	auto freq = float(fnum << block) * (3579545.0f / float(72 << 19));

	if (ImGui::TableNextColumn()) { // channel
		ImGui::StrCat(ch + 1);
	}
	drawCell(cs, newHoverList, regs, {r20, 0x20}, [&](uint32_t v) { // sustain
		ImGui::TextUnformatted(v ? "on"sv : "off"sv);
	});
	Hover keyH{r20, 0x10};
	drawCell(cs, newHoverList, regs, keyH, [&](uint32_t) { // key
		ImGui::TextUnformatted(key ? "on"sv : "off"sv);
	});
	drawCell(cs, newHoverList, regs, keyH, [&](uint32_t) { // key plot
		gl::vec2 size = {historySize, ImGui::GetFontSize()};
		auto topLeft = ImGui::GetCursorScreenPos();
		auto bottomRight = topLeft + size;
		ImGui::Dummy(size);
		plotLinesRaw(kh, 0.0f, 1.0f, topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_Text));
	});
	Hovers freqH = {r10, 0xff, r20, 0x0f};
	drawCell(cs, newHoverList, regs, freqH, [&](uint32_t) { // frequency
		ImGui::StrCat(std::lround(freq));
	});
	drawCell(cs, newHoverList, regs, freqH, [&](uint32_t) { // note
		ImGui::TextUnformatted(freq2note(freq));
	});
	drawCell(cs, newHoverList, regs, {r30, 0x0f}, [&](uint32_t v) { // volume
		ImGui::StrCat('-', v * 3); // show '0' as '-0'
	});
	drawCell(cs, newHoverList, regs, {r30, 0xf0}, [&](uint32_t v) { // instrument
		ImGui::StrCat('(', hex_string<1>(v), ") ", instrNames[v]);
	});
}

void ImGuiMsxMusicViewer::paintRhythmChannel(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t, 64> regs, int ch)
{
	auto mask = [&]{
		if      (ch == 0) return uint8_t(0x10); // bass
		else if (ch == 1) return uint8_t(0x08); // snare
		else if (ch == 2) return uint8_t(0x02); // cymbal
		else if (ch == 3) return uint8_t(0x01); // hi-hat
		else              return uint8_t(0x04); // tom
	}();
	bool key = (regs[0x0e] & mask) != 0;
	auto& kh = cs.keyHistory[ch + 9];
	kh.back() = key ? 1.0f : 0.0f;

	if (ImGui::TableNextColumn()) { // channel
		static constexpr std::array<std::string_view, 5> names = {
			"Bass drum",
			"Snare drum",
			"Top Cymbal",
			"High hat",
			"Tom-tom",
		};
		ImGui::TextUnformatted(names[ch]);
	}
	Hovers keyH = {0x0e, mask};
	drawCell(cs, newHoverList, regs, keyH, [&](uint32_t) { // key
		ImGui::TextUnformatted(key ? "on"sv : "off"sv);
	});
	drawCell(cs, newHoverList, regs, keyH, [&](uint32_t) { // key plot
		gl::vec2 size = {historySize, ImGui::GetFontSize()};
		auto topLeft = ImGui::GetCursorScreenPos();
		auto bottomRight = topLeft + size;
		ImGui::Dummy(size);
		plotLinesRaw(kh, 0.0f, 1.0f, topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_Text));
	});
	auto volH = [&]{
		if      (ch == 0) return Hover{0x36, 0x0f}; // bass
		else if (ch == 1) return Hover{0x37, 0x0f}; // snare
		else if (ch == 2) return Hover{0x38, 0x0f}; // cymbal
		else if (ch == 3) return Hover{0x37, 0xf0}; // hi-hat
		else              return Hover{0x38, 0xf0}; // tom
	}();
	drawCell(cs, newHoverList, regs, volH, [&](uint32_t v) { // volume
		ImGui::StrCat('-', v * 3); // show '0' as '-0'
	});
}

void ImGuiMsxMusicViewer::paintChannels(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t, 64> regs)
{
	bool rhythm = (regs[0x0e] & 0x20) != 0;
	auto numChannels = rhythm ? 6 : 9;

	int flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_Reorderable |
		ImGuiTableFlags_Hideable |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
	im::Table("##melodic", 8, flags, [&]{
		ImGui::TableSetupColumn("Ch.");
		ImGui::TableSetupColumn("Sustain");
		ImGui::TableSetupColumn("Key");
		ImGui::TableSetupColumn("Key-plot", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, historySize);
		ImGui::TableSetupColumn("Freq [Hz]", ImGuiTableColumnFlags_DefaultHide);
		ImGui::TableSetupColumn("Note");
		ImGui::TableSetupColumn("Vol [dB]");
		auto instrNameWidth = ImGui::CalcTextSize("(13) ").x + ImGui::CalcTextSize(instrNames[13]).x;
		ImGui::TableSetupColumn("Instrument", ImGuiTableColumnFlags_WidthFixed, instrNameWidth);
		ImGui::TableHeadersRow();

		im::ID_for_range(numChannels, [&](auto ch){
			paintMelodicChannel(cs, newHoverList, regs, ch);
		});
	});
	if (!rhythm) return;
	im::Table("##rhythm", 4, flags, [&]{
		ImGui::TableSetupColumn("Rhythm");
		ImGui::TableSetupColumn("Key");
		ImGui::TableSetupColumn("Key-plot", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, historySize);
		ImGui::TableSetupColumn("Vol [dB]");
		ImGui::TableHeadersRow();

		im::ID_for_range(5, [&](auto ch){
			paintRhythmChannel(cs, newHoverList, regs, ch);
		});
	});
}

static constexpr std::array<std::array<uint8_t, 8>, 15> fixedInstruments = {
	std::array<uint8_t, 8>{0x71, 0x61, 0x1e, 0x17, 0xd0, 0x78, 0x00, 0x17}, // Violin
	std::array<uint8_t, 8>{0x13, 0x41, 0x1a, 0x0d, 0xd8, 0xf7, 0x23, 0x13}, // Guitar
	std::array<uint8_t, 8>{0x13, 0x01, 0x89, 0x00, 0xf2, 0xc4, 0x11, 0x23}, // Piano
	std::array<uint8_t, 8>{0x31, 0x61, 0x0e, 0x07, 0xa8, 0x64, 0x70, 0x27}, // Flute
	std::array<uint8_t, 8>{0x32, 0x21, 0x1e, 0x06, 0xe0, 0x76, 0x00, 0x28}, // Clarinet
	std::array<uint8_t, 8>{0x31, 0x22, 0x16, 0x05, 0xe0, 0x71, 0x00, 0x18}, // Oboe
	std::array<uint8_t, 8>{0x21, 0x61, 0x1d, 0x07, 0x82, 0x81, 0x10, 0x07}, // Trumpet
	std::array<uint8_t, 8>{0x23, 0x21, 0x2d, 0x14, 0xa2, 0x72, 0x00, 0x07}, // Organ
	std::array<uint8_t, 8>{0x61, 0x61, 0x1b, 0x06, 0x64, 0x65, 0x10, 0x17}, // Horn
	std::array<uint8_t, 8>{0x41, 0x61, 0x0b, 0x18, 0x85, 0xf7, 0x71, 0x07}, // Synthesizer
	std::array<uint8_t, 8>{0x13, 0x01, 0x83, 0x11, 0xfa, 0xe4, 0x10, 0x04}, // Harpsichord
	std::array<uint8_t, 8>{0x17, 0xc1, 0x24, 0x07, 0xf8, 0xf8, 0x22, 0x12}, // Vibraphone
	std::array<uint8_t, 8>{0x61, 0x50, 0x0c, 0x05, 0xc2, 0xf5, 0x20, 0x42}, // Synthesizer Bass
	std::array<uint8_t, 8>{0x01, 0x01, 0x55, 0x03, 0xc9, 0x95, 0x03, 0x02}, // Acoustic Bass
	std::array<uint8_t, 8>{0x61, 0x41, 0x89, 0x03, 0xf1, 0xe4, 0x40, 0x13}, // Electric Guitar
};

static constexpr int plotResolution = 90;
static constexpr auto sine = []{
	std::array<float, plotResolution> result = {};
	for (auto i : xrange(plotResolution)) {
		auto t = double(i) / double(plotResolution);
		result[i] = float(cstd::sin<2>(2.0 * Math::pi * 6 * t));
	}
	return result;
}();
static constexpr auto half_sine = []{
	std::array<float, plotResolution> result = {};
	for (auto i : xrange(plotResolution)) {
		auto t = double(i) / double(plotResolution);
		result[i] = float(std::clamp(cstd::sin<2>(2.0 * Math::pi * 6 * t), 0.0, 1.0));
	}
	return result;
}();
static constexpr auto plotAmOff = []{
	std::array<float, plotResolution> result = {};
	for (auto i : xrange(plotResolution)) {
		auto t = double(i) / double(plotResolution);
		result[i] = float(cstd::sin<2>(2.0 * Math::pi * 6 * t));
	}
	return result;
}();
static constexpr auto plotAmOn = []{
	std::array<float, plotResolution> result = {};
	for (auto i : xrange(plotResolution)) {
		auto t = double(i) / double(plotResolution);
		auto mod = cstd::cos<2>(2.0 * Math::pi * t);
		auto car = cstd::sin<2>(2.0 * Math::pi * 6 * t);
		result[i] = float(car * (1.0 + 0.5 * mod));
	}
	return result;
}();
static constexpr auto plotFmOff = []{
	std::array<float, plotResolution> result = {};
	for (auto i : xrange(plotResolution)) {
		auto t = double(i) / double(plotResolution);
		auto phase = 2.0 * Math::pi * 6.0 * t;
		result[i] = float(cstd::sin<2>(phase));
	}
	return result;
}();
static constexpr auto plotFmOn = []{
	std::array<float, plotResolution> result = {};
	for (auto i : xrange(plotResolution)) {
		auto t = double(i) / double(plotResolution);
		auto mod = cstd::sin<2>(2.0 * Math::pi * t);
		auto phase = 2.0 * Math::pi * (6.0 * t + 0.5 * mod);
		result[i] = float(cstd::sin<2>(phase));
	}
	return result;
}();
static constexpr auto sineSamples = []{
	std::array<float, plotResolution> result = {};
	for (auto i : xrange(plotResolution)) {
		auto t = double(i) / double(plotResolution);
		result[i] = float(cstd::sin<2>(2.0 * Math::pi * t));
	}
	return result;
}();

void ImGuiMsxMusicViewer::paintInstrument(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t, 64> regs)
{
	auto customInstr = cs.selectedInstr == 0;
	auto instrRegs = customInstr ? subspan<8>(regs) : fixedInstruments[cs.selectedInstr - 1];

	auto headerColor = ImGui::GetColorU32(ImGuiCol_TableHeaderBg);
	auto drawRow = [&](std::string_view desc, Hover modH, Hover carH, auto draw,
	                   float highlightWidth = 0.0f, std::span<gl::vec2> getPos = {}) {
		if (modH.reg == carH.reg) {
			assert(modH.mask != carH.mask);
		} else if (carH.reg != 255) {
			assert((modH.reg & 1) == 0);
			assert((modH.reg + 1) == carH.reg);
			assert(modH.mask == carH.mask);
		}

		if (ImGui::TableNextColumn()) {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, headerColor);
			ImGui::TextUnformatted(desc);
		}
		drawCell(cs, newHoverList, instrRegs, modH, [&](uint32_t v) {
			draw(v);
		}, highlightWidth, getPos.empty() ? nullptr : &getPos[0]);
		drawCell(cs, newHoverList, instrRegs, carH, [&](uint32_t v) {
			if (carH.reg == 255) return;
			draw(v);
		}, highlightWidth, getPos.empty() ? nullptr : &getPos[1]);
	};

	int flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
	im::Table("tab", 3, flags, [&] {
		if (ImGui::TableNextColumn()) {
			auto instrNameWidth = ImGui::CalcTextSize(instrNames[13]).x;
			ImGui::SetNextItemWidth(instrNameWidth + ImGui::GetFrameHeight());
			im::Combo("##select-instr", instrNames[cs.selectedInstr].c_str(), [&]{
				for (auto i : xrange(16)) {
					if (ImGui::Selectable(instrNames[i].c_str())) {
						cs.selectedInstr = i;
					}
				}
			});
		}
		if (ImGui::TableNextColumn()) {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, headerColor);
			ImGui::TextUnformatted("Modulator");
		}
		if (ImGui::TableNextColumn()) {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, headerColor);
			ImGui::TextUnformatted("Carrier");
		}

		auto width = ImGui::CalcTextSize("0.5x").x + ImGui::GetStyle().ItemSpacing.x;
		auto textAndPlot = [&](const auto& text, std::span<const float> data) {
			auto pos = ImGui::GetCursorPos();
			ImGui::TextUnformatted(text);
			ImGui::SetCursorPos(pos + gl::vec2{width, 0.0f});

			auto topLeft = ImGui::GetCursorScreenPos();
			gl::vec2 plotSize = {plotResolution, ImGui::GetFontSize()};
			auto bottomRight = topLeft + plotSize;
			ImGui::Dummy(plotSize);
			plotLinesRaw(data, -1.0f, 1.0f, topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_Text));
		};

		int offset = customInstr ? 0 : 64; // don't highlight registers for fixed instruments
		drawRow("Waveform"sv, {offset + 3, 0x08}, {offset + 3, 0x10}, [&](uint32_t v) {
			textAndPlot(v ? "half"sv : "full"sv, v ? half_sine : sine);
		});
		drawRow("Tremolo"sv, {offset + 0, 0x80}, {offset + 1, 0x80}, [&](uint32_t v) {
			textAndPlot(v ? "on"sv : "off"sv, v ? plotAmOn : plotAmOff);
		});
		drawRow("Vibrato"sv, {offset + 0, 0x40}, {offset + 1, 0x40}, [&](uint32_t v) {
			textAndPlot(v ? "on"sv : "off"sv, v ? plotFmOn : plotFmOff);
		});
		drawRow("Multiplier"sv, {offset + 0, 0x0f}, {offset + 1, 0x0f}, [&](uint32_t v) {
			static constexpr std::array<uint8_t, 16> multipliers = {
				0, 1,  2,  3,  4,  5,  6,  7,
				8, 9, 10, 10, 12, 12, 15, 15,
			};
			int m = multipliers[v];
			auto text = v ? tmpStrCat(m, 'x') : TemporaryString("0.5x");
			std::array<float, plotResolution> data;
			auto f = m ? 2 * m : 1;
			for (auto i : xrange(plotResolution)) {
				data[i] = sineSamples[(f * i) % plotResolution];
			}
			textAndPlot(text, data);
		});
		drawRow("Key scale rate"sv, {offset + 0, 0x10}, {offset + 1, 0x10}, [](uint32_t v) {
			ImGui::TextUnformatted(v ? "stronger"sv : "weaker"sv);
		});
		drawRow("Key scale level"sv, {offset + 2, 0xc0}, {offset + 3, 0xc0}, [](uint32_t v) {
			static constexpr std::array<std::string_view, 4> tab = {
				"0", "3", "1.5", "6"
			};
			ImGui::StrCat('-', tab[v], "dB/octave");
		});
		drawRow("Attenuation"sv, {offset + 2, 0x3f}, {}, [](uint32_t v) {
			auto t = 75 * v;
			auto t1 = t / 100;
			auto t2 = t % 100;
			char t21 = narrow<char>(t2 / 10 + '0');
			char t22 = narrow<char>(t2 % 10 + '0');
			ImGui::StrCat(t1, '.', t21, t22, "dB");
		});
		auto printNum = [](uint32_t v) { ImGui::StrCat(v); };
		drawRow("Feedback"sv, {offset + 3, 0x07}, {}, printNum);
		drawRow("Envelope type"sv, {offset + 0, 0x20}, {offset + 1, 0x20}, [](uint32_t v) {
			ImGui::TextUnformatted(v ? "sustained"sv : "percussive"sv);
		});
		std::array<gl::vec2, 2> posMC;
		drawRow("Attack rate"sv,   {offset + 4, 0xf0}, {offset + 5, 0xf0}, printNum, width, posMC);
		drawRow("Decay rate"sv,    {offset + 4, 0x0f}, {offset + 5, 0x0f}, printNum, width);
		drawRow("Sustain level"sv, {offset + 6, 0xf0}, {offset + 7, 0xf0}, printNum, width);
		drawRow("Release rate"sv,  {offset + 6, 0x0f}, {offset + 7, 0x0f}, printNum, width);

		auto drawADSR = [&](gl::vec2 pos, bool egType, int o) {
			auto tl = pos + gl::vec2{width, 0.0f};
			auto sz = gl::vec2{plotResolution, 4 * ImGui::GetTextLineHeight() + 6 * ImGui::GetStyle().CellPadding.y};
			auto p = [&](float x, float y) { return tl + sz * gl::vec2{x, y}; };
			auto pA = p(0.0f, 1.0f);
			auto pB = p(0.0f, 0.3f);
			auto pC = p(0.3f, 0.0f);
			auto pD = p(0.4f, 0.3f);
			auto pE = egType ? p(0.8f, 0.3f) : p(0.8f, 0.5f);
			auto pF = p(1.0f, 1.0f);

			auto mouse = ImGui::GetIO().MousePos;
			Hovers hoverAttack  = {o + 4, 0xf0};
			Hovers hoverDecay   = {o + 4, 0x0f};
			Hovers hoverSustain = {o + 6, 0xf0};
			Hovers hoverRelease = {o + 6, 0x0f};
			if (tl.y <= mouse.y && mouse.y < (tl.y + sz.y) &&
			    tl.x <= mouse.x && mouse.x < (tl.x + sz.x)) {
				auto h = [&]{
					if (mouse.x < pC.x) return hoverAttack;
					if (mouse.x < pD.x) return hoverDecay;
					if (mouse.x < pE.x) return hoverSustain;
					return hoverRelease;
				}();
				newHoverList.push_back(h);
			}

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			auto white = getColor(imColor::TEXT);
			auto yellow = getColor(imColor::YELLOW);
			bool hlAttack  = match(cs.hoverList, hoverAttack);
			drawList->AddBezierQuadratic(pA, pB, pC, hlAttack ? yellow : white, hlAttack ? 2.0f : 1.0f);
			auto line = [&](gl::vec2 p1, gl::vec2 p2, Hovers hover) {
				bool h = match(cs.hoverList, hover);
				drawList->AddLine(p1, p2, h ? yellow : white, h ? 2.0f : 1.0f);
			};
			line(pC, pD, hoverDecay);
			line(pD, pE, hoverSustain);
			line(pE, pF, hoverRelease);
		};
		drawADSR(posMC[0], (instrRegs[0] & 0x20) != 0, offset + 0);
		drawADSR(posMC[1], (instrRegs[1] & 0x20) != 0, offset + 1);
	});
}

void ImGuiMsxMusicViewer::paintRegisters(ChipState& cs, HoverList& newHoverList, std::span<const uint8_t, 64> regs)
{
	int flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
	im::Table("##reg-tab", 17, flags, [&] {
		ImGui::TableSetupColumn("");
		std::array<char, 2> name = {};
		for (int i : xrange(16)) {
			name[0] = char(i + (i < 10 ? '0' : ('A' - 10)));
			ImGui::TableSetupColumn(name.data());
		}
		ImGui::TableHeadersRow();

		auto headerColor = ImGui::GetColorU32(ImGuiCol_TableHeaderBg);
		for (auto row : xrange(4)) {
			if (ImGui::TableNextColumn()) {
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, headerColor);
				ImGui::StrCat(row, "0-", row, 'F');
			}
			for (auto column : xrange(16)) {
				auto reg = uint8_t(16 * row + column);
				drawCell(cs, newHoverList, regs, {reg, 0xff}, [&](uint32_t v){
					bool disabled = (row == 0) ? (8 <= column && column <= 13) : (9 <= column);
					im::Disabled(disabled, [&]{
						ImGui::StrCat(hex_string<2>(v));
					});
				});
			}
		}
	});
}

void ImGuiMsxMusicViewer::paintMsxMusic(const YM2413& ym2413)
{
	auto& cs = chips[ym2413.getName()];
	for (auto& kh : cs.keyHistory) {
		std::shift_left(kh.begin(), kh.end(), 1); // TODO c++23 range version
		kh.back() = {}; // may or may not get overwritten, so initialize now
	}
	HoverList newHoverList;
	auto regs = ym2413.peekRegs();

	im::TreeNode("Channels", ImGuiTreeNodeFlags_DefaultOpen, [&]{
		paintChannels(cs, newHoverList, regs);
	});
	im::TreeNode("Instrument settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
		paintInstrument(cs, newHoverList, regs);
	});
	im::TreeNode("Registers", ImGuiTreeNodeFlags_DefaultOpen, [&]{
		paintRegisters(cs, newHoverList, regs);
	});

	cs.hoverList = std::move(newHoverList);
}

void ImGuiMsxMusicViewer::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	ImGui::SetNextWindowSize(gl::vec2{33, 50} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Msx-Music Viewer", &show, [&]{
		std::vector<const YM2413*> devices;
		for (const auto& info: motherBoard->getMSXMixer().getDeviceInfos()) {
			if (const auto* device = dynamic_cast<const YM2413*>(info.device)) {
				devices.push_back(device);
			}
		}
		if (auto num = devices.size(); num == 0) {
			ImGui::TextUnformatted("No MSX-Music devices currently present in the system.");
		} else if (num == 1) {
			paintMsxMusic(*devices.front());
		} else {
			for (const auto* device : devices) {
				im::TreeNode(device->getName().c_str(), [&] {
					paintMsxMusic(*device);
				});
			}
		}
	});
}

} // namespace openmsx
