#ifndef SYMBOL_MANAGER_HH
#define SYMBOL_MANAGER_HH

#include "function_ref.hh"
#include "hash_map.hh"
#include "zstring_view.hh"

#include <cassert>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

class CommandController;

struct Symbol
{
	Symbol(std::string n, uint16_t v)
		: name(std::move(n)), value(v) {} // clang-15 workaround

	std::string name;
	uint16_t value;

	auto operator<=>(const Symbol&) const = default;
};

struct SymbolFile
{
	enum class Type {
		AUTO_DETECT = 0,
		ASMSX,
		GENERIC, // includes PASMO, SJASM, TNIASM0, TNIASM1
		HTC,
		LINKMAP,
		NOICE,
		VASM,

		FIRST = AUTO_DETECT,
		LAST = VASM + 1,
	};
	[[nodiscard]] static zstring_view toString(Type type);
	[[nodiscard]] static std::optional<Type> parseType(std::string_view str);

	std::string filename;
	std::vector<Symbol> symbols;
	Type type;
};

struct SymbolObserver
{
	virtual void notifySymbolsChanged() = 0;
};

class SymbolManager
{
public:
	explicit SymbolManager(CommandController& commandController);

	void setObserver(SymbolObserver* observer_) {
		assert(!observer || !observer_);
		observer = observer_;
	}

	// Load or reload a file.
	// * Throws on failure to read from file. In that case the existing file is left unchanged.
	// * Parse errors in the file are ignored.
	// * When the file contains no symbols and when 'allowEmpty=false' the
	//   existing file is not replaced and this method returns 'false'.
	//   Otherwise it return 'true'.
	enum class LoadEmpty { ALLOWED, NOT_ALLOWED };
	bool reloadFile(const std::string& filename, LoadEmpty loadEmpty, SymbolFile::Type type);

	void removeFile(std::string_view filename);
	void removeAllFiles();

	[[nodiscard]] const auto& getFiles() const { return files; }
	[[nodiscard]] std::span<Symbol const * const> lookupValue(uint16_t value);
	[[nodiscard]] std::optional<uint16_t> parseSymbolOrValue(std::string_view s) const;

	[[nodiscard]] static std::string getFileFilters();
	[[nodiscard]] static SymbolFile::Type getTypeForFilter(std::string_view filter);

//private:
	// These should not be called directly, except by the unittest
	[[nodiscard]] static std::optional<unsigned> isHexDigit(char c);
	[[nodiscard]] static std::optional<uint16_t> is4DigitHex(std::string_view s);
	[[nodiscard]] static std::optional<uint16_t> parseValue(std::string_view str);
	[[nodiscard]] static std::optional<Symbol> checkLabel(std::string_view label, uint16_t value);
	[[nodiscard]] static std::optional<Symbol> checkLabelAndValue(std::string_view label, std::string_view value);
	[[nodiscard]] static SymbolFile::Type detectType(const std::string& filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadLines(
		const std::string& filename, std::string_view buffer, SymbolFile::Type type,
		function_ref<std::optional<Symbol>(std::span<std::string_view>)> lineParser);
	[[nodiscard]] static SymbolFile loadGeneric(const std::string& filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadNoICE(const std::string& filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadHTC(const std::string& filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadVASM(const std::string& filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadASMSX(const std::string& filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadLinkMap(const std::string& filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadSymbolFile(const std::string& filename, SymbolFile::Type type);

private:
	void refresh();

private:
	CommandController& commandController;
	SymbolObserver* observer = nullptr; // only one for now, could become a vector later
	std::vector<SymbolFile> files;
	hash_map<uint16_t, std::vector<const Symbol*>> lookupValueCache; // calculated from 'files'
};


} // namespace openmsx

#endif
