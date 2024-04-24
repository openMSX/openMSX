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
});
static_assert(allShortcutInfo.size() == Shortcuts::ID::NUM_SHORTCUTS);

static constexpr auto defaultShortcuts = []{
	std::array<Shortcuts::Shortcut, Shortcuts::ID::NUM_SHORTCUTS> result = {};
	for (int i = 0; i < Shortcuts::ID::NUM_SHORTCUTS; ++i) {
		const auto& all = allShortcutInfo[i];
		assert(all.id == i); // verify that rows are in-order
		result[i].keyChord = all.keyChord;
		result[i].type = all.type;
	}
	return result;
}();

static constexpr auto shortcutRepeats = []{
	std::array<bool, Shortcuts::ID::NUM_SHORTCUTS> result = {};
	for (int i = 0; i < Shortcuts::ID::NUM_SHORTCUTS; ++i) {
		result[i] = allShortcutInfo[i].repeat;
	}
	return result;
}();

static constexpr auto shortcutNames = []{
	std::array<zstring_view, Shortcuts::ID::NUM_SHORTCUTS> result = {};
	for (int i = 0; i < Shortcuts::ID::NUM_SHORTCUTS; ++i) {
		result[i] = allShortcutInfo[i].name;
	}
	return result;
}();

static constexpr auto shortcutDescriptions = []{
	std::array<zstring_view, Shortcuts::ID::NUM_SHORTCUTS> result = {};
	for (int i = 0; i < Shortcuts::ID::NUM_SHORTCUTS; ++i) {
		result[i] = allShortcutInfo[i].description;
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
	assert(id < ID::NUM_SHORTCUTS);
	return defaultShortcuts[id];
}

const Shortcuts::Shortcut& Shortcuts::getShortcut(Shortcuts::ID id) const
{
	assert(id < ID::NUM_SHORTCUTS);
	return shortcuts[id];
}

void Shortcuts::setShortcut(ID id, const Shortcut& shortcut)
{
	assert(id < ID::NUM_SHORTCUTS);
	auto oldType = shortcuts[id].type;
	shortcuts[id] = shortcut;
	if (oldType == one_of(Type::ALWAYS_LOCAL, Type::ALWAYS_GLOBAL)) {
		// cannot change this
		shortcuts[id].type = oldType;
	}
}

bool Shortcuts::getShortcutRepeat(ID id)
{
	assert(id < ID::NUM_SHORTCUTS);
	return shortcutRepeats[id];
}

zstring_view Shortcuts::getShortcutName(Shortcuts::ID id)
{
	assert(id < ID::NUM_SHORTCUTS);
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
	assert(id < ID::NUM_SHORTCUTS);
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
	assert(id < ID::NUM_SHORTCUTS);
	const auto& shortcut = shortcuts[id];
	if (shortcut.keyChord == ImGuiKey_None) return false;
	return checkShortcut({shortcut.keyChord, shortcut.type, getShortcutRepeat(id)});
}

} // namespace openmsx
