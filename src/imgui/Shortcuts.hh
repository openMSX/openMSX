#ifndef IMGUI_SHORTCUTS_HH
#define IMGUI_SHORTCUTS_HH

#include "ImGuiUtils.hh"

#include "strCat.hh"
#include "imgui_stdlib.h"

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace openmsx {

class ImGuiManager;
class GlobalSettings;
class SettingsConfig;

enum ShortcutIndex {
	GOTO_MEMORY_ADDRESS = 0,
	GOTO_DISASM_ADDRESS,
	NUM_SHORTCUTS,
};

enum ShortcutType {
	LOCAL,    // if ImGui widget has focus
	GLOBAL,   // inside ImGui layer only
};

class Shortcuts final
{
public:
	Shortcuts(const Shortcuts&) = delete;
	Shortcuts& operator=(const Shortcuts&) = delete;
	explicit Shortcuts(SettingsConfig& config_);

	// Shortcuts
	struct Data {
		ImGuiKeyChord keychord;
		bool local = false;
		bool repeat = false;
	};
	static std::string getShortcutName(ShortcutIndex index);
	static std::string getShortcutDescription(ShortcutIndex index);
	Shortcuts::Data& getShortcut(ShortcutIndex index);
	void setShortcut(ShortcutIndex index, std::optional<ImGuiKeyChord> keychord = {}, std::optional<bool> local = {}, std::optional<bool> repeat = {});
	void setDefaultShortcut(ShortcutIndex index);
	bool checkShortcut(ShortcutIndex index);

	template<typename XmlStream>
	void saveShortcuts(XmlStream& xml) const
	{
		int index = 0;
		xml.begin("shortcuts");
		for (const auto& data : shortcuts) {
			xml.begin("shortcut");
			if (data.keychord != ImGuiKey_None) {
				xml.attribute("keychord", std::to_string(static_cast<int>(data.keychord)));
			}
			if (data.repeat) xml.attribute("repeat", "true");
			if (data.local) xml.attribute("local", "true");
			xml.data(std::to_string(static_cast<int>(index++)));
			xml.end("shortcut");
		}
		xml.end("shortcuts");
	}

	void setDefaultShortcuts();

private:
	// Shortcuts
	std::array<Shortcuts::Data, ShortcutIndex::NUM_SHORTCUTS> shortcuts;
};

} // namespace openmsx

#endif
