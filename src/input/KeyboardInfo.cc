#include "KeyboardInfo.hh"

#include "StringOp.hh"
#include "ranges.hh"
#include "stl.hh"
#include "xrange.hh"

#include <algorithm>

namespace openmsx {

// The name of a key that types the given character (without modifiers).
static std::string charKeyName(unsigned u)
{
	if (('0' <= u && u <= '9') || ('A' <= u && u <= 'Z')) return std::string(1, char(u));
	if ('a' <= u && u <= 'z') return std::string(1, char(u - 'a' + 'A'));
	switch (u) {
		case 0x08: return "BACKSPACE";
		case 0x09: return "TAB";
		case 0x0B: return "HOME";
		case 0x0D: return "RETURN";
		case 0x1B: return "ESCAPE";
		case 0x1C: return "RIGHT";
		case 0x1D: return "LEFT";
		case 0x1E: return "UP";
		case 0x1F: return "DOWN";
		case 0x20: return "SPACE";
		case 0x7F: return "DELETE";
		default:   return {};
	}
}

KeyboardInfo::KeyboardInfo(
	const UnicodeKeymap& unicodeKeymap,
	const array_with_enum_index<UnicodeKeymap::KeyInfo::Modifier, KeyMatrixPosition>& modifierPos)
{
	auto add = [&](std::string name, KeyMatrixPosition pos) {
		if (name.empty() || !pos.isValid()) return;
		if (contains(msxKeyNames, name, &MsxKeyName::name) ||
		    contains(msxKeyNames, pos, &MsxKeyName::key)) return;
		msxKeyNames.emplace_back(std::move(name), pos);
	};
	using enum UnicodeKeymap::KeyInfo::Modifier;
	add("SHIFT",    modifierPos[SHIFT]);
	add("CTRL",     modifierPos[CTRL]);
	add("GRAPH",    modifierPos[GRAPH]);
	add("CAPSLOCK", modifierPos[CAPS]);
	add("CODE",     modifierPos[CODE]);
	for (auto row : xrange(KeyMatrixPosition::NUM_ROWS)) {
		for (auto col : xrange(KeyMatrixPosition::NUM_COLS)) {
			KeyMatrixPosition pos(row, col);
			if (auto u = unicodeKeymap.getUnicode(pos, 0)) add(charKeyName(*u), pos);
		}
	}
	std::ranges::sort(msxKeyNames, StringOp::caseless{}, &MsxKeyName::name);
}

KeyMatrixPosition KeyboardInfo::getMsxKeyPosition(std::string_view msxKeyName) const
{
	if (auto k = binary_find(msxKeyNames, msxKeyName, StringOp::caseless{}, &MsxKeyName::name)) {
		return k->key;
	}
	return {}; // not found -> return invalid position
}

std::vector<std::string_view> KeyboardInfo::getMsxKeyNames() const
{
	return to_vector<std::string_view>(std::views::transform(msxKeyNames, &MsxKeyName::name));
}

} // namespace openmsx
