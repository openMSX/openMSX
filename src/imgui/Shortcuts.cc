#include "Shortcuts.hh"
#include "imgui_internal.h"

#include "SettingsConfig.hh"

#include <array>

namespace openmsx {

static constexpr auto defaultShortcuts = std::array{
	Shortcuts::Data{ImGuiMod_Ctrl | ImGuiKey_G, ShortcutType::LOCAL, false},
	Shortcuts::Data{ImGuiMod_None             , ShortcutType::LOCAL, false},
};

Shortcuts::Shortcuts(SettingsConfig& config)
	: shortcuts{}
{
	config.setShortcuts(*this);
	setDefaultShortcuts();
}

void Shortcuts::setDefaultShortcuts()
{
	int index = 0;
	for (const auto& shortcut : defaultShortcuts) {
		setShortcut(static_cast<ShortcutIndex>(index++), shortcut.keychord, shortcut.local, shortcut.repeat);
	}
}

Shortcuts::Data& Shortcuts::getShortcut(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	return shortcuts[index];
}

void Shortcuts::setShortcut(ShortcutIndex index, std::optional<ImGuiKeyChord> keychord, std::optional<bool> local, std::optional<bool> repeat)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	if (keychord) shortcuts[index].keychord = *keychord;
	if (local) shortcuts[index].local = *local;
	if (repeat) shortcuts[index].repeat = *repeat;
}

void Shortcuts::setDefaultShortcut(ShortcutIndex index)
{
	const auto& shortcut = defaultShortcuts[index];
	setShortcut(index, shortcut.keychord, shortcut.local, shortcut.repeat);
}

std::string Shortcuts::getShortcutName(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	static constexpr auto shortcutNames = std::array{
		"GotoMemoryAddress", "GotoDisasmAddress"
	};
	return shortcutNames[index];
}

std::string Shortcuts::getShortcutDescription(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	static constexpr auto shortcutNames = std::array{
		"go to memory widget address",
		"go to disassembler widget address",
	};
	return shortcutNames[index];
}

bool Shortcuts::checkShortcut(ShortcutIndex index)
{
	assert(index < ShortcutIndex::NUM_SHORTCUTS);
	auto& shortcut = shortcuts[index];
	auto flags = (shortcut.local ? ImGuiInputFlags_RouteUnlessBgFocused : ImGuiInputFlags_RouteGlobalLow | ImGuiInputFlags_RouteUnlessBgFocused)
		| (shortcut.repeat ? ImGuiInputFlags_Repeat : 0);
	// invalid shortcut causes SIGTRAP so we test if keychord is valid
	return shortcut.keychord && ImGui::Shortcut(shortcut.keychord, 0, flags);
}

}
