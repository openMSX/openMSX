#include "Shortcuts.hh"

#include "narrow.hh"
#include "one_of.hh"

#include "imgui_internal.h"

#include <algorithm>
#include <array>
#include <utility>

namespace openmsx {

// When adding a new shortcut:
// * Add a new value in 'enum Shortcuts::ID'
// * Add a single line in this table
struct AllShortcutInfo {
	Shortcuts::ID id;
	ImGuiKeyChord keyChord;
	Shortcuts::Type type;
	bool repeat;
	zstring_view name;        // used in settings.xml
	zstring_view description; // shown in GUI
};
using enum Shortcuts::ID;
using enum Shortcuts::Type;
static constexpr auto allShortcutInfo = std::to_array<AllShortcutInfo>({
	{HEX_GOTO_ADDR,           ImGuiKey_G | ImGuiMod_Ctrl,   ALWAYS_LOCAL, false, "hex_editor_goto_address", "Go to address in hex viewer"},

	{DEBUGGER_STEP_IN,        ImGuiKey_F7,                  LOCAL,        true,  "step-in",                 "Debugger: step-in"},
	{DEBUGGER_STEP_OVER,      ImGuiKey_F8,                  LOCAL,        true,  "step-over",               "Debugger: step-over"},
	{DEBUGGER_STEP_OUT,       ImGuiKey_F7 | ImGuiMod_Shift, LOCAL,        true,  "step-out",                "Debugger: step-out"},
	{DEBUGGER_STEP_BACK,      ImGuiKey_F8 | ImGuiMod_Shift, LOCAL,        true,  "step-back",               "Debugger: step-back"},
	{DEBUGGER_BREAK_CONTINUE, ImGuiKey_F5,                  LOCAL,        false, "break-continue",          "Debugger: toggle break / continue"},

	{DISASM_GOTO_ADDR,        ImGuiMod_Ctrl | ImGuiKey_G,   ALWAYS_LOCAL, false, "disasm_goto_address",     "Scroll to address in disassembler"},
	{DISASM_RUN_TO_ADDR,      ImGuiMod_Ctrl | ImGuiKey_R,   ALWAYS_LOCAL, false, "disasm_run_to_address",   "Debugger: run to a specific address"},
	{DISASM_TOGGLE_BREAKPOINT,ImGuiMod_Ctrl | ImGuiKey_B,   ALWAYS_LOCAL, false, "disasm_toggle_breakpoint","Debugger: toggle breakpoint at current address"},

	{TRACE_ZOOM_TO_FIT,       ImGuiMod_Ctrl | ImGuiKey_0,    LOCAL,       false, "trace_zoom_to_fit",       "Trace Viewer: zoom to fit all events"},
	{TRACE_ZOOM_IN,           ImGuiMod_Ctrl | ImGuiKey_Equal,LOCAL,       false, "trace_zoom_in",           "Trace Viewer: zoom in"},
	{TRACE_ZOOM_OUT,          ImGuiMod_Ctrl | ImGuiKey_Minus,LOCAL,       false, "trace_zoom_out",          "Trace Viewer: zoom out"},
	{TRACE_PREV_NEG_EDGE,     ImGuiMod_Ctrl | ImGuiKey_A,    LOCAL,       false, "trace_prev_neg_edge",     "Trace Viewer: goto previous negative edge"},
	{TRACE_PREV_POS_EDGE,     ImGuiMod_Ctrl | ImGuiKey_S,    LOCAL,       false, "trace_prev_pos_edge",     "Trace Viewer: goto previous positive edge"},
	{TRACE_PREV_EDGE,         ImGuiMod_Ctrl | ImGuiKey_D,    LOCAL,       false, "trace_prev_edge",         "Trace Viewer: goto previous edge"},
	{TRACE_NEXT_EDGE,         ImGuiMod_Ctrl | ImGuiKey_F,    LOCAL,       false, "trace_next_edge",         "Trace Viewer: goto next edge"},
	{TRACE_NEXT_POS_EDGE,     ImGuiMod_Ctrl | ImGuiKey_G,    LOCAL,       false, "trace_next_pos_edge",     "Trace Viewer: goto next positive edge"},
	{TRACE_NEXT_NEG_EDGE,     ImGuiMod_Ctrl | ImGuiKey_H,    LOCAL,       false, "trace_next_neg_edge",     "Trace Viewer: goto next negative edge"},
	{TRACE_ALT_PREV_NEG_EDGE, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_A, LOCAL, false, "trace_alt_prev_neg_edge", "Trace Viewer: secondary goto previous negative edge"},
	{TRACE_ALT_PREV_POS_EDGE, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, LOCAL, false, "trace_alt_prev_pos_edge", "Trace Viewer: secondary goto previous positive edge"},
	{TRACE_ALT_PREV_EDGE,     ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_D, LOCAL, false, "trace_alt_prev_edge",     "Trace Viewer: secondary goto previous edge"},
	{TRACE_ALT_NEXT_EDGE,     ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_F, LOCAL, false, "trace_alt_next_edge",     "Trace Viewer: secondary goto next edge"},
	{TRACE_ALT_NEXT_POS_EDGE, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_G, LOCAL, false, "trace_alt_next_pos_edge", "Trace Viewer: secondary goto next positive edge"},
	{TRACE_ALT_NEXT_NEG_EDGE, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_H, LOCAL, false, "trace_alt_next_neg_edge", "Trace Viewer: secondary goto next negative edge"},
	{TRACE_SHOW_MENU_BAR,     ImGuiMod_Ctrl | ImGuiKey_M,    LOCAL,       false, "trace_show_menu_bar",     "Trace Viewer: show/hide menu bar"},
});
static_assert(narrow<int>(allShortcutInfo.size()) == std::to_underlying(Shortcuts::ID::NUM));

static constexpr auto defaultShortcuts = []{
	array_with_enum_index<Shortcuts::ID, Shortcuts::Shortcut> result = {};
	for (auto i : xrange(std::to_underlying(Shortcuts::ID::NUM))) {
		const auto& all = allShortcutInfo[i];
		auto id = static_cast<Shortcuts::ID>(i);
		assert(all.id == id); // verify that rows are in-order
		result[id].keyChord = all.keyChord;
		result[id].type = all.type;
	}
	return result;
}();

static constexpr auto shortcutRepeats = []{
	array_with_enum_index<Shortcuts::ID, bool> result = {};
	for (auto i : xrange(std::to_underlying(Shortcuts::ID::NUM))) {
		auto id = static_cast<Shortcuts::ID>(i);
		result[id] = allShortcutInfo[i].repeat;
	}
	return result;
}();

static constexpr auto shortcutNames = []{
	array_with_enum_index<Shortcuts::ID, zstring_view> result = {};
	for (auto i : xrange(std::to_underlying(Shortcuts::ID::NUM))) {
		auto id = static_cast<Shortcuts::ID>(i);
		result[id] = allShortcutInfo[i].name;
	}
	return result;
}();

static constexpr auto shortcutDescriptions = []{
	array_with_enum_index<Shortcuts::ID, zstring_view> result = {};
	for (auto i : xrange(std::to_underlying(Shortcuts::ID::NUM))) {
		auto id = static_cast<Shortcuts::ID>(i);
		result[id] = allShortcutInfo[i].description;
	}
	return result;
}();

Shortcuts::Shortcuts()
{
	setDefaultShortcuts();
}

void Shortcuts::setDefaultShortcuts()
{
	shortcuts = defaultShortcuts; // this can overwrite the 'type' field
}

const Shortcuts::Shortcut& Shortcuts::getDefaultShortcut(Shortcuts::ID id)
{
	return defaultShortcuts[id];
}

const Shortcuts::Shortcut& Shortcuts::getShortcut(Shortcuts::ID id) const
{
	return shortcuts[id];
}

void Shortcuts::setShortcut(ID id, const Shortcut& shortcut)
{
	auto oldType = shortcuts[id].type;
	shortcuts[id] = shortcut;
	if (oldType == one_of(Type::ALWAYS_LOCAL, Type::ALWAYS_GLOBAL)) {
		// cannot change this
		shortcuts[id].type = oldType;
	}
}

bool Shortcuts::getShortcutRepeat(ID id)
{
	return shortcutRepeats[id];
}

zstring_view Shortcuts::getShortcutName(Shortcuts::ID id)
{
	return shortcutNames[id];
}

std::optional<Shortcuts::ID> Shortcuts::parseShortcutName(std::string_view name)
{
	auto it = std::ranges::find(shortcutNames, name);
	if (it == shortcutNames.end()) return {};
	return static_cast<Shortcuts::ID>(std::distance(shortcutNames.begin(), it));
}

std::optional<Shortcuts::Type> Shortcuts::parseType(std::string_view name)
{
	if (name == "global") return GLOBAL;
	if (name == "local") return LOCAL;
	return {};
}

zstring_view Shortcuts::getShortcutDescription(ID id)
{
	return shortcutDescriptions[id];
}

bool Shortcuts::checkShortcut(const ShortcutWithRepeat& shortcut) const
{
	assert(shortcut.keyChord != ImGuiKey_None);
	auto flags = (shortcut.type == one_of(GLOBAL, ALWAYS_GLOBAL) ? (ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteUnlessBgFocused) : 0)
	           | (shortcut.repeat ? ImGuiInputFlags_Repeat : 0);
	return ImGui::Shortcut(shortcut.keyChord, flags, 0);
}

bool Shortcuts::checkShortcut(ID id) const
{
	const auto& shortcut = shortcuts[id];
	if (shortcut.keyChord == ImGuiKey_None) return false;
	return checkShortcut({shortcut.keyChord, shortcut.type, getShortcutRepeat(id)});
}

} // namespace openmsx
