#include "KeyboardInfo.hh"

#include "ParseUtils.hh"

#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "MSXException.hh"

#include "StringOp.hh"
#include "ranges.hh"
#include "stl.hh"

namespace openmsx {

KeyboardInfo::KeyboardInfo(std::string_view keyboardType)
{
	auto filename = systemFileContext().resolve(
		tmpStrCat("keyboard_info/keyboard_info.", keyboardType));
	try {
		auto buf = File(filename).mmap<const char>();
		parseKeyboardInfoFile(std::string_view(buf.data(), buf.size())); // TODO c++23
	} catch (FileException&) {
		throw MSXException("Couldn't load keyboard_info file: ", filename);
	}

	if (!unicodeKeymap) {
		throw MSXException("Missing UnicodeMap reference in ", filename);
	}
	if (!msxChars) {
		throw MSXException("Missing MSX-Video-Characterset reference in ", filename);
	}
}

void KeyboardInfo::parseKeyboardInfoFile(std::string_view file)
{
	while (!file.empty()) {
		auto origLine = getLine(file);
		auto line = stripComments(origLine);

		auto token1 = getToken(line);
		if (token1.empty()) continue; // empty line (or only whitespace / comments)

		if (token1 == "UnicodeMap:") {
			auto unicodeMapFileExtension = getToken(line);
			if (unicodeMapFileExtension.empty()) {
				throw MSXException("Missing extension for UnicodeMap file");
			}
			unicodeKeymap.emplace(unicodeMapFileExtension);

		} else if (token1 == "MSX-Video-Characterset:") {
			auto vidFileName = getToken(line);
			if (vidFileName.empty()) {
				throw MSXException("Missing filename for MSX-Video-Characterset");
			}
			msxChars.emplace(vidFileName);

		} else if (token1 == "row") {
			auto rowToken = getToken(line);
			auto row = StringOp::stringTo<unsigned>(rowToken);
			if (!row || (*row >= KeyMatrixPosition::NUM_ROWS)) {
				throw MSXException("Invalid row value msx-keyboard-matrix-row line: ", rowToken);
			}

			auto separator = getToken(line);
			if (separator.size() != 1) {
				throw MSXException("Invalid msx-keyboard-matrix-row line: ", origLine);
			}

			for (int column = 7; column >= 0; --column) {
				auto name = getToken(line);
				if (name == separator) continue;

				auto sep2 = getToken(line);
				if (sep2 != separator) {
					throw MSXException("Invalid msx-keyboard-matrix-row line: ", origLine);
				}

				msxKeyNames.emplace_back(std::string(name), KeyMatrixPosition(*row, column));
			}

		} else {
			throw MSXException("Unrecognized directive in keyboard_info file: ", origLine);
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
