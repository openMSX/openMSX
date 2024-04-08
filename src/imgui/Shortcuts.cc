#include "Shortcuts.hh"
#include "imgui_internal.h"

#include "SettingsConfig.hh"

#include <array>

namespace openmsx {

//-- (Only) the table in this section will be edited when adding new shortcuts

struct AllShortcutInfo {
    ShortcutIndex index;
    ImGuiKeyChord keyChord;
    ShortcutType type;
    bool repeat;
    std::string_view name; // used in settings.xml
    std::string_view description; // shown in GUI
};

// When adding a new Shortcut, we only:
//  * Add a new value in the above 'enum ShortcutIndex'
//  * Add a single line in this table
// Note: if you look at the generated code on the right, this table does NOT end up in the generated code
//       instead it's only used to calculate various other tables (that do end up in the generated code)
//       those 'calculations' are all done at compile-time
// Note: the first column is ONLY used to verify (at-compile-time) whether the rows in this
//       table are in the same order as the values in ShortcutIndex
static constexpr auto allShortcutInfo = std::to_array<AllShortcutInfo>({
//   Index             keychord                    global                repeat name                       description
    {HEX_GOTO_ADDR,    ImGuiMod_Ctrl | ImGuiKey_G, ShortcutType::LOCAL,  false, "hex_editor_goto_address", "Go to address in hex viewer"},
    {STEP,             ImGuiKey_F6,                ShortcutType::GLOBAL, true,  "step",                    "Single step in debugger"},
    {BREAK,            ImGuiKey_F7,                ShortcutType::GLOBAL, false, "break",                   "Break CPU emulation in debugger"},
    {DISASM_GOTO_ADDR, ImGuiMod_Ctrl | ImGuiKey_G, ShortcutType::LOCAL,  false, "disasm_goto_address",     "Go to address in dissassembler"},
    {HEX_MOVE_UP,      ImGuiKey_UpArrow,           ShortcutType::LOCAL,  true,  "hex_editor_move_up",      "Move up in hex viewer"},
    {HEX_MOVE_DOWN,    ImGuiKey_DownArrow,         ShortcutType::LOCAL,  true,  "hex_editor_move_down",    "Move down in hex viewer"},
    {HEX_MOVE_LEFT,    ImGuiKey_LeftArrow,         ShortcutType::LOCAL,  true,  "hex_editor_move_left",    "Move left in hex viewer"},
    {HEX_MOVE_RIGHT,   ImGuiKey_RightArrow,        ShortcutType::LOCAL,  true,  "hex_editor_move_right",   "Move right hex viewer"},
});
static_assert(allShortcutInfo.size() == ShortcutIndex::NUM_SHORTCUTS);

std::string_view Shortcuts::getLargerDescription()
{
	static constexpr auto MAX_DESCRIPTION = []{
		size_t max = 0;
		size_t imax = 0;
		for (size_t i = 0; i < allShortcutInfo.size(); ++i) {
			const auto& info = allShortcutInfo[i];
			if (info.description.size() > max) {
				max = info.description.size();
				imax = i;
			}
		}
		return std::string_view(allShortcutInfo[imax].description);
	}();
	return MAX_DESCRIPTION;
}

// Note: if we forget a row we get a compile error.
//       if we swap the order of some rows we also get a compile error (when asserts are enabled)

static constexpr auto defaultShortcuts = []{
    std::array<Shortcuts::Data, ShortcutIndex::NUM_SHORTCUTS> result = {};
    for (int i = 0; i < ShortcutIndex::NUM_SHORTCUTS; ++i) {
        auto& all = allShortcutInfo[i];
        assert(all.index == i); // verify that table is in-order
        result[i].keyChord = all.keyChord;
        result[i].type     = all.type;
        result[i].repeat   = all.repeat;
    }
    return result;
}();

static constexpr auto shortcutNames = []{
    std::array<std::string_view, ShortcutIndex::NUM_SHORTCUTS> result = {};
    for (int i = 0; i < ShortcutIndex::NUM_SHORTCUTS; ++i) {
        result[i] = allShortcutInfo[i].name;
    }
    return result;
}();

static constexpr auto shortcutDescriptions = []{
    std::array<std::string_view, ShortcutIndex::NUM_SHORTCUTS> result = {};
    for (int i = 0; i < ShortcutIndex::NUM_SHORTCUTS; ++i) {
        result[i] = allShortcutInfo[i].description;
    }
    return result;
}();

Shortcuts::Shortcuts(SettingsConfig& config)
	: shortcuts{}
{
	config.setShortcuts(*this);
	setDefaultShortcuts();
}

void Shortcuts::setDefaultShortcuts()
{
	shortcuts = defaultShortcuts;
}

const Shortcuts::Data& Shortcuts::getDefaultShortcut(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	return defaultShortcuts[index];
}

const Shortcuts::Data& Shortcuts::getShortcut(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	return shortcuts[index];
}

void Shortcuts::setShortcut(ShortcutIndex index, std::optional<ImGuiKeyChord> keyChord, std::optional<ShortcutType> type, std::optional<bool> repeat)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	if (keyChord) shortcuts[index].keyChord = *keyChord;
	if (type)     shortcuts[index].type     = *type;
	if (repeat)   shortcuts[index].repeat   = *repeat;
}

void Shortcuts::setDefaultShortcut(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	shortcuts[index] = defaultShortcuts[index];
}

std::string_view Shortcuts::getShortcutName(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	return shortcutNames[index];
}

std::optional<ShortcutIndex> Shortcuts::getShortcutIndex(std::string_view name)
{
        auto it = ranges::find(shortcutNames, name);
        if (it == shortcutNames.end()) return {};
        return static_cast<ShortcutIndex>(std::distance(shortcutNames.begin(), it));
}

std::optional<ShortcutType> Shortcuts::getShortcutTypeValue(std::string_view name)
{
	static constexpr auto typeNames = std::array{ "local", "global" };
        auto it = ranges::find(typeNames, name);
        if (it == typeNames.end()) return {};
        return static_cast<ShortcutType>(std::distance(typeNames.begin(), it));
}

std::string_view Shortcuts::getShortcutDescription(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	return shortcutDescriptions[index];
}

bool Shortcuts::checkShortcut(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	auto& shortcut = shortcuts[index];
	auto flags = (shortcut.type == GLOBAL ? ImGuiInputFlags_RouteGlobalLow | ImGuiInputFlags_RouteUnlessBgFocused : ImGuiInputFlags_RouteUnlessBgFocused) | (shortcut.repeat ? ImGuiInputFlags_Repeat : 0);
	// invalid shortcut causes SIGTRAP so we test if keyChord is valid
	return shortcut.keyChord && ImGui::Shortcut(shortcut.keyChord, 0, flags);
}

}
