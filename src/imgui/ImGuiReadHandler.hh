#ifndef IMGUI_READ_HANDLER_HH
#define IMGUI_READ_HANDLER_HH

#include "zstring_view.hh"
#include <string_view>

namespace openmsx {

struct ImGuiReadHandler
{
	virtual void loadStart() {}
	virtual void loadLine(std::string_view name, zstring_view value) = 0;
	virtual void loadEnd() {}
};

} // namespace openmsx

#endif
