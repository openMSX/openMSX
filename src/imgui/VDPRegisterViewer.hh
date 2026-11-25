#ifndef VDP_REGISTER_VIEWER_HH
#define VDP_REGISTER_VIEWER_HH

// The core of the VDP register viewers (ImGuiVdpRegs, ImGuiSMSVDPRegs).
//
// Each viewer provides tables that describe its registers, and a 'Regs' type
// for the registers of its VDP, with:
// - a member 'values': the current register values, indexed by register
//   number (the control registers, followed by the status registers),
// - a member function 'write(uint8_t reg, uint8_t value)' to change a control
//   register,
// - whatever else its explanations need (e.g. the display mode).

#include "ImGuiCpp.hh"
#include "ImGuiUtils.hh"

#include "StringOp.hh"
#include "TemporaryString.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "strCat.hh"
#include "xrange.hh"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

struct RegisterDescription {
	std::array<const char*, 8> bits; // most significant bit first
	std::string_view name;
};

struct RegSubFunction {
	uint8_t reg = uint8_t(-1);
	uint8_t mask = 0;
};

// An item of the register explanation: (up to) two bit fields, combined into
// a single value, and how to display that value. Most items only need that
// value, the others also get the 'Regs' (e.g. to show an address that depends
// on the display mode).
// A line of the form "label\tvalue\tremark" (the remark is optional) is drawn
// like a table row: the values of all such lines are aligned in one column and
// drawn in the mono-space font.
template<typename Regs>
struct RegFunction {
	using Display     = TemporaryString(*)(uint32_t value);
	using DisplayRegs = TemporaryString(*)(uint32_t value, const Regs& regs);

	constexpr RegFunction(std::array<RegSubFunction, 2> subs_, std::string_view toolTip_, Display display_)
		: subs(subs_), toolTip(toolTip_), display(display_) {}
	constexpr RegFunction(std::array<RegSubFunction, 2> subs_, std::string_view toolTip_, DisplayRegs displayRegs_)
		: subs(subs_), toolTip(toolTip_), displayRegs(displayRegs_) {}

	[[nodiscard]] TemporaryString explain(uint32_t value, const Regs& regs) const {
		// msvc workaround, this doesn't compile:
		//     return display ? display(value) : displayRegs(value, regs);
		if (display) {
			return display(value);
		} else {
			return displayRegs(value, regs);
		}
	}

	// VDPRegisterViewer::drawSection() draws this item when 'subs[0].reg' or
	// 'subs[1].reg' is part of 'showRegisters'; when 'subs[0].mask == 0' the
	// item cannot be highlighted (typically such items show a fixed string).
	std::array<RegSubFunction, 2> subs;
	std::string_view toolTip;
	Display display = nullptr;
	DisplayRegs displayRegs = nullptr;
};

template<typename Regs>
struct VDPRegisterTables {
	// Indexed by register number: the control registers, followed by the
	// status registers (starting at 'firstStatusReg').
	std::span<const RegisterDescription> descriptions;
	std::span<const RegFunction<Regs>> functions;
	uint8_t firstStatusReg;
};

template<typename Regs>
[[nodiscard]] int lookupRegFunction(std::span<const RegFunction<Regs>> functions, uint8_t reg, uint8_t mask)
{
	int i = 0;
	for (const auto& f : functions) {
		for (const auto& s : f.subs) {
			if ((reg == s.reg) && (mask & s.mask)) {
				return i;
			}
		}
		++i;
	}
	return -1;
}

// Draws the register sections of one frame, see drawVDPRegisters().
template<typename Regs>
class VDPRegisterViewer {
public:
	// Draw a table with the registers 'showRegisters' (the control registers
	// are editable), followed by their explanation (when enabled).
	void drawSection(std::span<const uint8_t> showRegisters);

private:
	VDPRegisterViewer(const VDPRegisterTables<Regs>& tables_, const Regs& regs_,
	                  int& hoveredFunction_, bool& explanation_, ImFont* monoFont_)
		: tables(tables_), regs(regs_)
		, hoveredFunction(hoveredFunction_), explanation(explanation_)
		, monoFont(monoFont_) {}

	// After the last section (but still inside the window): remembers the
	// hovered item and handles the context menu.
	void end();

	template<typename R, typename Sections>
	friend void drawVDPRegisters(const VDPRegisterTables<R>& tables, const R& regs,
	                             int& hoveredFunction, bool& explanation, ImFont* monoFont,
	                             Sections&& sections);

