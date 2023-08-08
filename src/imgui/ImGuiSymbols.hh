#ifndef IMGUI_SYMBOLS_HH
#define IMGUI_SYMBOLS_HH

#include "ImGuiPart.hh"

#include "hash_map.hh"

#include <cstdint>
#include <optional>
#include <vector>

namespace openmsx {

class ImGuiManager;

struct Symbol {
	std::string name;
	std::string file;
	uint16_t value;
};

// Parse the data in 'buffer' and return the resulting Symbol objects.
// The Symbol's refer to 'filename', but this parse function itself doesn't
// perform any filesystem access (useful for unittest).
std::vector<Symbol> parseSymbolBuffer(const std::string& filename, std::string_view buffer);

class ImGuiSymbols final : public ImGuiPart
{
public:
	ImGuiSymbols(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "symbols"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadStart() override;
	void loadLine(std::string_view name, zstring_view value) override;
	void loadEnd() override;
	void paint(MSXMotherBoard* motherBoard) override;

	[[nodiscard]] const std::vector<std::string>& getFiles();
	[[nodiscard]] std::string_view lookupValue(uint16_t value);
	std::optional<uint16_t> parseSymbolOrValue(std::string_view str) const;

public:
	bool show = false;

private:
	void dropCaches();

	void reload(const std::string& file);
	void reload1(const std::string& file);
	void reloadAll();
	void remove(const std::string& file);
	void remove2(const std::string& file);
	void removeAll();
	std::vector<Symbol> load(const std::string& filename);

private:
	ImGuiManager& manager;
	std::vector<Symbol> symbols;
	std::vector<std::string> filesCache; // calculated from 'symbols'
	hash_map<uint16_t, std::string> lookupValueCache; // calculated from 'symbols'

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiSymbols::show}
	};
};

} // namespace openmsx

#endif
