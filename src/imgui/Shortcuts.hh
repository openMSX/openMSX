#ifndef IMGUI_SHORTCUTS_HH
#define IMGUI_SHORTCUTS_HH

#include "ImGuiUtils.hh"

#include "enumerate.hh"
#include "zstring_view.hh"

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
	struct Shortcut {
		ImGuiKeyChord keyChord = ImGuiKey_None;
		Type type = LOCAL;

		[[nodiscard]] bool operator==(const Shortcut& other) const = default;
	};
	struct ShortcutWithRepeat {
		ImGuiKeyChord keyChord = ImGuiKey_None;
		Type type = LOCAL;
		bool repeat = false;
	};

public:
	Shortcuts();
	Shortcuts(const Shortcuts&) = delete;
	Shortcuts(Shortcuts&&) = delete;
	Shortcuts& operator=(const Shortcuts&) = delete;
	Shortcuts& operator=(Shortcuts&&) = delete;
	~Shortcuts() = default;

	[[nodiscard]] static bool getShortcutRepeat(ID id);
	[[nodiscard]] static zstring_view getShortcutName(ID id);
	[[nodiscard]] static zstring_view getShortcutDescription(ID id);
	[[nodiscard]] static std::optional<ID> parseShortcutName(std::string_view name);
	[[nodiscard]] static std::optional<Type> parseType(std::string_view name);
	[[nodiscard]] static const Shortcut& getDefaultShortcut(ID id);

	[[nodiscard]] const Shortcut& getShortcut(ID id) const;
	void setShortcut(ID id, const Shortcut& shortcut);

	void setDefaultShortcuts();

	[[nodiscard]] bool checkShortcut(const ShortcutWithRepeat& shortcut) const;
	[[nodiscard]] bool checkShortcut(ID id) const;

	template<typename XmlStream>
	void saveShortcuts(XmlStream& xml) const
	{
		xml.with_tag("shortcuts", [&]{
			for (auto [id_, shortcut_] : enumerate(shortcuts)) {
				auto id = static_cast<ID>(id_);
				const auto& shortcut = shortcut_; // clang-15 workaround
				if (shortcut == getDefaultShortcut(id)) continue;
				xml.with_tag("shortcut", [&]{
					xml.attribute("key", getKeyChordName(shortcut.keyChord));
					if (shortcut.type == GLOBAL) xml.attribute("type", "global");
					xml.data(getShortcutName(id));
				});
			}
		});
	}

private:
	std::array<Shortcut, ID::NUM_SHORTCUTS> shortcuts;
};

} // namespace openmsx

#endif
