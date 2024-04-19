#include "SymbolManager.hh"

#include "CommandController.hh"
#include "File.hh"
#include "Interpreter.hh"
#include "TclObject.hh"

#include "narrow.hh"
#include "ranges.hh"
#include "static_vector.hh"
#include "stl.hh"
#include "StringOp.hh"
#include "unreachable.hh"
#include "view.hh"

#include <bit>
#include <cassert>
#include <fstream>

namespace openmsx {

zstring_view SymbolFile::toString(Type type)
{
	switch (type) {
		case Type::AUTO_DETECT: return "auto-detect";
		case Type::ASMSX:       return "asMSX";
		case Type::GENERIC:     return "generic";
		case Type::HTC:         return "htc";
		case Type::LINKMAP:     return "linkmap";
		case Type::NOICE:       return "NoICE";
		case Type::VASM:        return "vasm";
		default: UNREACHABLE;
	}
}

std::optional<SymbolFile::Type> SymbolFile::parseType(std::string_view str)
{
	if (str == "auto-detect") return Type::AUTO_DETECT;
	if (str == "asMSX")       return Type::ASMSX;
	if (str == "generic")     return Type::GENERIC;
	if (str == "htc")         return Type::HTC;
	if (str == "linkmap")     return Type::LINKMAP;
	if (str == "NoICE")       return Type::NOICE;
	if (str == "vasm")        return Type::VASM;
	return {};
}


SymbolManager::SymbolManager(CommandController& commandController_)
	: commandController(commandController_)
{
}

// detection logic taken from old openmsx-debugger, could probably be improved.
[[nodiscard]] SymbolFile::Type SymbolManager::detectType(std::string_view filename, std::string_view buffer)
{
	auto fname = StringOp::toLower(filename);

	if (fname.ends_with(".noi")) {
		// NoICE command file
		return SymbolFile::Type::NOICE;
	} else if (fname.ends_with(".map")) {
		// HiTech link map file
		return SymbolFile::Type::LINKMAP;
	} else if (fname.ends_with(".sym")) {
		// auto detect which sym file
		auto [line, _] = StringOp::splitOnFirst(buffer, "\n\r");
		if (line.starts_with("; Symbol table")) {
			return SymbolFile::Type::ASMSX;
		} else if (StringOp::containsCaseInsensitive(line, " %equ ")) { // TNIASM1
			return SymbolFile::Type::GENERIC;
		} else if (StringOp::containsCaseInsensitive(line, " equ ")) {
			return SymbolFile::Type::GENERIC;
		} else if (StringOp::containsCaseInsensitive(line, "Sections:")) {
			return SymbolFile::Type::VASM;
		} else {
			// this is a blunt conclusion but I don't know a way
			// to detect this file type
			return SymbolFile::Type::HTC;
		}
	} else if (fname.ends_with(".symbol") || fname.ends_with(".publics") || fname.ends_with(".sys")) {
		/* They are the same type of file. For some reason the Debian
			* manpage uses the extension ".sys"
			* pasmo doc -> pasmo [options] file.asm file.bin [file.symbol [file.publics] ]
			* pasmo manpage in Debian -> pasmo [options]  file.asm file.bin [file.sys]
		*/
		return SymbolFile::Type::GENERIC; // pasmo
	}
	return SymbolFile::Type::GENERIC;
}

[[nodiscard]] SymbolFile SymbolManager::loadLines(
	std::string_view filename, std::string_view buffer, SymbolFile::Type type,
	function_ref<std::optional<Symbol>(std::span<std::string_view>)> lineParser)
{
	SymbolFile result;
	result.filename = filename;
	result.type = type;

	static constexpr std::string_view whitespace = " \t\r";
	for (std::string_view fullLine : StringOp::split_view(buffer, '\n')) {
		auto [line, _] = StringOp::splitOnFirst(fullLine, ';');

		auto tokens = static_vector<std::string_view, 3 + 1>{from_range,
			view::take(StringOp::split_view<StringOp::REMOVE_EMPTY_PARTS>(line, whitespace), 3 + 1)};
		if (auto symbol = lineParser(tokens)) {
			result.symbols.push_back(std::move(*symbol));
		}
	}

	return result;
}

[[nodiscard]] std::optional<uint16_t> SymbolManager::parseValue(std::string_view str)
{
	if (str.ends_with('h') || str.ends_with('H')) { // hex
		str.remove_suffix(1);
		return StringOp::stringToBase<16, uint16_t>(str);
	}
	if (str.starts_with('$') || str.starts_with('#')) { // hex
		str.remove_prefix(1);
		return StringOp::stringToBase<16, uint16_t>(str);
	}
	if (str.starts_with('%')) { // bin
		str.remove_prefix(1);
		return StringOp::stringToBase<2, uint16_t>(str);
	}
	// this recognizes the prefixes "0x" or "0X" (for hexadecimal)
	//                          and "0b" or "0B" (for binary)
	// no prefix in interpreted as decimal
	// "0" as a prefix for octal is intentionally NOT supported
	return StringOp::stringTo<uint16_t>(str);
}

[[nodiscard]] std::optional<Symbol> SymbolManager::checkLabel(std::string_view label, uint16_t value)
{
	if (label.ends_with(':')) label.remove_suffix(1);
	if (label.empty()) return {};

	return Symbol{std::string(label), value};
}

[[nodiscard]] std::optional<Symbol> SymbolManager::checkLabelAndValue(std::string_view label, std::string_view value)
{
	if (auto num = parseValue(value)) {
		return checkLabel(label, *num);
	}
	return {};
}

[[nodiscard]] SymbolFile SymbolManager::loadGeneric(std::string_view filename, std::string_view buffer)
{
	auto parseLine = [](std::span<std::string_view> tokens) -> std::optional<Symbol> {
		if (tokens.size() != 3) return {};
		auto label = tokens[0];
		auto equ   = tokens[1];
		auto value = tokens[2];
		StringOp::casecmp cmp;
		if (!cmp(equ, "equ") &&         // TNIASM0, PASMO, SJASM, ...
		    !cmp(equ, "%equ")) return {};  // TNIASM1
		return checkLabelAndValue(label, value);
	};
	return loadLines(filename, buffer, SymbolFile::Type::GENERIC, parseLine);
}

[[nodiscard]] SymbolFile SymbolManager::loadNoICE(std::string_view filename, std::string_view buffer)
{
	auto parseLine = [](std::span<std::string_view> tokens) -> std::optional<Symbol> {
		if (tokens.size() != 3) return {};
		auto def   = tokens[0];
		auto label = tokens[1];
		auto value = tokens[2];
		if (StringOp::casecmp cmp; !cmp(def, "def")) return {};
		return checkLabelAndValue(label, value);
	};
	return loadLines(filename, buffer, SymbolFile::Type::NOICE, parseLine);
}

[[nodiscard]] SymbolFile SymbolManager::loadHTC(std::string_view filename, std::string_view buffer)
{
	// TODO check with real HTC file
	auto parseLine = [](std::span<std::string_view> tokens) -> std::optional<Symbol> {
		if (tokens.size() != 3) return {};
		auto label = tokens[0];
		auto value = tokens[1];
		// tokens[2] ???

		auto val = StringOp::stringToBase<16, uint16_t>(value);
		if (!val) return {};
		return checkLabel(label, *val);
	};
	return loadLines(filename, buffer, SymbolFile::Type::HTC, parseLine);
}

[[nodiscard]] SymbolFile SymbolManager::loadVASM(std::string_view filename, std::string_view buffer)
{
	SymbolFile result;
	result.filename = filename;
	result.type = SymbolFile::Type::VASM;

	static constexpr std::string_view whitespace = " \t\r";
	bool skipLines = true;
	for (std::string_view line : StringOp::split_view(buffer, '\n')) {
		if (skipLines) {
			if (line.starts_with("Symbols by value:")) {
				skipLines = false;
			}
			continue;
		}

		auto tokens = static_vector<std::string_view, 2 + 1>{from_range,
			view::take(StringOp::split_view<StringOp::REMOVE_EMPTY_PARTS>(line, whitespace), 2 + 1)};
		if (tokens.size() != 2) continue;
		auto value = tokens[0];
		auto label = tokens[1];

		if (auto val = StringOp::stringToBase<16, uint16_t>(value)) {
			if (auto symbol = checkLabel(label, *val)) {
				result.symbols.push_back(std::move(*symbol));
			}
		}
	}

	return result;
}

[[nodiscard]] SymbolFile SymbolManager::loadASMSX(std::string_view filename, std::string_view buffer)
{
	SymbolFile result;
	result.filename = filename;
	result.type = SymbolFile::Type::ASMSX;

	static constexpr std::string_view whitespace = " \t\r";
	bool symbolPart = false;
	for (std::string_view line : StringOp::split_view(buffer, '\n')) {
		if (line.starts_with(';')) {
			if (line.starts_with("; global and local")) {
				symbolPart = true;
			} else if (line.starts_with("; other")) {
				symbolPart = false;
			}
			continue;
		}
		if (!symbolPart) continue;

		// Possible formats are:  (checked in: https://github.com/Fubukimaru/asMSX/blob/master/src/dura.y#L3987)
		//   <abcd>h <name>         with <abcd> a 4-digit hex value
		//   <xy>h:<abcd>h <name>        <xy>   a 2-digit hex indicating the MegaRom Page (ignored)
		//                               <name> the symbol name
		auto tokens = static_vector<std::string_view, 2 + 1>{from_range,
			view::take(StringOp::split_view<StringOp::REMOVE_EMPTY_PARTS>(line, whitespace), 2 + 1)};
		if (tokens.size() != 2) continue;
		auto value = tokens[0];
		auto label = tokens[1];

		auto [f, l] = StringOp::splitOnFirst(value, ':');
		value = l.empty() ? f : l;

		if (auto symbol = checkLabelAndValue(label, value)) {
			result.symbols.push_back(std::move(*symbol));
		}
	}

	return result;
}

[[nodiscard]] std::optional<unsigned> SymbolManager::isHexDigit(char c)
{
	if ('0' <= c && c <= '9') return c - '0';
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	return {};
}
[[nodiscard]] std::optional<uint16_t> SymbolManager::is4DigitHex(std::string_view s)
{
	if (s.size() != 4) return {};
	unsigned value = 0;
	for (int i = 0; i < 4; ++i) {
		auto digit = isHexDigit(s[i]);
		if (!digit) return {};
		value = (value << 4) | *digit;
	}
	return narrow<uint16_t>(value);
}

[[nodiscard]] SymbolFile SymbolManager::loadLinkMap(std::string_view filename, std::string_view buffer)
{
	// Hi-Tech C link map file. Here's an example of such a file:
	//    https://github.com/artrag/C-experiments-for-msx/blob/master/START.MAP
	SymbolFile result;
	result.filename = filename;
	result.type = SymbolFile::Type::LINKMAP;

	static constexpr std::string_view whitespace = " \t\r";
	bool symbolPart = false;
	for (std::string_view line : StringOp::split_view(buffer, '\n')) {
		if (!symbolPart) {
			if (line.find("Symbol Table") != std::string_view::npos) { // c++23 contains()
				symbolPart = true;
			}
			continue;
		}
		// Here's an example of a few lines:
		//   asllmod            text    2CE7  asllsub            text    0AEE
		//   cret               text    2E58  csv                text    2E4C
		//   float_or_long_used (abs)   0001  indir              text    2E5F
		// Note:
		// * Multiple (2 in this case) symbols are defined in a single line.
		// * The width of the columns seems to be the same within a single file, but not across files (?)
		// * Looking at a single symbol:
		//   * There are 3 columns: name, psect, value
		//   * BUT the psect column can be empty!!!
		//     This in combination with an unknown column-width makes parsing difficult.
		//     The heuristic we use is that the last column must match: [0-9A-Fa-f]{4}
		auto tokens = StringOp::split_view<StringOp::REMOVE_EMPTY_PARTS>(line, whitespace);
		auto it = tokens.begin();
		auto et = tokens.end();
		while (it != et) {
			auto label = *it++;

			if (it == et) break;
			auto value = *it++; // this could either be the psect or the value column
			if (auto val = is4DigitHex(value)) {
				result.symbols.emplace_back(std::string(label), *val);
				continue;
			}

			if (it == et) break;
			value = *it++; // try again with 3rd column
			auto val = is4DigitHex(value);
			if (!val) break; // if this also doesn't work there's something wrong, skip this line
			result.symbols.emplace_back(std::string(label), *val);
		}
	}

	return result;
}

[[nodiscard]] SymbolFile SymbolManager::loadSymbolFile(const std::string& filename, SymbolFile::Type type)
{
	File file(filename);
	auto buf = file.mmap();
	std::string_view buffer(std::bit_cast<const char*>(buf.data()), buf.size());

	if (type == SymbolFile::Type::AUTO_DETECT) {
		type = detectType(filename, buffer);
	}
	assert(type != SymbolFile::Type::AUTO_DETECT);

	switch (type) {
		case SymbolFile::Type::ASMSX:
			return loadASMSX(filename, buffer);
		case SymbolFile::Type::GENERIC:
			return loadGeneric(filename, buffer);
		case SymbolFile::Type::HTC:
			return loadHTC(filename, buffer);
		case SymbolFile::Type::LINKMAP:
			return loadLinkMap(filename, buffer);
		case SymbolFile::Type::NOICE:
			return loadNoICE(filename, buffer);
		case SymbolFile::Type::VASM:
			return loadVASM(filename, buffer);
		default: UNREACHABLE;
	}
}

void SymbolManager::refresh()
{
	// Drop caches
	lookupValueCache.clear();

	// Allow to access symbol-values in Tcl expression with syntax: $sym(JIFFY)
	auto& interp = commandController.getInterpreter();
	TclObject arrayName("sym");
	interp.unsetVariable(arrayName.getString().c_str());
	for (const auto& file : files) {
		for (const auto& sym : file.symbols) {
			interp.setVariable(arrayName, TclObject(sym.name), TclObject(sym.value));
		}
	}

	if (observer) observer->notifySymbolsChanged();
}

bool SymbolManager::reloadFile(const std::string& filename, LoadEmpty loadEmpty, SymbolFile::Type type)
{
	auto file = loadSymbolFile(filename, type); // might throw
	if (file.symbols.empty() && loadEmpty == LoadEmpty::NOT_ALLOWED) return false;

	if (auto it = ranges::find(files, filename, &SymbolFile::filename);
	    it == files.end()) {
		files.push_back(std::move(file));
	} else {
		*it = std::move(file);
	}
	refresh();
	return true;
}

void SymbolManager::removeFile(std::string_view filename)
{
	auto it = ranges::find(files, filename, &SymbolFile::filename);
	if (it == files.end()) return; // not found
	files.erase(it);
	refresh();
}

void SymbolManager::removeAllFiles()
{
	files.clear();
	refresh();
}

std::optional<uint16_t> SymbolManager::parseSymbolOrValue(std::string_view str) const
{
	// linear search is fine: only used interactively
	// prefer an exact match
	for (const auto& file : files) {
		if (auto it = ranges::find(file.symbols, str, &Symbol::name);
		    it != file.symbols.end()) {
			return it->value;
		}
	}
	// but if not found, a case-insensitive match is fine as well
	for (const auto& file : files) {
		if (auto it = ranges::find_if(file.symbols, [&](const auto& sym) {
			return StringOp::casecmp{}(str, sym.name); });
		    it != file.symbols.end()) {
			return it->value;
		}
	}
	// also not found, then try to parse as a numerical value
	return parseValue(str);
}

std::span<Symbol const * const> SymbolManager::lookupValue(uint16_t value)
{
	if (lookupValueCache.empty()) {
		for (const auto& file : files) {
			for (const auto& sym : file.symbols) {
				auto [it, inserted] = lookupValueCache.try_emplace(sym.value, std::vector<const Symbol*>{});
				it->second.push_back(&sym);
			}
		}
	}
	if (auto* sym = lookup(lookupValueCache, value)) {
		return *sym;
	}
	return {};
}

std::string SymbolManager::getFileFilters()
{
	return "Auto-detect file type (*){.*},"
	       "asMSX 0.x symbol files (*.sym){.sym},"
	       "HiTech C link map files (*.map){.map},"
	       "HiTech C symbol files (*.sym){.sym},"
	       "NoICE command files (*.noi){.noi},"
	       "pasmo symbol files (*.symbol *.publics *.sys){.symbol,.publics,.sys},"
	       "tniASM 0.x symbol files (*.sym){.sym},"
	       "tniASM 1.x symbol files (*.sym){.sym},"
	       "vasm symbol files (*.sym){.sym}";
}

SymbolFile::Type SymbolManager::getTypeForFilter(std::string_view filter)
{
	if (filter.starts_with("Auto")) {
		return SymbolFile::Type::AUTO_DETECT;
	} else if (filter.starts_with("asMSX")) {
		return SymbolFile::Type::ASMSX;
	} else if (filter.starts_with("HiTechC link")) {
		return SymbolFile::Type::LINKMAP;
	} else if (filter.starts_with("HiTechC symbol")) {
		return SymbolFile::Type::HTC;
	} else if (filter.starts_with("NoICE")) {
		return SymbolFile::Type::NOICE;
	} else if (filter.starts_with("vasm")) {
		return SymbolFile::Type::VASM;
	} else {
		return SymbolFile::Type::GENERIC;
	}
}

} // namespace openmsx
