#include "Shortcuts.hh"

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
    std::string_view name; // used in settings.xml
    std::string_view description; // shown in GUI
};
using enum Shortcuts::ID;
using enum Shortcuts::Type;
static constexpr auto allShortcutInfo = std::to_array<AllShortcutInfo>({
	{HEX_GOTO_ADDR,    ImGuiMod_Ctrl | ImGuiKey_G, LOCAL,  false, "hex_editor_goto_address", "Go to address in hex viewer"},
	{HEX_MOVE_UP,      ImGuiKey_UpArrow,           LOCAL,  true,  "hex_editor_move_up",      "Move up in hex viewer"},
	{HEX_MOVE_DOWN,    ImGuiKey_DownArrow,         LOCAL,  true,  "hex_editor_move_down",    "Move down in hex viewer"},
	{HEX_MOVE_LEFT,    ImGuiKey_LeftArrow,         LOCAL,  true,  "hex_editor_move_left",    "Move left in hex viewer"},
	{HEX_MOVE_RIGHT,   ImGuiKey_RightArrow,        LOCAL,  true,  "hex_editor_move_right",   "Move right hex viewer"},
	{STEP,             ImGuiKey_F6,                GLOBAL, true,  "step",                    "Single step in debugger"},
	{BREAK,            ImGuiKey_F7,                GLOBAL, false, "break",                   "Break CPU emulation in debugger"},
	{DISASM_GOTO_ADDR, ImGuiMod_Ctrl | ImGuiKey_G, LOCAL,  false, "disasm_goto_address",     "Scroll to address in disassembler"},
});
static_assert(allShortcutInfo.size() == Shortcuts::ID::NUM_SHORTCUTS);

static constexpr auto defaultShortcuts = []{
	std::array<Shortcuts::Data, Shortcuts::ID::NUM_SHORTCUTS> result = {};
	for (int i = 0; i < Shortcuts::ID::NUM_SHORTCUTS; ++i) {
		const auto& all = allShortcutInfo[i];
		assert(all.id == i); // verify that rows are in-order
		result[i].keyChord = all.keyChord;
		result[i].type = all.type;
		result[i].repeat = all.repeat;
	}
	return result;
}();

static constexpr auto shortcutNames = []{
	std::array<std::string_view, Shortcuts::ID::NUM_SHORTCUTS> result = {};
	for (int i = 0; i < Shortcuts::ID::NUM_SHORTCUTS; ++i) {
		result[i] = allShortcutInfo[i].name;
	}
	return result;
}();

static constexpr auto shortcutDescriptions = []{
	std::array<std::string_view, Shortcuts::ID::NUM_SHORTCUTS> result = {};
	for (int i = 0; i < Shortcuts::ID::NUM_SHORTCUTS; ++i) {
		result[i] = allShortcutInfo[i].description;
	}
	return result;
}();

// TODO this needs to be reworked (doesn't work because of proportional font)
std::string_view Shortcuts::getLargerDescription()
{
	static constexpr auto MAX_DESCRIPTION = []{
		size_t maxSize = 0;
		size_t maxIdx = 0;
		for (size_t i = 0; i < shortcutDescriptions.size(); ++i) {
			const auto& description = shortcutDescriptions[i];
			if (description.size() > maxSize) {
				maxSize = description.size();
				maxIdx = i;
			}
		}
		return shortcutDescriptions[maxIdx];
	}();
	return MAX_DESCRIPTION;
}

Shortcuts::Shortcuts()
{
	setDefaultShortcuts();
}

void Shortcuts::setDefaultShortcuts()
{
	shortcuts = defaultShortcuts;
}

const Shortcuts::Data& Shortcuts::getDefaultShortcut(Shortcuts::ID id)
{
	assert(id < Shortcuts::ID::NUM_SHORTCUTS);
	return defaultShortcuts[id];
}

const Shortcuts::Data& Shortcuts::getShortcut(Shortcuts::ID id)
{
	assert(id < Shortcuts::ID::NUM_SHORTCUTS);
	return shortcuts[id];
}

void Shortcuts::setShortcut(Shortcuts::ID id, std::optional<ImGuiKeyChord> keyChord, std::optional<Type> type, std::optional<bool> repeat)
{
	assert(id < Shortcuts::ID::NUM_SHORTCUTS);
	if (keyChord) shortcuts[id].keyChord = *keyChord;
	if (type)     shortcuts[id].type     = *type;
	if (repeat)   shortcuts[id].repeat   = *repeat;
}

void Shortcuts::setDefaultShortcut(Shortcuts::ID id)
{
	assert(id < Shortcuts::ID::NUM_SHORTCUTS);
	shortcuts[id] = defaultShortcuts[id];
}

std::string_view Shortcuts::getShortcutName(Shortcuts::ID id)
{
	assert(id < Shortcuts::ID::NUM_SHORTCUTS);
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

std::string_view Shortcuts::getShortcutDescription(Shortcuts::ID id)
{
	assert(id < Shortcuts::ID::NUM_SHORTCUTS);
	return shortcutDescriptions[id];
}

bool Shortcuts::checkShortcut(Shortcuts::ID id)
{
	assert(id < Shortcuts::ID::NUM_SHORTCUTS);
	auto& shortcut = shortcuts[id];
	if (shortcut.keyChord == ImGuiKey_None) return false;
	auto flags = (shortcut.type == GLOBAL ? ImGuiInputFlags_RouteGlobalLow : 0)
	           | ImGuiInputFlags_RouteUnlessBgFocused
	           | (shortcut.repeat ? ImGuiInputFlags_Repeat : 0);
	return ImGui::Shortcut(shortcut.keyChord, 0, flags);
}

} // namespace openmsx
