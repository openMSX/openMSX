#include "Shortcuts.hh"

#include "one_of.hh"

#include "imgui_internal.h"

#include <array>

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
	{HEX_GOTO_ADDR,           ImGuiKey_G | ImGuiMod_Ctrl,   ALWAYS_LOCAL,  false, "hex_editor_goto_address", "Go to address in hex viewer"},
	{DEBUGGER_STEP_IN,        ImGuiKey_F7,                  LOCAL,         true,  "step-in",                 "Debugger: step-in"},
	{DEBUGGER_STEP_OVER,      ImGuiKey_F8,                  LOCAL,         true,  "step-over",               "Debugger: step-over"},
	{DEBUGGER_STEP_OUT,       ImGuiKey_F7 | ImGuiMod_Shift, LOCAL,         true,  "step-out",                "Debugger: step-out"},
	{DEBUGGER_STEP_BACK,      ImGuiKey_F8 | ImGuiMod_Shift, LOCAL,         true,  "step-back",               "Debugger: step-back"},
	{DEBUGGER_BREAK_CONTINUE, ImGuiKey_F5,                  LOCAL,         false, "break-continue",          "Debugger: toggle break / continue"},
	{DISASM_GOTO_ADDR,        ImGuiMod_Ctrl | ImGuiKey_G,   ALWAYS_LOCAL,  false, "disasm_goto_address",     "Scroll to address in disassembler"},
	{DISASM_RUN_TO_ADDR,      ImGuiMod_Ctrl | ImGuiKey_R,   ALWAYS_LOCAL,  false, "disasm_run_to_address",   "Debugger: run to a specific address"},
});
static_assert(allShortcutInfo.size() == to_underlying(Shortcuts::ID::NUM));

static constexpr auto defaultShortcuts = []{
	array_with_enum_index<Shortcuts::ID, Shortcuts::Shortcut> result = {};
	for (int i = 0; i < to_underlying(Shortcuts::ID::NUM); ++i) {
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
	for (int i = 0; i < to_underlying(Shortcuts::ID::NUM); ++i) {
		auto id = static_cast<Shortcuts::ID>(i);
		result[id] = allShortcutInfo[i].repeat;
	}
	return result;
}();

static constexpr auto shortcutNames = []{
	array_with_enum_index<Shortcuts::ID, zstring_view> result = {};
	for (int i = 0; i < to_underlying(Shortcuts::ID::NUM); ++i) {
		auto id = static_cast<Shortcuts::ID>(i);
		result[id] = allShortcutInfo[i].name;
	}
	return result;
}();

static constexpr auto shortcutDescriptions = []{
	array_with_enum_index<Shortcuts::ID, zstring_view> result = {};
	for (int i = 0; i < to_underlying(Shortcuts::ID::NUM); ++i) {
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
	auto it = ranges::find(shortcutNames, name);
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
	auto flags = (shortcut.type == one_of(GLOBAL, ALWAYS_GLOBAL) ? ImGuiInputFlags_RouteGlobalLow : 0)
	           | ImGuiInputFlags_RouteUnlessBgFocused
	           | (shortcut.repeat ? ImGuiInputFlags_Repeat : 0);
	return ImGui::Shortcut(shortcut.keyChord, 0, flags);
}

bool Shortcuts::checkShortcut(ID id) const
{
	const auto& shortcut = shortcuts[id];
	if (shortcut.keyChord == ImGuiKey_None) return false;
	return checkShortcut({shortcut.keyChord, shortcut.type, getShortcutRepeat(id)});
}

} // namespace openmsx
