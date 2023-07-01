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

class ImGuiSymbols final : public ImGuiPart
{
public:
	ImGuiSymbols(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "symbols"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

	[[nodiscard]] const std::vector<std::string>& getFiles();
	[[nodiscard]] std::string_view lookupValue(uint16_t value);
	std::optional<uint16_t> parseSymbolOrValue(std::string_view str) const;

public:
	bool show = false;

private:
	void dropCaches();

	void reload(const std::string& file);
	void remove(const std::string& file);
	void remove2(const std::string& file);
	void removeAll();
	std::vector<Symbol> load(const std::string& filename);

private:
	ImGuiManager& manager;
	std::vector<Symbol> symbols;
	std::vector<std::string> filesCache; // calculated from 'symbols'
	hash_map<uint16_t, std::string_view> lookupValueCache; // calculated from 'symbols'

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show", &ImGuiSymbols::show}
	};
};

} // namespace openmsx

#endif