	const VDPRegisterTables<Regs>& tables;
	const Regs& regs;
	int& hoveredFunction;
	bool& explanation;
	ImFont* monoFont;
	int newHoveredFunction = -1;
};

template<typename Regs>
void VDPRegisterViewer<Regs>::drawSection(std::span<const uint8_t> showRegisters)
{
	using namespace std::literals;
	std::span<const uint8_t> regValues = regs.values;

	ImGui::SameLine();
	HelpMarker("Click to toggle bits, or edit the hex-field.\n"
	           "Right-click to show/hide register explanation.");

	im::Table("table", 10, 0, [&]{
		// isCellHovered:
		//   This uses an internal API, see here for a discussion
		//       https://github.com/ocornut/imgui/issues/6347
		//   In other words: this solution is suggested by the Dear ImGui
		//   developers until something better gets implemented.
		auto mouse_pos = ImGui::GetMousePos();
		const auto* table = ImGui::GetCurrentTable();
		auto isCellHovered = [&] {
			return ImGui::TableGetCellBgRect(table, table->CurrentColumn).Contains(mouse_pos);
		};

		im::ID_for_range(showRegisters.size(), [&](int i) {
			auto reg = showRegisters[i];
			bool isStatus = reg >= tables.firstStatusReg;
			const auto& rd = tables.descriptions[reg];
			if (ImGui::TableNextColumn()) {
				auto name = tmpStrCat(isStatus ? "S#"sv : "R#"sv, isStatus ? reg - tables.firstStatusReg : reg);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(static_cast<std::string_view>(name));
				simpleToolTip(rd.name);
			}
			uint8_t value = regValues[reg];
			bool changed = false;
			if (ImGui::TableNextColumn()) {
				if (!isStatus) {
					if (ImGui::InputScalar("##value", ImGuiDataType_U8, &value, nullptr, nullptr, "%02X", ImGuiInputTextFlags_CharsHexadecimal)) {
						changed = true;
					}
				} else {
					ImGui::Text("%02X", value);
				}
			}
			const auto& bits = rd.bits;
			im::ID_for_range(8, [&](int bit_) {
				if (ImGui::TableNextColumn()) {
					int bit = 7 - bit_;
					auto mask = narrow<uint8_t>(1 << bit);
					int f = lookupRegFunction(tables.functions, reg, mask);
					if (f != -1 && f == hoveredFunction) {
						ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
							getColor(imColor::YELLOW));
					}
					im::StyleColor(ImGuiCol_Button, getColor(value & mask ? imColor::KEY_ACTIVE : imColor::KEY_NOT_ACTIVE), [&]{
						if (ImGui::Button(bits[bit_], {40.0f, 0.0f})) {
							value ^= mask;
							changed = true;
						}
					});
					if ((f != -1) && isCellHovered()) {
						newHoveredFunction = f;
						if (auto tip = tables.functions[f].toolTip; !tip.empty()) {
							simpleToolTip(tip);
						}
					}
				}
			});
			if (changed && !isStatus) {
				regs.write(reg, value);
			}
		});
	});
	if (explanation) {
		// Collect the explanations first: lines of the form "label\tvalue"
		// are drawn with all values aligned in one column (after the widest
		// label), like a table.
		struct Explained {
			int f;
			int shift;
			std::string text;
		};
		std::vector<Explained> explained;
		for (auto f : xrange(narrow<int>(tables.functions.size()))) {
			const auto& func = tables.functions[f];
			if (!(contains(showRegisters, func.subs[0].reg) ||
			      contains(showRegisters, func.subs[1].reg))) {
				continue;
			}
			uint32_t value = 0;
			int shift = 0;
			for (const auto& sub : func.subs) {
				if (sub.reg == uint8_t(-1)) break;
				auto tmp = ((regValues[sub.reg] & sub.mask) >> std::countr_zero(sub.mask));
				value |= tmp << shift;
				shift += std::popcount(sub.mask);
			}
			if (auto s = func.explain(value, regs); !s.empty()) {
				explained.emplace_back(f, shift, std::string(s));
			}
		}

		auto lines = [](std::string_view s) {
			return StringOp::split_view<StringOp::EmptyParts::REMOVE>(s, '\n');
		};
		// A line "label\tvalue\tremark" (the remark is optional) is drawn as
		// a table row: all values aligned in one column after the widest
		// label, the value (presented data) in the mono-space font, the
		// remark after it in the proportional font.
		struct Fields {
			std::string_view label, value, remark;
		};
		auto split = [](std::string_view line) -> std::optional<Fields> {
			auto [label, rest] = StringOp::splitOnFirst(line, '\t');
			if (label.size() == line.size()) return {}; // no tab
			auto [value, remark] = StringOp::splitOnFirst(rest, '\t');
			return Fields{label, value, remark};
		};
		float valueColumn = 0.0f;
		for (const auto& e : explained) {
			for (auto line : lines(e.text)) {
				if (auto fields = split(line)) {
					valueColumn = std::max(valueColumn, ImGui::CalcTextSize(fields->label).x);
				}
			}
		}
		valueColumn += ImGui::GetStyle().ItemSpacing.x;

		auto drawText = [&](std::string_view s) {
			if (s.find('\t') == std::string_view::npos) {
				ImGui::TextUnformatted(s);
				return;
			}
			im::Group([&]{
				for (auto line : lines(s)) {
					auto fields = split(line);
					if (!fields) {
						ImGui::TextUnformatted(line);
						continue;
					}
					ImGui::TextUnformatted(fields->label);
					ImGui::SameLine(valueColumn); // relative to the start of the group
					if (!fields->value.empty()) {
						im::Font(monoFont, [&]{
							ImGui::TextUnformatted(fields->value);
						});
						if (!fields->remark.empty()) ImGui::SameLine();
					}
					if (!fields->remark.empty()) {
						ImGui::TextUnformatted(fields->remark);
					}
				}
			});
		};

		auto* drawList = ImGui::GetWindowDrawList();
		gl::vec2 framePadding = ImGui::GetStyle().FramePadding;
		for (const auto& [f, shift, s] : explained) {
			const auto& func = tables.functions[f];
			bool canHover = func.subs[0].mask != 0;
			bool highlight = canHover && (f == hoveredFunction);
			// The highlight is drawn after the text (so that it has the size
			// of the drawn text), but in a channel behind it.
			ImDrawListSplitter splitter;
			if (highlight) {
				splitter.Split(drawList, 2);
				splitter.SetCurrentChannel(drawList, 1);
			}
			im::StyleColor(ImGuiCol_Text, getColor(highlight ? imColor::BLACK : imColor::TEXT), [&]{
				drawText(s);
			});
			if (highlight) {
				splitter.SetCurrentChannel(drawList, 0);
				gl::vec2 min = ImGui::GetItemRectMin();
				gl::vec2 max = ImGui::GetItemRectMax();
				drawList->AddRectFilled(min - framePadding, max + 2.0f * framePadding, getColor(imColor::YELLOW));
				splitter.Merge(drawList);
			}
			if (canHover) {
				if (ImGui::IsItemHovered()) {
					newHoveredFunction = f;
				}
				if (shift == 1) { // single bit
					if (auto reg = func.subs[0].reg; reg < tables.firstStatusReg) { // non-status-register
						if (ImGui::IsItemClicked()) {
							auto mask = func.subs[0].mask;
							regs.write(reg, regValues[reg] ^ mask);
						}
					}
				}
			}
			if (!s.ends_with('\n')) ImGui::SameLine();
		}
	}
}

