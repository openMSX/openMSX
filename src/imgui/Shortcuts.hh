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
	HEX_GOTO_ADDR = 0,
	STEP,
	BREAK,
	DISASM_GOTO_ADDR,
	HEX_MOVE_UP,
	HEX_MOVE_DOWN,
	HEX_MOVE_LEFT,
	HEX_MOVE_RIGHT,
	NUM_SHORTCUTS,

	INVALID,
};

enum ShortcutType {
	LOCAL,
	GLOBAL,
};

class Shortcuts
{
public:
	Shortcuts(const Shortcuts&) = delete;
	Shortcuts& operator=(const Shortcuts&) = delete;
	explicit Shortcuts(SettingsConfig& config_);

	// Shortcuts
	struct Data {
		ImGuiKeyChord keyChord;
		ShortcutType type;
		bool repeat = false;
		[[nodiscard]] bool operator==(const Data& other) const = default;
	};
	static std::string_view getShortcutName(ShortcutIndex index);
	static std::string_view getLargerDescription();
	static std::string_view getShortcutDescription(ShortcutIndex index);
	static std::optional<ShortcutIndex> getShortcutIndex(std::string_view name);
	static std::optional<ShortcutType> getShortcutTypeValue(std::string_view name);
	const Shortcuts::Data& getShortcut(ShortcutIndex index);
	static const Shortcuts::Data& getDefaultShortcut(ShortcutIndex index);
	void setShortcut(ShortcutIndex index, std::optional<ImGuiKeyChord> keyChord = {}, std::optional<ShortcutType> type = {}, std::optional<bool> repeat = {});
	void setDefaultShortcut(ShortcutIndex index);
	bool checkShortcut(ShortcutIndex index);

	template<typename XmlStream>
	void saveShortcuts(XmlStream& xml) const
	{
		int index = 0;
		xml.with_tag("shortcuts", [&]{
			for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it, ++index) {
				const auto& data = *it;
				if (data == getDefaultShortcut(static_cast<ShortcutIndex>(index))) {
					continue;
				}
				xml.with_tag("shortcut", [&]{
					xml.attribute("key", getKeyChordName(data.keyChord));
					if (data.type == GLOBAL) xml.attribute("type", "global");
					if (data.repeat) xml.attribute("repeat", "true");
					xml.data(getShortcutName(static_cast<ShortcutIndex>(index)));
				});
			}
		});
	}

	void setDefaultShortcuts();

private:
	// Shortcuts
	std::array<Shortcuts::Data, ShortcutIndex::NUM_SHORTCUTS> shortcuts;
};

} // namespace openmsx

#endif
