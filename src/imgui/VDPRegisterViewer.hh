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

#include "TemporaryString.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "strCat.hh"
#include "xrange.hh"

#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <string_view>

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
template<typename Regs>
struct RegFunction {
	using Display     = TemporaryString(*)(uint32_t value);
	using DisplayRegs = TemporaryString(*)(uint32_t value, const Regs& regs);

	constexpr RegFunction(std::array<RegSubFunction, 2> subs_, std::string_view toolTip_, Display display_)
		: subs(subs_), toolTip(toolTip_), display(display_) {}
	constexpr RegFunction(std::array<RegSubFunction, 2> subs_, std::string_view toolTip_, DisplayRegs displayRegs_)
		: subs(subs_), toolTip(toolTip_), displayRegs(displayRegs_) {}

	[[nodiscard]] TemporaryString explain(uint32_t value, const Regs& regs) const {
		return display ? display(value) : displayRegs(value, regs);
	}

	// drawVDPRegisterSection() draws this item when 'subs[0].reg' or 'subs[1].reg' is
	// part of 'showRegisters'; when 'subs[0].mask == 0' the item cannot be
	// highlighted (typically such items show a fixed string).
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

// Draw a table with the registers 'showRegisters' (the control registers are
// editable), followed by their explanation (when enabled).
// The function 'hoveredFunction' is highlighted; 'newHoveredFunction' is set
// when a function is hovered.
template<typename Regs>
void drawVDPRegisterSection(const VDPRegisterTables<Regs>& tables, std::span<const uint8_t> showRegisters,
                            const Regs& regs, bool explanation,
                            int hoveredFunction, int& newHoveredFunction)
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
		auto* drawList = ImGui::GetWindowDrawList();
		gl::vec2 framePadding = ImGui::GetStyle().FramePadding;
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
				auto textColor = getColor(imColor::TEXT);
				bool canHover = func.subs[0].mask != 0;
				if (canHover && (f == hoveredFunction)) {
					gl::vec2 textSize = ImGui::CalcTextSize(s);
					gl::vec2 pos = ImGui::GetCursorScreenPos();
					textColor = getColor(imColor::BLACK);
					drawList->AddRectFilled(pos - framePadding, pos + textSize + 2.0f * framePadding, getColor(imColor::YELLOW));
				}
				im::StyleColor(ImGuiCol_Text, textColor, [&]{
					ImGui::TextUnformatted(s);
				});
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
}

} // namespace openmsx

#endif