template<typename Regs>
void VDPRegisterViewer<Regs>::end()
{
	hoveredFunction = newHoveredFunction;

	if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
		ImGui::OpenPopup("context");
	}
	im::Popup("context", [&]{
		if (ImGui::Checkbox("Show explanation ", &explanation)) {
			ImGui::CloseCurrentPopup();
		}
	});
}

// Draws the register sections of one frame: 'sections(viewer)' draws each
// section with viewer.drawSection(), e.g.
//     drawVDPRegisters(tables, regs, hoveredFunction, explanation, manager.fontMono, [&](auto& viewer) {
//         im::TreeNode("Mode registers", ..., [&]{ viewer.drawSection(modeRegs); });
//         ...
//     });
// 'hoveredFunction' and 'explanation' must outlive the frame: the hovered item
// is highlighted in the next frame, 'explanation' is a (persistent) setting.
// 'monoFont' is used for the values in the explanation.
template<typename Regs, typename Sections>
void drawVDPRegisters(const VDPRegisterTables<Regs>& tables, const Regs& regs,
                      int& hoveredFunction, bool& explanation, ImFont* monoFont,
                      Sections&& sections)
{
	VDPRegisterViewer<Regs> viewer(tables, regs, hoveredFunction, explanation, monoFont);
	sections(viewer);
	viewer.end();
}

} // namespace openmsx

#endif
