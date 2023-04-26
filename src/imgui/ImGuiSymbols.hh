#ifndef IMGUI_SYMBOLS_HH
#define IMGUI_SYMBOLS_HH

#include "ImGuiPart.hh"

#include <cstdint>
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

public:
	bool show = false;

private:
	void dropCaches();
	[[nodiscard]] const std::vector<std::string>& getFiles();

	void reload(const std::string& file);
	void remove(const std::string& file);
	void removeAll();
	std::vector<Symbol> load(const std::string& filename);

private:
	ImGuiManager& manager;
	std::vector<Symbol> symbols;
	std::vector<std::string> filesCache; // calculated from symbols
};

} // namespace openmsx

#endif
