#ifndef IMGUI_PART_HH
#define IMGUI_PART_HH

#include "zstring_view.hh"
#include <string_view>

struct ImGuiTextBuffer;

namespace openmsx {

class MSXMotherBoard;

struct ImGuiPart
{
	[[nodiscard]] virtual zstring_view iniName() const { return ""; }
	virtual void save(ImGuiTextBuffer& /*buf*/) {}
	virtual void loadStart() {}
	virtual void loadLine(std::string_view /*name*/, zstring_view /*value*/) {}
	virtual void loadEnd() {}

	virtual void showMenu(MSXMotherBoard* /*motherBoard*/) {}
	virtual void paint(MSXMotherBoard* /*motherBoard*/) {}
};

} // namespace openmsx

#endif
