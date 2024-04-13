#ifndef IMGUI_SHORTCUTS_HH
#define IMGUI_SHORTCUTS_HH

#include "ImGuiUtils.hh"

#include <array>
#include <optional>
#include <string_view>

namespace openmsx {

class Shortcuts
{
public:
	enum ID {
		HEX_GOTO_ADDR,
		STEP,
		BREAK,
		DISASM_GOTO_ADDR,

		NUM_SHORTCUTS,
		INVALID = NUM_SHORTCUTS,
	};
	enum Type {
		LOCAL,
		GLOBAL,
	};
	struct Data {
		ImGuiKeyChord keyChord = ImGuiKey_None;
		Type type = LOCAL;
		bool repeat = false;
		[[nodiscard]] bool operator==(const Data& other) const = default;
	};

public:
	Shortcuts(const Shortcuts&) = delete;
	Shortcuts& operator=(const Shortcuts&) = delete;
	Shortcuts();

	[[nodiscard]] static std::string_view getShortcutName(ID id);
	[[nodiscard]] static std::string_view getLargerDescription();
	[[nodiscard]] static std::string_view getShortcutDescription(ID id);
	[[nodiscard]] static std::optional<ID> parseShortcutName(std::string_view name);
	[[nodiscard]] static std::optional<Type> parseType(std::string_view name);
	[[nodiscard]] static const Shortcuts::Data& getDefaultShortcut(ID id);

	[[nodiscard]] const Shortcuts::Data& getShortcut(ID id);
	void setShortcut(ID id, std::optional<ImGuiKeyChord> keyChord = {}, std::optional<Type> type = {}, std::optional<bool> repeat = {});
	void setDefaultShortcut(ID id);
	void setDefaultShortcuts();
	[[nodiscard]] bool checkShortcut(ImGuiKeyChord keyChord, Shortcuts::Type type, bool repeat);
	[[nodiscard]] bool checkShortcut(ID id);

	template<typename XmlStream>
	void saveShortcuts(XmlStream& xml) const
	{
		int id = 0;
		xml.with_tag("shortcuts", [&]{
			for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it, ++id) {
				const auto& data = *it;
				if (data == getDefaultShortcut(static_cast<ID>(id))) {
					continue;
				}
				xml.with_tag("shortcut", [&]{
					xml.attribute("key", getKeyChordName(data.keyChord));
					if (data.type == GLOBAL) xml.attribute("type", "global");
					if (data.repeat) xml.attribute("repeat", "true");
					xml.data(getShortcutName(static_cast<ID>(id)));
				});
			}
		});
	}

private:
	std::array<Shortcuts::Data, ID::NUM_SHORTCUTS> shortcuts;
};

} // namespace openmsx

#endif
