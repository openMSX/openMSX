#ifndef KEYBOARDINFO_HH
#define KEYBOARDINFO_HH

#include "UnicodeKeymap.hh"

#include "stl.hh"

#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

/** The names of the keys of an MSX keyboard, with their matrix positions:
  * the modifier keys, and the keys that type a letter, digit or control
  * character without modifiers (according to the unicodemap).
  */
class KeyboardInfo {
public:
	KeyboardInfo(const UnicodeKeymap& unicodeKeymap,
	             const array_with_enum_index<UnicodeKeymap::KeyInfo::Modifier, KeyMatrixPosition>& modifierPos);

	[[nodiscard]] KeyMatrixPosition getMsxKeyPosition(std::string_view msxKeyName) const;
	[[nodiscard]] std::vector<std::string_view> getMsxKeyNames() const;

private:
	struct MsxKeyName {
		std::string name;
		KeyMatrixPosition key; // msx keyboard matrix position
	};
	std::vector<MsxKeyName> msxKeyNames; // sorted on name
};

} // namespace openmsx

#endif
