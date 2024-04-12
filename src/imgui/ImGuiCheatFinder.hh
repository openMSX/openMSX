#ifndef IMGUI_CHEAT_FINDER_HH
#define IMGUI_CHEAT_FINDER_HH

#include "ImGuiPart.hh"

#include <cstdint>
#include <vector>

namespace openmsx {

class ImGuiCheatFinder final : public ImGuiPart
{
public:
	using ImGuiPart::ImGuiPart;

	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	struct SearchResult {
		uint16_t address;
		uint8_t oldValue;
		uint8_t newValue;
	};
	std::vector<SearchResult> searchResults;
	uint8_t searchValue = 0;
};

} // namespace openmsx

#endif
