#include "UnicodeKeymap.hh"

#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "ParseUtils.hh"

#include "narrow.hh"
#include "ranges.hh"
#include "stl.hh"

#include <algorithm>
#include <bit>
#include <optional>

namespace openmsx {

using namespace std::literals;

/** Parses the given string as a hexadecimal integer.
  */
[[nodiscard]] static constexpr std::optional<unsigned> parseHex(std::string_view str)
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

[[nodiscard]] static constexpr KeyMatrixPosition parseRowCol(std::string_view token)
{
	auto rowCol = parseHex(token);
	if (!rowCol || *rowCol >= 0x100) {
		throw MSXException(
			(token.empty() ? "Missing" : "Wrong"),
			" <ROW><COL> value in keymap file");
	}
	if ((*rowCol >> 4) >= KeyMatrixPosition::NUM_ROWS) {
		throw MSXException("Too high row value in keymap file");
	}
	if ((*rowCol & 0x0F) >= KeyMatrixPosition::NUM_COLS) {
		throw MSXException("Too high column value in keymap file");
	}
	return KeyMatrixPosition(narrow_cast<uint8_t>(*rowCol));
}

UnicodeKeymap::UnicodeKeymap(std::string_view extension)
{
	auto filename = systemFileContext().resolve(
		tmpStrCat("keyboard_info/unicodemap.", extension));
	try {
		auto buf = File(filename).mmap<const char>();
		parseUnicodeKeyMapFile(std::string_view(buf.data(), buf.size())); // TODO c++23
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

void UnicodeKeymap::parseUnicodeKeyMapFile(std::string_view file)
{
	std::ranges::fill(relevantMods, 0);

	while (!file.empty()) {
		auto origLine = getLine(file);
		auto line = stripComments(origLine);

		auto token1 = getToken(line, ',');
		if (token1.empty()) continue; // empty line (or only whitespace / comments)

		if (token1.starts_with("DEADKEY")) {
			// DEADKEY<n> ...
			auto n = token1.substr(strlen("DEADKEY"));
			unsigned deadKeyIndex = [&] {
				if (n.empty()) {
					// The normal keywords are
					//    DEADKEY1  DEADKEY2  DEADKEY3
					// but for backwards compatibility also still recognize
					//    DEADKEY
					return unsigned(0);
				} else {
					auto d = parseHex(n);
					if (!d || *d > NUM_DEAD_KEYS) {
						throw MSXException(
							"Wrong deadkey number in keymap file. "
							"It must be 1..", NUM_DEAD_KEYS);
					}
					return *d - 1; // Make index 0 based instead of 1 based
				}
			}();

			auto pos = parseRowCol(getToken(line, ','));

			if (!getToken(line).empty()) {
				throw MSXException(
					"DEADKEY entry in keymap file cannot have modifiers");
			}

			deadKeys[deadKeyIndex] = KeyInfo(pos, 0);

		} else {
			// <unicode>, <rowcol>, [<modifiers>...]
			auto u = parseHex(token1);
			if (!u || *u > 0x1FBFF) {
				throw MSXException("Wrong unicode value in keymap file");
			}
			unsigned unicode = *u;

			// Parse second token. It must be <ROW><COL>
			auto rowColToken = getToken(line, ',');
			if (rowColToken == "--") {
				// Skip -- for now, it means the character cannot be typed.
				continue;
			}
			auto pos = parseRowCol(rowColToken);

			// Parse remaining tokens. It is an optional list of modifier keywords.
			uint8_t modMask = 0;
			while (true) {
				auto modToken = getToken(line);
				if (modToken.empty()) {
					break;
				} else if (modToken == "SHIFT") {
					modMask |= KeyInfo::SHIFT_MASK;
				} else if (modToken == "CTRL") {
					modMask |= KeyInfo::CTRL_MASK;
				} else if (modToken == "GRAPH") {
					modMask |= KeyInfo::GRAPH_MASK;
				} else if (modToken == "CAPSLOCK") {
					modMask |= KeyInfo::CAPS_MASK;
				} else if (modToken == "CODE") {
					modMask |= KeyInfo::CODE_MASK;
				} else {
					throw MSXException(
						"Invalid modifier \"", modToken, "\" in keymap file");
				}
			}

			mapData.emplace_back(Entry{.unicode = unicode, .keyInfo = KeyInfo(pos, modMask)});
			// Note: getRowCol() uses 3 bits for column, rowcol uses 4.
			relevantMods[pos.getRowCol()] |= modMask;
		}
	}

	std::ranges::sort(mapData, {}, &Entry::unicode);
}

} // namespace openmsx
