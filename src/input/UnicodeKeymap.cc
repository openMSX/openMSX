#include "UnicodeKeymap.hh"

#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "MSXException.hh"

#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "stl.hh"

#include <algorithm>
#include <bit>
#include <optional>

namespace openmsx {

using std::string_view;
namespace rg = std::ranges;

/** Parses the given string as a hexadecimal integer.
  */
[[nodiscard]] static constexpr std::optional<unsigned> parseHex(string_view str)
{
	if (str.empty()) {
		return {};
	}
	unsigned value = 0;
	for (const char c : str) {
		value *= 16;
		if ('0' <= c && c <= '9') {
			value += c - '0';
		} else if ('A' <= c && c <= 'F') {
			value += c - 'A' + 10;
		} else if ('a' <= c && c <= 'f') {
			value += c - 'a' + 10;
		} else {
			return {};
		}
	}
	return value;
}

/** Returns true iff the given character is a separator.
  * Separators are: comma, whitespace and hash mark.
  * Newline (\n) is not considered a separator.
  */
[[nodiscard]] static constexpr bool isSep(char c)
{
	return c == one_of(',',             // comma
	                   ' ', '\t', '\r', // whitespace
	                   '#');            // comment
}

/** Removes separator characters at the start of the given string reference.
  * Characters between a hash mark and the following newline are also skipped.
  */
static constexpr void skipSep(string_view& str)
{
	while (!str.empty()) {
		const char c = str.front();
		if (!isSep(c)) break;
		if (c == '#') {
			// Skip till end of line.
			while (!str.empty() && str.front() != '\n') str.remove_prefix(1);
			break;
		}
		str.remove_prefix(1);
	}
}

/** Returns the next token in the given string.
  * The token and any separators preceding it are removed from the string.
  */
[[nodiscard]] static constexpr string_view nextToken(string_view& str)
{
	skipSep(str);
	const auto* tokenBegin = str.data();
	while (!str.empty() && str.front() != '\n' && !isSep(str.front())) {
		// Pop non-separator character.
		str.remove_prefix(1);
	}
	return {tokenBegin, size_t(str.data() - tokenBegin)};
}


UnicodeKeymap::UnicodeKeymap(string_view keyboardType)
{
	auto filename = systemFileContext().resolve(
		tmpStrCat("unicodemaps/unicodemap.", keyboardType));
	try {
		File file(filename);
		auto buf = file.mmap();
		parseUnicodeKeyMapFile(
			string_view(std::bit_cast<const char*>(buf.data()), buf.size()));
		// TODO in the future we'll require the presence of
		//      "MSX-Video-Characterset" in the keyboard information
		//      file, then we don't need this fallback.
		if (!msxChars.has_value()) {
			msxChars.emplace("MSXVID.TXT");
		}
	} catch (FileException&) {
		throw MSXException("Couldn't load unicode keymap file: ", filename);
	}
}

UnicodeKeymap::KeyInfo UnicodeKeymap::get(unsigned unicode) const
{
	auto m = binary_find(mapData, unicode, {}, &Entry::unicode);
	return m ? m->keyInfo : KeyInfo();
}

UnicodeKeymap::KeyInfo UnicodeKeymap::getDeadKey(unsigned n) const
{
	assert(n < NUM_DEAD_KEYS);
	return deadKeys[n];
}

void UnicodeKeymap::parseUnicodeKeyMapFile(string_view data)
{
	ranges::fill(relevantMods, 0);

	while (!data.empty()) {
		if (data.front() == '\n') {
			// Next line.
			data.remove_prefix(1);
		}

		string_view token = nextToken(data);
		if (token.empty()) {
			// Skip empty line.
			continue;
		}

		if (token == "MSX-Video-Characterset:") {
			auto vidFileName = nextToken(data);
			if (vidFileName.empty()) {
				throw MSXException("Missing filename for MSX-Video-Characterset");
			}
			msxChars.emplace(vidFileName);
			continue;
		}

		// Parse first token: a unicode value or the keyword DEADKEY.
		unsigned unicode = 0;
		unsigned deadKeyIndex = 0;
		bool isDeadKey = token.starts_with("DEADKEY");
		if (isDeadKey) {
			token.remove_prefix(strlen("DEADKEY"));
			if (token.empty()) {
				// The normal keywords are
				//    DEADKEY1  DEADKEY2  DEADKEY3
				// but for backwards compatibility also still recognize
				//    DEADKEY
			} else {
				auto d = parseHex(token);
				if (!d || *d > NUM_DEAD_KEYS) {
					throw MSXException(
						"Wrong deadkey number in keymap file. "
						"It must be 1..", NUM_DEAD_KEYS);
				}
				deadKeyIndex = *d - 1; // Make index 0 based instead of 1 based
			}
		} else {
			auto u = parseHex(token);
			if (!u || *u > 0x1FBFF) {
				throw MSXException("Wrong unicode value in keymap file");
			}
			unicode = *u;
		}

		// Parse second token. It must be <ROW><COL>
		token = nextToken(data);
		if (token == "--") {
			// Skip -- for now, it means the character cannot be typed.
			continue;
		}
		auto rowcol = parseHex(token);
		if (!rowcol || *rowcol >= 0x100) {
			throw MSXException(
				(token.empty() ? "Missing" : "Wrong"),
				" <ROW><COL> value in keymap file");
		}
		if ((*rowcol >> 4) >= KeyMatrixPosition::NUM_ROWS) {
			throw MSXException("Too high row value in keymap file");
		}
		if ((*rowcol & 0x0F) >= KeyMatrixPosition::NUM_COLS) {
			throw MSXException("Too high column value in keymap file");
		}
		auto pos = KeyMatrixPosition(narrow_cast<uint8_t>(*rowcol));

		// Parse remaining tokens. It is an optional list of modifier keywords.
		uint8_t modMask = 0;
		while (true) {
			token = nextToken(data);
			if (token.empty()) {
				break;
			} else if (token == "SHIFT") {
				modMask |= KeyInfo::SHIFT_MASK;
			} else if (token == "CTRL") {
				modMask |= KeyInfo::CTRL_MASK;
			} else if (token == "GRAPH") {
				modMask |= KeyInfo::GRAPH_MASK;
			} else if (token == "CAPSLOCK") {
				modMask |= KeyInfo::CAPS_MASK;
			} else if (token == "CODE") {
				modMask |= KeyInfo::CODE_MASK;
			} else {
				throw MSXException(
					"Invalid modifier \"", token, "\" in keymap file");
			}
		}

		if (isDeadKey) {
			if (modMask != 0) {
				throw MSXException(
					"DEADKEY entry in keymap file cannot have modifiers");
			}
			deadKeys[deadKeyIndex] = KeyInfo(pos, 0);
		} else {
			mapData.emplace_back(Entry{unicode, KeyInfo(pos, modMask)});
			// Note: getRowCol() uses 3 bits for column, rowcol uses 4.
			relevantMods[pos.getRowCol()] |= modMask;
		}
	}

	rg::sort(mapData, {}, &Entry::unicode);
}

} // namespace openmsx
