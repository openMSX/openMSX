#ifndef KEYBOARDINFO_HH
#define KEYBOARDINFO_HH

#include "KeyMatrixPosition.hh"
#include "MsxChar2Unicode.hh"
#include "UnicodeKeymap.hh"

#include <cassert>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

namespace openmsx {

class KeyboardInfo {
public:
	explicit KeyboardInfo(std::string_view keyboardType);

	[[nodiscard]] KeyMatrixPosition getMsxKeyPosition(std::string_view msxKeyName) const;
	[[nodiscard]] std::vector<std::string_view> getMsxKeyNames() const;

	[[nodiscard]] const UnicodeKeymap& getUnicodeKeymap() const {
		assert(unicodeKeymap);
		return *unicodeKeymap;
	}

	[[nodiscard]] const MsxChar2Unicode& getMsxChars() const {
		assert(msxChars);
		return *msxChars;
	}

	/** The character that the key at 'pos' types with exactly the
	  * modifiers in 'modMask', see UnicodeKeymap::getUnicode() (with the
	  * character set of this keyboard).
	  */
	[[nodiscard]] std::optional<unsigned> getUnicode(KeyMatrixPosition pos, uint8_t modMask) const {
		return getUnicodeKeymap().getUnicode(pos, modMask, getMsxChars());
	}
	/** Is the key at 'pos' one of the dead keys? */
	[[nodiscard]] bool isDeadKey(KeyMatrixPosition pos) const {
		return getUnicodeKeymap().isDeadKey(pos);
	}

private:
	void parseKeyboardInfoFile(std::string_view file);

private:
	struct MsxKeyName {
		std::string name;
		KeyMatrixPosition key; // msx keyboard matrix position
	};
	std::vector<MsxKeyName> msxKeyNames; // sorted on name

	std::optional<UnicodeKeymap> unicodeKeymap; // optional because of delayed initialization
	std::optional<MsxChar2Unicode> msxChars; // optional because of delayed initialization
};

} // namespace openmsx

#endif
