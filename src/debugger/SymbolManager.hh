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
	Symbol(std::string n, uint16_t v, std::optional<uint8_t> s1, std::optional<uint16_t> s2)
		: name(std::move(n)), value(v), slot(s1), segment(s2) {} // clang-15 workaround

	std::string name;
	uint16_t value;
	std::optional<uint8_t> slot;
	std::optional<uint16_t> segment;

	auto operator<=>(const Symbol&) const = default;
};

struct SymbolFile
{
	enum class Type {
		FIRST = 0,

		AUTO_DETECT = FIRST,
		ASMSX,
		GENERIC, // includes PASMO, SJASM, TNIASM0, TNIASM1
		HTC,
		LINKMAP,
		NOICE,
		VASM,
		WLALINK_NOGMB,

		LAST,
	};
	[[nodiscard]] static zstring_view toString(Type type);
	[[nodiscard]] static std::optional<Type> parseType(std::string_view str);
	[[nodiscard]] auto& getSymbols() { return symbols; }

	std::optional<uint8_t> slot;
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
	bool reloadFile(const std::string& filename, LoadEmpty loadEmpty, SymbolFile::Type type, std::optional<uint8_t> slot = {});

	void removeFile(std::string_view filename);
	void removeAllFiles();

	[[nodiscard]] const auto& getFiles() const { return files; }
	[[nodiscard]] SymbolFile* findFile(std::string_view filename);
	[[nodiscard]] std::span<Symbol const * const> lookupValue(uint16_t value);
	[[nodiscard]] std::optional<uint16_t> parseSymbolOrValue(std::string_view s) const;

	[[nodiscard]] static std::string getFileFilters();
	[[nodiscard]] static SymbolFile::Type getTypeForFilter(std::string_view filter);

//private:
	// These should not be called directly, except by the unittest
	[[nodiscard]] static std::optional<unsigned> isHexDigit(char c);
	[[nodiscard]] static std::optional<uint16_t> is4DigitHex(std::string_view s);
	template <typename T>
	[[nodiscard]] static std::optional<T> parseValue(std::string_view str);
	[[nodiscard]] static std::optional<Symbol> checkLabel(std::string_view label, uint32_t value);
	[[nodiscard]] static std::optional<Symbol> checkLabelAndValue(std::string_view label, std::string_view value);
	[[nodiscard]] static std::optional<Symbol> checkLabelSegmentAndValue(std::string_view label, std::string_view value);
	[[nodiscard]] static SymbolFile::Type detectType(std::string_view filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadLines(
		std::string_view filename, std::string_view buffer, SymbolFile::Type type,
		function_ref<std::optional<Symbol>(std::span<std::string_view>)> lineParser);
	[[nodiscard]] static SymbolFile loadGeneric(std::string_view filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadNoICE(std::string_view filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadHTC(std::string_view filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadVASM(std::string_view filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadNoGmb(std::string_view filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadASMSX(std::string_view filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadLinkMap(std::string_view filename, std::string_view buffer);
	[[nodiscard]] static SymbolFile loadSymbolFile(const std::string& filename, SymbolFile::Type type, std::optional<uint8_t> slot = {});

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
