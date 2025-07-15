#ifndef IMGUI_SHORTCUTS_HH
#define IMGUI_SHORTCUTS_HH

#include "ImGuiUtils.hh"

#include "enumerate.hh"
#include "stl.hh"
#include "zstring_view.hh"

#include <cstdint>
#include <optional>
#include <string_view>

namespace openmsx {

class Shortcuts
{
public:
	enum class ID : uint8_t {
		HEX_GOTO_ADDR,
		DEBUGGER_STEP_IN,
		DEBUGGER_STEP_OVER,
		DEBUGGER_STEP_OUT,
		DEBUGGER_STEP_BACK,
		DEBUGGER_BREAK_CONTINUE,
		DISASM_GOTO_ADDR,
		DISASM_RUN_TO_ADDR,
		DISASM_TOGGLE_BREAKPOINT,

		NUM,
		INVALID = NUM
	};
	enum class Type : uint8_t {
		LOCAL,
		GLOBAL,
		ALWAYS_LOCAL,
		ALWAYS_GLOBAL,
	};
	struct Shortcut {
		ImGuiKeyChord keyChord = ImGuiKey_None;
		Type type = Type::LOCAL;

		[[nodiscard]] bool operator==(const Shortcut& other) const = default;
	};
	struct ShortcutWithRepeat {
		ImGuiKeyChord keyChord = ImGuiKey_None;
		Type type = Type::LOCAL;
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
			for (auto [id_, shortcut] : enumerate(shortcuts)) {
				auto id = static_cast<ID>(id_);
				if (shortcut == getDefaultShortcut(id)) continue;
				xml.with_tag("shortcut", [&]{
					xml.attribute("key", getKeyChordName(shortcut.keyChord));
					// don't save the 'ALWAYS_xxx' values
					if (shortcut.type == Type::GLOBAL) xml.attribute("type", "global");
					xml.data(getShortcutName(id));
				});
			}
		});
	}

private:
	array_with_enum_index<ID, Shortcut> shortcuts;
};

} // namespace openmsx

#endif
